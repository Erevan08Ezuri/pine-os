#include "core/Configuration.hpp"
#include "core/Logger.hpp"
#include "Version.hpp"
#include "ui/Font.hpp"
#include "ui/Shell.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>
#include <vector>

int main(int argc,char**argv){
  using namespace Pine;const auto workingRoot=std::filesystem::current_path();std::filesystem::path root=workingRoot,smokeOutput;for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg.rfind("--data=",0)==0)root=arg.substr(7);if(arg.rfind("--acceptance=",0)==0)smokeOutput=arg.substr(13);}
  Logger::instance().initialize(root/"data",LogLevel::Trace);Logger::instance().info("BOOT",std::string("Pine OS ")+Version+" starting");
  Configuration config(root/"data");Logger::instance().info("BOOT","Loading user configuration");config.load();
  if(!SDL_Init(SDL_INIT_VIDEO)){Logger::instance().error("BOOT",std::string("SDL initialization failed: ")+SDL_GetError());return 1;}
  const std::string title=std::string("Pine OS ")+Version+" - Desktop Simulator";SDL_Window*window=nullptr;SDL_Renderer*renderer=nullptr;SDL_Surface*acceptanceSurface=nullptr;
  if(smokeOutput.empty()){if(!SDL_CreateWindowAndRenderer(title.c_str(),720,1280,SDL_WINDOW_RESIZABLE,&window,&renderer)){Logger::instance().error("BOOT",SDL_GetError());SDL_Quit();return 1;}}
  else {acceptanceSurface=SDL_CreateSurface(720,1280,SDL_PIXELFORMAT_RGBA32);if(acceptanceSurface)renderer=SDL_CreateSoftwareRenderer(acceptanceSurface);if(!renderer){Logger::instance().error("BOOT",SDL_GetError());if(acceptanceSurface)SDL_DestroySurface(acceptanceSurface);SDL_Quit();return 1;}}
  auto fontRoot=workingRoot/"assets"/"fonts";if(!std::filesystem::exists(fontRoot/"Inter-Regular.ttf")){if(const char*base=SDL_GetBasePath())fontRoot=std::filesystem::path(base)/"assets"/"fonts";}
  if(!initializeFonts(fontRoot/"Inter-Regular.ttf",fontRoot/"Inter-SemiBold.ttf")){Logger::instance().error("BOOT","Bundled Inter fonts could not be loaded from "+fontRoot.string());SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);if(acceptanceSurface)SDL_DestroySurface(acceptanceSurface);SDL_Quit();return 1;}
  Logger::instance().info("BOOT","Professional Inter typography loaded");SDL_SetRenderLogicalPresentation(renderer,720,1280,SDL_LOGICAL_PRESENTATION_LETTERBOX);SDL_SetRenderVSync(renderer,1);Logger::instance().info("BOOT","Initializing platform and services");
  auto platform=createPlatform("desktop");Shell shell(window,renderer,config,std::move(platform));Logger::instance().info("BOOT","Shell ready");
  if(!smokeOutput.empty()){
    std::filesystem::create_directories(smokeOutput);bool passed=true;
    auto shot=[&](const std::string&name){shell.render();for(int i=0;i<14;++i){SDL_Delay(16);shell.update(.016);}if(!shell.captureFrame(smokeOutput/(name+".bmp"))){Logger::instance().error("UI",SDL_GetError());passed=false;}};
    shot("01-boot");shell.acceptanceSkipBoot();shot("02-home");passed&=shell.acceptanceLaunch("settings");shell.audio().setVolume(35);shell.network().setWifiEnabled(false);shell.persist();shot("03-settings");
    passed&=shell.acceptanceLaunch("bluetooth");shell.bluetooth().setEnabled(true);shell.bluetooth().startScan();passed&=shell.bluetooth().connect("pine-buds");shot("04-bluetooth");
    passed&=shell.acceptanceLaunch("camera");auto photo=shell.camera().capturePhoto();passed&=!photo.empty()&&std::filesystem::exists(photo);shot("05-camera");
    passed&=shell.acceptanceLaunch("files");shell.acceptancePath("DCIM");shot("06-files-dcim");
    passed&=shell.acceptanceLaunch("notes");shot("07-notes-empty");shell.textInput().setForceSoftwareKeyboard(true);shell.acceptanceBeginNote();shot("08-notes-keyboard-title");shell.acceptanceInput("Field Journal");shell.acceptanceKey("ENTER",1000);shell.acceptanceInput("First paragraph.");shell.acceptanceKey("ENTER",1100);shell.acceptanceKey("ENTER",1200);shell.acceptanceKey("SHIFT",1300);shell.acceptanceKey("s",1400);shell.acceptanceInput("econd paragraph with alpine signalx");shell.acceptanceKey("BKSP",1500);shell.acceptanceKey("123",1600);shell.acceptanceKey("!",1700);shell.acceptanceKey("ABC",1800);shell.acceptancePinNote();shot("09-notes-editor");shell.acceptanceDismissKeyboard();shell.acceptanceCloseNote();shell.acceptanceSearch("alpine");shot("10-notes-search");shell.acceptanceSearch("");const auto retainedNotes=shell.acceptanceNoteCount();shell.acceptanceBeginNote();shell.acceptanceInput("Delete Me");shell.acceptanceDismissKeyboard();shell.acceptanceShowDelete();shot("11-notes-delete-confirm");shell.acceptanceConfirmDelete();passed&=shell.acceptanceNoteCount()==retainedNotes;
    shell.battery().simulatePercentage(42);shell.battery().simulateCharging(true);shell.usb().setMode(UsbMode::FileTransfer);shell.camera().simulateAvailable(false);shell.acceptanceDeveloper(true);shot("12-developer");shell.persist();
    shutdownFonts();SDL_DestroyRenderer(renderer);SDL_DestroySurface(acceptanceSurface);SDL_Quit();Logger::instance().info("BOOT",passed?"Acceptance rendering passed":"Acceptance rendering failed");return passed?0:2;
  }
  std::uint64_t previous=SDL_GetTicks();while(shell.running()){SDL_Event event;while(SDL_PollEvent(&event))shell.handleEvent(event);auto now=SDL_GetTicks();shell.update((now-previous)/1000.0);previous=now;shell.render();}
  shutdownFonts();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();Logger::instance().info("BOOT","Pine OS stopped cleanly");return 0;
}
