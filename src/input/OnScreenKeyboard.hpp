#pragma once

#include "input/TextInputManager.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Pine {
class Shell;
enum class KeyboardLayout { Alphabet, Symbols };
enum class ShiftState { Inactive, OneShot, CapsLock };

class OnScreenKeyboard {
public:
  void render(Shell&, float displayWidth, float displayHeight);
  bool pointerDown(float x,float y,TextInputManager&,std::uint64_t nowMs);
  bool pointerUp(float x,float y,TextInputManager&,std::uint64_t nowMs);
  void update(std::uint64_t nowMs,TextInputManager&);
  void press(const std::string& token,TextInputManager&,std::uint64_t nowMs);
  void beginBackspace(TextInputManager&,std::uint64_t nowMs);
  void endBackspace();
  KeyboardLayout layout()const{return layout_;}
  ShiftState shiftState()const{return shift_;}
  std::string pressedToken()const{return pressed_;}
private:
  struct Hit {float x,y,w,h;std::string label,token;};
  std::vector<std::vector<std::pair<std::string,float>>> rows(const TextInputManager&)const;
  std::vector<Hit> hits_;
  KeyboardLayout layout_{KeyboardLayout::Alphabet};
  ShiftState shift_{ShiftState::Inactive};
  std::string pressed_;
  std::uint64_t lastShiftTap_{},backspaceStarted_{},nextRepeat_{};
  bool backspaceHeld_{};
  TextInputSession* repeatSession_{};
};
}
