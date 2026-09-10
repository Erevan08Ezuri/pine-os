#pragma once
namespace Pine { class UiContext { public: virtual ~UiContext()=default; virtual void renderSettings()=0; virtual void renderFiles()=0; virtual void renderCamera()=0; virtual void renderBluetooth()=0; virtual void renderNotes()=0; virtual void renderFinance()=0; }; }
