#pragma once
#include <string>
namespace Pine { class UiContext; class Application { public: virtual ~Application()=default; virtual std::string id()const=0; virtual std::string name()const=0; virtual void onOpen(){} virtual void onClose(){} virtual void update(double){} virtual void render(UiContext&)=0; }; }
