#include "input/TextInputSession.hpp"

#include <algorithm>
#include <cctype>

namespace Pine {
TextInputSession::TextInputSession(std::string& value, InputType type, Callback changed,
  Callback submitted, Callback blurred, std::size_t maxLength)
  :value_(value),type_(type),changed_(std::move(changed)),submitted_(std::move(submitted)),
   blurred_(std::move(blurred)),maxLength_(maxLength),cursor_(value.size()),anchor_(value.size()){}

void TextInputSession::setCursor(std::size_t position){cursor_=anchor_=std::min(position,value_.size());}
void TextInputSession::setSelection(std::size_t anchor,std::size_t cursor){anchor_=std::min(anchor,value_.size());cursor_=std::min(cursor,value_.size());}
void TextInputSession::selectAll(){anchor_=0;cursor_=value_.size();}
std::size_t TextInputSession::previousCodepoint(std::size_t p)const{if(p==0)return 0;--p;while(p>0&&(static_cast<unsigned char>(value_[p])&0xC0)==0x80)--p;return p;}
std::size_t TextInputSession::nextCodepoint(std::size_t p)const{if(p>=value_.size())return value_.size();++p;while(p<value_.size()&&(static_cast<unsigned char>(value_[p])&0xC0)==0x80)++p;return p;}

std::string TextInputSession::filtered(std::string_view input)const{
  std::string out;out.reserve(input.size());
  for(char c:input){
    if(type_!=InputType::Multiline&&(c=='\n'||c=='\r'))continue;
    if(type_==InputType::Number&&!(std::isdigit(static_cast<unsigned char>(c))||c=='-'||c=='.'))continue;
    if(c=='\r')c='\n';
    out.push_back(c);
  }
  return out;
}
void TextInputSession::replaceSelection(std::string_view input){
  auto text=filtered(input);const auto first=std::min(cursor_,anchor_),last=std::max(cursor_,anchor_);
  const auto available=maxLength_-(value_.size()-(last-first));if(text.size()>available)text.resize(available);
  if(first==last&&text.empty())return;
  value_.replace(first,last-first,text);
  cursor_=anchor_=first+text.size();
  if(changed_)changed_();
}
void TextInputSession::insertText(std::string_view text){replaceSelection(text);}
void TextInputSession::deleteBackward(){
  if(hasSelection()){replaceSelection("");return;}if(cursor_==0)return;const auto prior=previousCodepoint(cursor_);value_.erase(prior,cursor_-prior);cursor_=anchor_=prior;if(changed_)changed_();
}
void TextInputSession::moveLeft(bool selecting){const auto p=previousCodepoint(cursor_);cursor_=p;if(!selecting)anchor_=cursor_;}
void TextInputSession::moveRight(bool selecting){cursor_=nextCodepoint(cursor_);if(!selecting)anchor_=cursor_;}
void TextInputSession::submit(){if(type_==InputType::Multiline)insertText("\n");else if(submitted_)submitted_();}
void TextInputSession::notifyBlurred(){if(blurred_)blurred_();}
}
