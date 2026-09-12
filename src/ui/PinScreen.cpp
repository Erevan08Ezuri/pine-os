#include "ui/Shell.hpp"
#include "ui/Font.hpp"
namespace Pine {
void Shell::lockDevice(){
    cancelPrompts();commitNote();camera_.stop();developerOpen_=false;wifiScreen_=false;apps_.home();
    financeSecurity_.lock();deviceLock_.lock();pinChanging_=false;pinMode_=PinMode::Unlock;
    pinInput_.clear();pinCurrent_.clear();pinNew_.clear();pinMessage_.clear();
    click_=pointerDown_=pointerDragged_=false;toast_.clear();
}
void Shell::beginPinChange(){
    if(deviceLock_.locked()||deviceLock_.busy())return;
    cancelPrompts();pinChanging_=true;pinMode_=PinMode::Current;
    pinInput_.clear();pinCurrent_.clear();pinNew_.clear();pinMessage_.clear();
}
void Shell::pinKey(const std::string& key){
    if(deviceLock_.busy()||!deviceLock_.ready()||deviceLock_.cooldownSeconds(SDL_GetTicks()))return;
    if(key=="DELETE"){if(!pinInput_.empty())pinInput_.pop_back();return;}
    if(key=="ENTER"){submitPin();return;}
    if(key.size()==1&&key[0]>='0'&&key[0]<='9'&&pinInput_.size()<6){
        pinInput_+=key;pinMessage_.clear();
    }
}
void Shell::submitPin(){
    if(!DeviceLockService::validPin(pinInput_)){pinMessage_="Enter six digits";return;}
    if(!pinChanging_){
        deviceLock_.unlock(std::move(pinInput_),SDL_GetTicks());pinInput_.clear();return;
    }
    switch(pinMode_){
    case PinMode::Current:pinCurrent_=std::move(pinInput_);pinMode_=PinMode::New;break;
    case PinMode::New:pinNew_=std::move(pinInput_);pinMode_=PinMode::Confirm;break;
    case PinMode::Confirm:
        if(pinInput_!=pinNew_){pinMessage_="PINs did not match - try again";pinNew_.clear();pinMode_=PinMode::New;}
        else{
            pinChangeRevision_=deviceLock_.revision();
            if(deviceLock_.changePin(std::move(pinCurrent_),std::move(pinNew_),SDL_GetTicks()))pinMode_=PinMode::Saving;
            else{pinMode_=PinMode::Current;pinMessage_=deviceLock_.status();}
            pinCurrent_.clear();pinNew_.clear();
        }
        break;
    default:break;
    }
    pinInput_.clear();
}
void Shell::renderPinScreen(){
    renderingPin_=true;
    label(360-textWidth("PINE",9)/2,185,"PINE",9,Theme::Gold);
    const std::string title=!pinChanging_?"ENTER DEVICE PIN":pinMode_==PinMode::Current?"CURRENT PIN":
        pinMode_==PinMode::New?"NEW SIX-DIGIT PIN":pinMode_==PinMode::Confirm?"CONFIRM NEW PIN":"SAVING PIN";
    label(360-textWidth(title,3.2f)/2,290,title,3.2f);
    for(std::size_t i=0;i<6;++i){panel({155+static_cast<float>(i)*70,355,55,65});if(i<pinInput_.size())label(174+i*70,376,"*",4,Theme::Gold);}
    const auto cooldown=deviceLock_.cooldownSeconds(SDL_GetTicks());
    const auto message=cooldown?"Try again in "+std::to_string(cooldown)+" seconds":
        !pinMessage_.empty()?pinMessage_:deviceLock_.busy()||!deviceLock_.ready()||!pinChanging_?deviceLock_.status():"Use the keypad below";
    sublabel(85,440,message);
    for(int i=0;i<9;++i){
        const auto digit=std::to_string(i+1);
        const Rect key{70+static_cast<float>(i%3)*200,500+static_cast<float>(i/3)*120,180,100};
        if(button(key,""))pinKey(digit);
        label(key.x+(key.w-textWidth(digit,5))/2,key.y+32,digit,5);
    }
    if(button({70,860,180,100},"DELETE"))pinKey("DELETE");
    if(button({270,860,180,100},""))pinKey("0");
    label(360-textWidth("0",5)/2,892,"0",5);
    if(button({470,860,180,100},pinChanging_?"NEXT":"UNLOCK",true))pinKey("ENTER");
    if(pinChanging_&&!deviceLock_.busy()&&button({245,1030,230,60},"CANCEL")){
        pinChanging_=false;pinMode_=PinMode::Unlock;pinInput_.clear();pinCurrent_.clear();pinNew_.clear();pinMessage_.clear();
    }
    renderingPin_=false;
}
}
