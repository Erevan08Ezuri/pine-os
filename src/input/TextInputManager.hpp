#pragma once

#include "input/TextInputSession.hpp"
#include <cstdint>
#include <string_view>

namespace Pine {
class TextInputManager {
public:
  void focus(TextInputSession&, std::uint64_t nowMs, bool requestSoftwareKeyboard = true);
  void blur();
  TextInputSession* focused() const { return focused_; }
  bool isFocused(const TextInputSession& session) const { return focused_ == &session; }
  void handlePhysicalText(std::string_view, std::uint64_t nowMs);
  void physicalBackspace(std::uint64_t nowMs);
  void physicalEnter(std::uint64_t nowMs);
  void physicalLeft(std::uint64_t nowMs, bool selecting = false);
  void physicalRight(std::uint64_t nowMs, bool selecting = false);
  void insertFromKeyboard(std::string_view);
  void deleteFromKeyboard();
  void enterFromKeyboard();
  void update(double seconds);
  void showKeyboard();
  void hideKeyboard(bool clearFocus = true);
  bool keyboardVisible() const { return animation_ > 0.001f; }
  bool keyboardTargetVisible() const { return targetVisible_; }
  float keyboardProgress() const { return animation_; }
  float keyboardHeight() const { return keyboardHeight_; }
  float safeContentBottom(float displayHeight) const { return displayHeight-keyboardHeight_*animation_; }
  void setForceSoftwareKeyboard(bool value){forceSoftware_=value;if(value&&focused_)targetVisible_=true;}
  bool forceSoftwareKeyboard()const{return forceSoftware_;}
  bool physicalKeyboardActive(std::uint64_t nowMs)const{return lastPhysicalMs_&&nowMs-lastPhysicalMs_<5000;}
private:
  void notePhysical(std::uint64_t nowMs);
  TextInputSession* focused_{};
  bool targetVisible_{false},forceSoftware_{false};
  float animation_{},keyboardHeight_{420.0f};
  std::uint64_t lastPhysicalMs_{};
};
}
