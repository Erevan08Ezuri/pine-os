#pragma once
#include "platform/Platform.hpp"
namespace Pine {class DesktopAudio final:public AudioBackend{public:int volume()const override{return volume_;}void setVolume(int)override;bool muted()const override{return muted_;}void setMuted(bool v)override{muted_=v;}private:int volume_{75};bool muted_{false};};}
