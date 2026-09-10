#include "input/TextInputManager.hpp"
#include <algorithm>

namespace Pine {
void TextInputManager::focus(TextInputSession&session,std::uint64_t now,bool requestSoftware){if(focused_!=&session){if(focused_)focused_->notifyBlurred();focused_=&session;}targetVisible_=requestSoftware&&(forceSoftware_||!physicalKeyboardActive(now));}
void TextInputManager::blur(){if(focused_)focused_->notifyBlurred();focused_=nullptr;targetVisible_=false;}
void TextInputManager::notePhysical(std::uint64_t now){lastPhysicalMs_=now;if(!forceSoftware_)targetVisible_=false;}
void TextInputManager::handlePhysicalText(std::string_view text,std::uint64_t now){notePhysical(now);if(focused_)focused_->insertText(text);}
void TextInputManager::physicalBackspace(std::uint64_t now){notePhysical(now);if(focused_)focused_->deleteBackward();}
void TextInputManager::physicalEnter(std::uint64_t now){notePhysical(now);if(focused_)focused_->submit();}
void TextInputManager::physicalLeft(std::uint64_t now,bool selecting){notePhysical(now);if(focused_)focused_->moveLeft(selecting);}
void TextInputManager::physicalRight(std::uint64_t now,bool selecting){notePhysical(now);if(focused_)focused_->moveRight(selecting);}
void TextInputManager::insertFromKeyboard(std::string_view text){if(focused_)focused_->insertText(text);}
void TextInputManager::deleteFromKeyboard(){if(focused_)focused_->deleteBackward();}
void TextInputManager::enterFromKeyboard(){if(focused_)focused_->submit();}
void TextInputManager::update(double seconds){const float step=static_cast<float>(seconds/0.20);animation_=std::clamp(animation_+(targetVisible_?step:-step),0.0f,1.0f);}
void TextInputManager::showKeyboard(){if(focused_)targetVisible_=true;}
void TextInputManager::hideKeyboard(bool clearFocus){targetVisible_=false;if(clearFocus&&focused_){focused_->notifyBlurred();focused_=nullptr;}}
}
