#pragma once
#include "core/Application.hpp"
namespace Pine {
class SettingsApp final:public Application{public:std::string id()const override{return"settings";}std::string name()const override{return"Settings";}void render(UiContext&)override;};
class FilesApp final:public Application{public:std::string id()const override{return"files";}std::string name()const override{return"Files";}void render(UiContext&)override;};
class CameraApp final:public Application{public:std::string id()const override{return"camera";}std::string name()const override{return"Camera";}void render(UiContext&)override;};
class BluetoothApp final:public Application{public:std::string id()const override{return"bluetooth";}std::string name()const override{return"Bluetooth";}void render(UiContext&)override;};
class NotesApp final:public Application{public:std::string id()const override{return"notes";}std::string name()const override{return"Notes";}void render(UiContext&)override;};
}
