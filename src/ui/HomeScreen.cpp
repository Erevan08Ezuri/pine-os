#include "ui/Shell.hpp"
#include "ui/Font.hpp"
#include <algorithm>
namespace Pine {void renderHomeScreen(Shell&s){s.label(55,110,"PINE OS",2.2f,Theme::Gold);s.label(55,155,"WHERE DO WE GO?",5,Theme::Ink);s.sublabel(58,215,"PRIVATE. LOCAL. BUILT TO TRAVEL.");auto apps=s.apps().applications();std::ranges::sort(apps,[](auto*a,auto*b){return a->name()<b->name();});for(size_t i=0;i<apps.size();++i){float x=55+(i%2)*315,y=285+(i/2)*220;Rect tile{x,y,280,180};s.panel(tile);s.label(x+25,y+28,apps[i]->name(),4);s.sublabel(x+25,y+78,"PINE APPLICATION");if(s.button({x+25,y+118,230,48},"OPEN",true))s.launchApp(apps[i]->id());}}}
