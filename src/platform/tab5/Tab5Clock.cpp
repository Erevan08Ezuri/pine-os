#include "platform/tab5/Tab5Clock.hpp"
#include "driver/i2c_master.h"
#include "bsp/m5stack_tab5.h"
#include <chrono>
#include <ctime>
#include <sys/time.h>
namespace Pine {
namespace {
i2c_master_dev_handle_t rtc{};
bool read(unsigned char reg,unsigned char* data,size_t n){return rtc&&i2c_master_transmit_receive(rtc,&reg,1,data,n,100)==ESP_OK;}
bool write(unsigned char reg,unsigned char value){
    unsigned char data[]={reg,value};return rtc&&i2c_master_transmit(rtc,data,sizeof(data),100)==ESP_OK;
}
int decimal(unsigned char b){return(b>>4)*10+(b&15);}
unsigned char bcd(int v){return((v/10)<<4)|(v%10);}
}
bool tab5ClockValid(){return std::time(nullptr)>=1735689600;}
void initializeTab5Clock(){
    auto bus=bsp_i2c_get_handle();if(!bus)return;
    i2c_device_config_t cfg{};cfg.dev_addr_length=I2C_ADDR_BIT_LEN_7;cfg.device_address=0x32;cfg.scl_speed_hz=400000;
    if(i2c_master_bus_add_device(bus,&cfg,&rtc)!=ESP_OK)return;
    unsigned char flags{},control{},data[7]{};
    if(!read(0x1d,&flags,1)||(flags&2)||!read(0x1e,&control,1)||(control&0x40)||!read(0x10,data,7))return;
    int year=2000+decimal(data[6]),month=decimal(data[5]&31),day=decimal(data[4]&63);
    auto date=std::chrono::year(year)/month/day;
    int hour=decimal(data[2]&63),minute=decimal(data[1]&127),second=decimal(data[0]&127);
    if(!date.ok()||year<2025||hour>23||minute>59||second>59)return;
    auto seconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::sys_days(date).time_since_epoch()).count()+hour*3600+minute*60+second;
    timeval now{static_cast<time_t>(seconds),0};settimeofday(&now,nullptr);
}
void persistTab5Clock(){
    if(!rtc||!tab5ClockValid())return;
    auto now=std::time(nullptr);std::tm t{};gmtime_r(&now,&t);
    unsigned char control{},flags{};
    if(!read(0x1e,&control,1)||!write(0x1e,control|0x40))return;
    unsigned char data[]={0x10,bcd(t.tm_sec),bcd(t.tm_min),bcd(t.tm_hour),
        static_cast<unsigned char>(1<<t.tm_wday),bcd(t.tm_mday),bcd(t.tm_mon+1),bcd(t.tm_year-100)};
    bool saved=i2c_master_transmit(rtc,data,sizeof(data),100)==ESP_OK;
    bool restarted=write(0x1e,control&~0x40);
    if(saved&&restarted&&read(0x1d,&flags,1))write(0x1d,flags&~2);
}
}
