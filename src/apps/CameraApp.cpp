#include "apps/Apps.hpp"
#include "core/UiContext.hpp"
#include "ui/Shell.hpp"
#include <cmath>
namespace Pine {void CameraApp::render(UiContext&ui){ui.renderCamera();}
void Shell::renderCamera(){
  label(45,105,"CAMERA",6,Theme::Gold);sublabel(48,165,camera_.available()?"DESKTOP TEST CAMERA":"NO CAMERA DETECTED");Rect preview{55,220,610,750};panel(preview);
  if(camera_.available()){camera_.start();const auto t=SDL_GetTicks()/30;for(int y=0;y<15;++y)for(int x=0;x<12;++x){SDL_SetRenderDrawColor(renderer_,static_cast<Uint8>(18+(x*7+t)%58),static_cast<Uint8>(17+y*3),static_cast<Uint8>(13+(x+y)*3),255);SDL_FRect cell{preview.x+x*preview.w/12,preview.y+y*preview.h/15,preview.w/12+1,preview.h/15+1};SDL_RenderFillRect(renderer_,&cell);}label(155,555,"PINE CAMERA PREVIEW",3,Theme::Ink);}
  else {label(185,560,"NO CAMERA DETECTED",3.5f);sublabel(195,610,"ENABLE IT IN DEVELOPER PANEL");}
  if(button({250,1000,220,70},"CAPTURE",camera_.available())){auto path=camera_.capturePhoto();toast(path.empty()?camera_.lastStatus():"PHOTO SAVED TO DCIM");}
  sublabel(150,1100,camera_.lastStatus());
}}
