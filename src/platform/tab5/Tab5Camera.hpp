#pragma once
#include "platform/Platform.hpp"
#include <array>
#include <cstddef>
namespace Pine {
class Tab5Camera final:public CameraBackend{
public:
    Tab5Camera()=default;
    ~Tab5Camera() override;
    bool available()const override{return !failed_;}
    bool start()override;
    void stop()override;
    bool capturePhoto(const std::filesystem::path&)override;
    void setAvailable(bool value)override{if(!value)stop();}
    bool previewFrame(CameraFrame&)override;
private:
    struct Buffer{void* data{};std::size_t length{};};
    bool initialize();
    bool queueBuffers();
    void release();
    bool writeBmp(const std::filesystem::path&,const CameraFrame&);
    int fd_{-1};
    std::array<Buffer,2> buffers_{};
    unsigned bufferCount_{};
    int width_{1280},height_{720};
    bool initialized_{},running_{},failed_{};
};
}
