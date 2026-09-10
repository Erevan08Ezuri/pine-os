#pragma once
#include "platform/Platform.hpp"
namespace Pine {class AudioService{public:explicit AudioService(AudioBackend&b):backend_(b){}int volume()const{return backend_.volume();}void setVolume(int v){backend_.setVolume(v);}bool muted()const{return backend_.muted();}void setMuted(bool v){backend_.setMuted(v);}private:AudioBackend&backend_;};}
