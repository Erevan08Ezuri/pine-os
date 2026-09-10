#include "ui/Shell.hpp"
#include "ui/Font.hpp"
#include <chrono>
#include <format>
namespace Pine {void renderStatusBar(Shell&s){SDL_SetRenderDrawColor(s.renderer(),Theme::Background.r,Theme::Background.g,Theme::Background.b,255);SDL_FRect bar{0,0,720,72};SDL_RenderFillRect(s.renderer(),&bar);SDL_SetRenderDrawColor(s.renderer(),Theme::Border.r,Theme::Border.g,Theme::Border.b,255);SDL_RenderLine(s.renderer(),0,71,720,71);auto now=std::chrono::system_clock::now();s.label(28,25,std::format("{:%H:%M}",now),3);s.label(310,25,"PINE",3,Theme::Gold);std::string status=(s.network().connected()?"WIFI  ":"OFF  ")+(s.bluetooth().enabled()?std::string("BT  "):std::string())+(s.battery().isCharging()?"+":"")+std::to_string(s.battery().percentage())+"%";s.label(690-textWidth(status,2.2f),27,status,2.2f);}}
