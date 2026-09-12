#include "platform/tab5/Tab5Camera.hpp"
#include "bsp/m5stack_tab5.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "linux/videodev2.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <system_error>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

namespace Pine {
namespace {
constexpr const char* Tag="PineCamera";
constexpr int PreviewWidth=320;
constexpr int PreviewHeight=180;

bool startCameraClock(){
    // The M5Stack helper uses LEDC timer 0, which is also the LCD backlight
    // PWM timer. Give the camera its own timer so opening Camera cannot break
    // brightness control.
    ledc_timer_config_t timer{};
    timer.speed_mode=LEDC_LOW_SPEED_MODE;
    timer.duty_resolution=LEDC_TIMER_1_BIT;
    timer.timer_num=LEDC_TIMER_1;
    timer.freq_hz=24000000;
    timer.clk_cfg=LEDC_AUTO_CLK;
    if(ledc_timer_config(&timer)!=ESP_OK)return false;
    ledc_channel_config_t channel{};
    channel.gpio_num=36;
    channel.speed_mode=LEDC_LOW_SPEED_MODE;
    channel.channel=LEDC_CHANNEL_0;
    channel.intr_type=LEDC_INTR_DISABLE;
    channel.timer_sel=LEDC_TIMER_1;
    channel.duty=1;
    channel.hpoint=0;
    channel.sleep_mode=LEDC_SLEEP_MODE_KEEP_ALIVE;
    return ledc_channel_config(&channel)==ESP_OK;
}
}

Tab5Camera::~Tab5Camera(){release();}

bool Tab5Camera::initialize(){
    if(initialized_)return true;
    if(failed_)return false;
    if(!startCameraClock()){
        ESP_LOGE(Tag,"Unable to start 24 MHz camera clock");
        failed_=true;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    static esp_video_init_csi_config_t csi{};
    csi.sccb_config.init_sccb=false;
    csi.sccb_config.i2c_handle=bsp_i2c_get_handle();
    csi.sccb_config.freq=400000;
    csi.reset_pin=-1;
    csi.pwdn_pin=-1;
    esp_video_init_config_t video{};
    video.csi=&csi;
    esp_err_t init=esp_video_init(&video);
    if(init!=ESP_OK){
        ESP_LOGE(Tag,"esp_video_init failed: %s",esp_err_to_name(init));
        failed_=true;
        return false;
    }

    fd_=open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME,O_RDONLY);
    if(fd_<0){
        ESP_LOGE(Tag,"Unable to open %s: errno %d",ESP_VIDEO_MIPI_CSI_DEVICE_NAME,errno);
        failed_=true;
        return false;
    }

    v4l2_format format{};
    format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd_,VIDIOC_G_FMT,&format)!=0){
        ESP_LOGE(Tag,"VIDIOC_G_FMT failed: errno %d",errno);
        release();failed_=true;return false;
    }
    format.fmt.pix.pixelformat=V4L2_PIX_FMT_RGB565;
    if(ioctl(fd_,VIDIOC_S_FMT,&format)!=0){
        ESP_LOGE(Tag,"Camera cannot output RGB565: errno %d",errno);
        release();failed_=true;return false;
    }
    if(ioctl(fd_,VIDIOC_G_FMT,&format)!=0){
        release();failed_=true;return false;
    }
    width_=static_cast<int>(format.fmt.pix.width);
    height_=static_cast<int>(format.fmt.pix.height);
    if(width_<=0||height_<=0){release();failed_=true;return false;}

    v4l2_requestbuffers request{};
    request.count=static_cast<decltype(request.count)>(buffers_.size());
    request.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory=V4L2_MEMORY_MMAP;
    if(ioctl(fd_,VIDIOC_REQBUFS,&request)!=0||request.count==0){
        ESP_LOGE(Tag,"VIDIOC_REQBUFS failed: errno %d",errno);
        release();failed_=true;return false;
    }
    bufferCount_=std::min<unsigned>(request.count,buffers_.size());
    for(unsigned i=0;i<bufferCount_;++i){
        v4l2_buffer buffer{};
        buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory=V4L2_MEMORY_MMAP;
        buffer.index=i;
        if(ioctl(fd_,VIDIOC_QUERYBUF,&buffer)!=0){release();failed_=true;return false;}
        void* mapped=mmap(nullptr,buffer.length,PROT_READ|PROT_WRITE,MAP_SHARED,fd_,buffer.m.offset);
        if(mapped==MAP_FAILED){release();failed_=true;return false;}
        buffers_[i]={mapped,buffer.length};
    }
    const int flags=fcntl(fd_,F_GETFL,0);
    if(flags>=0)fcntl(fd_,F_SETFL,flags|O_NONBLOCK);
    initialized_=true;
    ESP_LOGI(Tag,"Camera initialized: %dx%d RGB565 with %u buffers",width_,height_,bufferCount_);
    return true;
}

bool Tab5Camera::queueBuffers(){
    for(unsigned i=0;i<bufferCount_;++i){
        v4l2_buffer buffer{};
        buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory=V4L2_MEMORY_MMAP;
        buffer.index=i;
        if(ioctl(fd_,VIDIOC_QBUF,&buffer)!=0){
            ESP_LOGE(Tag,"VIDIOC_QBUF failed: errno %d",errno);
            return false;
        }
    }
    return true;
}

bool Tab5Camera::start(){
    if(running_)return true;
    if(!initialize()||!queueBuffers())return false;
    int type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd_,VIDIOC_STREAMON,&type)!=0){
        ESP_LOGE(Tag,"VIDIOC_STREAMON failed: errno %d",errno);
        return false;
    }
    running_=true;
    return true;
}

void Tab5Camera::stop(){
    if(!running_||fd_<0)return;
    int type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd_,VIDIOC_STREAMOFF,&type);
    running_=false;
}

bool Tab5Camera::previewFrame(CameraFrame& frame){
    if(!start())return false;
    v4l2_buffer buffer{};
    buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory=V4L2_MEMORY_MMAP;
    if(ioctl(fd_,VIDIOC_DQBUF,&buffer)!=0){
        if(errno!=EAGAIN)ESP_LOGW(Tag,"VIDIOC_DQBUF failed: errno %d",errno);
        return false;
    }
    bool ok=false;
    if(buffer.index<bufferCount_&&buffers_[buffer.index].data){
        const auto* source=static_cast<const std::uint16_t*>(buffers_[buffer.index].data);
        frame.width=PreviewWidth;
        frame.height=PreviewHeight;
        frame.pixels.resize(static_cast<std::size_t>(PreviewWidth)*PreviewHeight);
        // The front-facing camera is mirrored to behave like a normal selfie
        // preview. Downsample 1280x720 to 320x180 to keep UI bandwidth modest.
        for(int y=0;y<PreviewHeight;++y){
            const int sy=std::min(height_-1,y*height_/PreviewHeight);
            for(int x=0;x<PreviewWidth;++x){
                const int sx=std::min(width_-1,(PreviewWidth-1-x)*width_/PreviewWidth);
                frame.pixels[static_cast<std::size_t>(y)*PreviewWidth+x]=source[static_cast<std::size_t>(sy)*width_+sx];
            }
        }
        ok=true;
    }
    if(ioctl(fd_,VIDIOC_QBUF,&buffer)!=0){
        ESP_LOGE(Tag,"Unable to return camera buffer: errno %d",errno);
        return false;
    }
    return ok;
}

bool Tab5Camera::writeBmp(const std::filesystem::path& path,const CameraFrame& frame){
    if(frame.width<=0||frame.height<=0||frame.pixels.size()<static_cast<std::size_t>(frame.width)*frame.height)return false;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(),ec);
    FILE* file=fopen(path.string().c_str(),"wb");
    if(!file)return false;
    const std::uint32_t rowBytes=(static_cast<std::uint32_t>(frame.width)*3u+3u)&~3u;
    const std::uint32_t imageBytes=rowBytes*static_cast<std::uint32_t>(frame.height);
    const std::uint32_t fileBytes=54u+imageBytes;
    unsigned char header[54]{};
    auto put16=[&](int offset,std::uint16_t value){header[offset]=value&0xff;header[offset+1]=(value>>8)&0xff;};
    auto put32=[&](int offset,std::uint32_t value){for(int i=0;i<4;++i)header[offset+i]=(value>>(i*8))&0xff;};
    header[0]='B';header[1]='M';put32(2,fileBytes);put32(10,54);put32(14,40);put32(18,frame.width);put32(22,frame.height);put16(26,1);put16(28,24);put32(34,imageBytes);
    if(fwrite(header,1,sizeof(header),file)!=sizeof(header)){fclose(file);return false;}
    std::vector<unsigned char> row(rowBytes);
    for(int y=frame.height-1;y>=0;--y){
        std::fill(row.begin(),row.end(),0);
        for(int x=0;x<frame.width;++x){
            const std::uint16_t pixel=frame.pixels[static_cast<std::size_t>(y)*frame.width+x];
            const unsigned char r=static_cast<unsigned char>(((pixel>>11)&31)*255/31);
            const unsigned char g=static_cast<unsigned char>(((pixel>>5)&63)*255/63);
            const unsigned char b=static_cast<unsigned char>((pixel&31)*255/31);
            row[x*3]=b;row[x*3+1]=g;row[x*3+2]=r;
        }
        if(fwrite(row.data(),1,row.size(),file)!=row.size()){fclose(file);return false;}
    }
    const bool ok=fclose(file)==0;
    return ok;
}

bool Tab5Camera::capturePhoto(const std::filesystem::path& path){
    if(!start())return false;
    CameraFrame frame;
    for(int attempt=0;attempt<8;++attempt){
        if(previewFrame(frame))return writeBmp(path,frame);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

void Tab5Camera::release(){
    stop();
    for(auto& buffer:buffers_){
        if(buffer.data&&buffer.data!=MAP_FAILED)munmap(buffer.data,buffer.length);
        buffer={};
    }
    bufferCount_=0;
    if(fd_>=0){close(fd_);fd_=-1;}
    initialized_=false;
}
}
