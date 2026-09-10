#include "input/OnScreenKeyboard.hpp"
#include "ui/Font.hpp"
#include "ui/Shell.hpp"
#include "ui/Theme.hpp"

#include <algorithm>
#include <cctype>

namespace Pine {
std::vector<std::vector<std::pair<std::string,float>>> OnScreenKeyboard::rows(const TextInputManager&manager)const{
  const bool numberOnly=manager.focused()&&manager.focused()->type()==InputType::Number;
  if(layout_==KeyboardLayout::Symbols||numberOnly){
    return {{{"1",1.0f},{"2",1.0f},{"3",1.0f},{"4",1.0f},{"5",1.0f},{"6",1.0f},{"7",1.0f},{"8",1.0f},{"9",1.0f},{"0",1.0f}},
            {{"-",1.0f},{"/",1.0f},{":",1.0f},{";",1.0f},{"(",1.0f},{")",1.0f},{"$",1.0f},{"&",1.0f},{"@",1.0f},{"\"",1.0f}},
            {{".",1.0f},{",",1.0f},{"?",1.0f},{"!",1.0f},{"'",1.0f},{"#",1.0f},{"%",1.0f},{"+",1.0f},{"BKSP",1.5f}},
            {{numberOnly?"123":"ABC",1.35f},{",",.75f},{"SPACE",4.0f},{".",.75f},{"HIDE",1.25f},{"ENTER",1.4f}}};
  }
  const bool upper=shift_!=ShiftState::Inactive;
  auto makeRow=[&](const std::string&letters){std::vector<std::pair<std::string,float>>row;for(char c:letters){std::string v(1,upper?static_cast<char>(std::toupper(static_cast<unsigned char>(c))):c);row.push_back({v,1.0f});}return row;};
  auto third=makeRow("zxcvbnm");third.insert(third.begin(),{shift_==ShiftState::CapsLock?"CAPS":shift_==ShiftState::OneShot?"SHIFT 1X":"SHIFT",1.55f});third.push_back({"BKSP",1.55f});
  return {makeRow("qwertyuiop"),makeRow("asdfghjkl"),std::move(third),{{"123",1.35f},{",",.75f},{"SPACE",4.0f},{".",.75f},{"HIDE",1.25f},{"ENTER",1.4f}}};
}

void OnScreenKeyboard::render(Shell&s,float width,float height){
  auto&manager=s.textInput();if(!manager.keyboardVisible()){hits_.clear();return;}
  const float keyboardHeight=manager.keyboardHeight(),top=height-keyboardHeight*manager.keyboardProgress();
  s.coloredPanel({0,top,width,keyboardHeight},Theme::Surface,Theme::Border,0);
  hits_.clear();const auto layoutRows=rows(manager);const float margin=10,gap=6,rowHeight=88;float y=top+12;
  for(const auto&row:layoutRows){float totalWeight=0;for(const auto&key:row)totalWeight+=key.second;const float unit=(width-margin*2-gap*(row.size()-1))/totalWeight;float x=margin;
    for(const auto&key:row){const float w=unit*key.second;std::string token=key.first,labelText=key.first;if(token=="SHIFT 1X"||token=="CAPS")token="SHIFT";if(token=="ENTER"){if(manager.focused()){switch(manager.focused()->type()){case InputType::Search:labelText="SEARCH";break;case InputType::Text:labelText="NEXT";break;case InputType::Number:labelText="DONE";break;default:labelText="ENTER";break;}}}
      const bool active=pressed_==token||(token=="SHIFT"&&shift_!=ShiftState::Inactive)||(token=="123"&&layout_==KeyboardLayout::Symbols);
      s.coloredPanel({x,y,w,rowHeight},active?Theme::GoldSoft:Theme::SurfaceRaised,active?Theme::Gold:Theme::Border,8);
      const float scale=labelText.size()>5?1.65f:2.25f;const auto tw=textWidth(labelText,scale);s.label(x+(w-tw)/2,y+(rowHeight-18)/2,labelText,scale,active?Theme::OnGold:Theme::Ink);
      hits_.push_back({x,y,w,rowHeight,labelText,token});x+=w+gap;
    }y+=rowHeight+6;
  }
}

bool OnScreenKeyboard::pointerDown(float x,float y,TextInputManager&manager,std::uint64_t now){for(const auto&key:hits_)if(x>=key.x&&x<=key.x+key.w&&y>=key.y&&y<=key.y+key.h){pressed_=key.token;if(key.token=="BKSP")beginBackspace(manager,now);return true;}return false;}
bool OnScreenKeyboard::pointerUp(float,float,TextInputManager&manager,std::uint64_t now){if(pressed_.empty())return false;const auto token=pressed_;if(token!="BKSP")press(token,manager,now);endBackspace();pressed_.clear();return true;}
void OnScreenKeyboard::beginBackspace(TextInputManager&manager,std::uint64_t now){manager.deleteFromKeyboard();backspaceHeld_=true;backspaceStarted_=now;nextRepeat_=now+450;}
void OnScreenKeyboard::endBackspace(){backspaceHeld_=false;backspaceStarted_=nextRepeat_=0;}
void OnScreenKeyboard::update(std::uint64_t now,TextInputManager&manager){if(backspaceHeld_&&now>=nextRepeat_){manager.deleteFromKeyboard();nextRepeat_=now+(now-backspaceStarted_>1500?45:80);}}

void OnScreenKeyboard::press(const std::string&token,TextInputManager&manager,std::uint64_t now){
  if(token=="SHIFT"){if(shift_==ShiftState::CapsLock)shift_=ShiftState::Inactive;else if(shift_==ShiftState::OneShot&&lastShiftTap_&&now-lastShiftTap_<=350)shift_=ShiftState::CapsLock;else shift_=ShiftState::OneShot;lastShiftTap_=now;return;}
  if(token=="123"){layout_=KeyboardLayout::Symbols;return;}if(token=="ABC"){layout_=KeyboardLayout::Alphabet;return;}if(token=="HIDE"){manager.hideKeyboard(true);return;}if(token=="ENTER"){manager.enterFromKeyboard();return;}if(token=="BKSP"){manager.deleteFromKeyboard();return;}
  std::string text=token=="SPACE"?" ":token;if(layout_==KeyboardLayout::Alphabet&&text.size()==1&&std::isalpha(static_cast<unsigned char>(text[0]))){text[0]=shift_==ShiftState::Inactive?static_cast<char>(std::tolower(static_cast<unsigned char>(text[0]))):static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));if(shift_==ShiftState::OneShot)shift_=ShiftState::Inactive;}
  manager.insertFromKeyboard(text);
}
}
