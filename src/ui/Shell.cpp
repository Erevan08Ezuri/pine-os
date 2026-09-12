#include "ui/Shell.hpp"
#include "finance/BankClient.hpp"
#include "apps/Apps.hpp"
#include "core/Logger.hpp"
#include "ui/Font.hpp"
#include "ui/Theme.hpp"
#include <algorithm>
#ifdef PINE_TAB5
#include "platform/tab5/Tab5Clock.hpp"
#endif
#include <chrono>
#include <cmath>
#include <format>

namespace Pine {
namespace {
void roundedFill(SDL_Renderer* renderer, Rect rect, float radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a);
  const int top=static_cast<int>(rect.y),bottom=static_cast<int>(rect.y+rect.h);
  for(int row=top;row<bottom;++row){
    const float fromEdge=std::min(static_cast<float>(row)-rect.y,rect.y+rect.h-1-static_cast<float>(row));
    float inset=0;
    if(fromEdge<radius){const float dy=radius-fromEdge-1;inset=radius-std::sqrt(std::max(0.0f,radius*radius-dy*dy));}
    SDL_RenderLine(renderer,rect.x+inset,static_cast<float>(row),rect.x+rect.w-inset-1,static_cast<float>(row));
  }
}
}
Shell::Shell(SDL_Window*w,SDL_Renderer*r,Configuration&c,std::unique_ptr<Platform>p):deviceLock_(c.dataRoot()),window_(w),renderer_(r),config_(c),platform_(std::move(p)),battery_(platform_->battery()),camera_(platform_->camera(),c.storageRoot()),bluetooth_(platform_->bluetooth()),audio_(platform_->audio()),network_(platform_->network()),usb_(platform_->usb()),display_(platform_->display()),files_(c.storageRoot()),system_(c,*platform_),notes_(c.dataRoot()),notifications_(c.dataRoot()),security_(c.dataRoot()),finance_(c.dataRoot()),financeSecurity_(security_),financeFiles_(finance_,files_){
  bankConfigured_=Finance::BankClient(c.dataRoot()).configured();
  files_.initialize();auto&s=config_.settings();audio_.setVolume(s.volume);audio_.setMuted(s.muted);display_.setBrightness(s.brightness);bluetooth_.setEnabled(s.bluetoothEnabled);network_.setWifiEnabled(s.wifiEnabled);
  searchSession_=std::make_unique<TextInputSession>(notesSearch_,InputType::Search,[]{},[this]{dismissInput();});
  titleSession_=std::make_unique<TextInputSession>(noteTitle_,InputType::Text,[this]{noteHasChanges_=true;noteSaveDebounce_.mark(SDL_GetTicks());},[this]{focusInput(*bodySession_);},[this]{commitNote();},256);
  bodySession_=std::make_unique<TextInputSession>(noteBody_,InputType::Multiline,[this]{noteHasChanges_=true;noteSaveDebounce_.mark(SDL_GetTicks());},TextInputSession::Callback{},[this]{commitNote();},262144);
  financeSearchSession_=std::make_unique<TextInputSession>(financeSearch_,InputType::Search,TextInputSession::Callback{},[this]{dismissInput();});
  financeSecurity_.configure(finance_.settings());if(!finance_.database().warning().empty())toast("FINANCE STORAGE WARNING - SEE SETTINGS");apps_.registerApp(std::make_unique<FilesApp>());apps_.registerApp(std::make_unique<CameraApp>());apps_.registerApp(std::make_unique<BluetoothApp>());apps_.registerApp(std::make_unique<NotesApp>());apps_.registerApp(std::make_unique<FinanceApp>());apps_.registerApp(std::make_unique<SettingsApp>());
  started_=lastFrame_=SDL_GetTicks();Logger::instance().info("CORE","Application manager initialized");
}
Shell::~Shell(){if(bankJob_.valid())bankJob_.wait();commitNote();textInput_.blur();notes_.flush();camera_.stop();persist();}
void Shell::persist(){auto&s=config_.settings();s.volume=audio_.volume();s.muted=audio_.muted();s.brightness=display_.brightness();s.bluetoothEnabled=bluetooth_.enabled();s.wifiEnabled=network_.wifiEnabled();try{config_.save();}catch(const std::exception&e){Logger::instance().error("CONFIG",e.what());}}
bool Shell::hit(Rect r)const{if(pinScreenVisible()&&!renderingPin_)return false;if(renderer_&&SDL_RenderClipEnabled(renderer_)){SDL_Rect clip{};SDL_GetRenderClipRect(renderer_,&clip);if(clickX_<clip.x||clickX_>=clip.x+clip.w||clickY_<clip.y||clickY_>=clip.y+clip.h)return false;}return (!(promptAction_)||renderingPrompt_)&&(!hasAppOverlay()||renderingOverlay_||renderingPrompt_)&&click_&&clickX_>=r.x&&clickX_<=r.x+r.w&&clickY_>=r.y&&clickY_<=r.y+r.h;}
void Shell::handleEvent(const SDL_Event&e){
  if(e.type==SDL_EVENT_QUIT){running_=false;return;}
  SDL_Event event=e;if(renderer_)SDL_ConvertEventToRenderCoordinates(renderer_,&event);const auto now=SDL_GetTicks();
  if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST){lockDevice();return;}
  if(pinScreenVisible()){
    if(event.type==SDL_EVENT_KEY_DOWN){
      if(event.key.key>=SDLK_0&&event.key.key<=SDLK_9)pinKey(std::string(1,static_cast<char>(event.key.key)));
      const SDL_Keycode keys[]={SDLK_KP_0,SDLK_KP_1,SDLK_KP_2,SDLK_KP_3,SDLK_KP_4,SDLK_KP_5,SDLK_KP_6,SDLK_KP_7,SDLK_KP_8,SDLK_KP_9};
      for(int i=0;i<10;++i)if(event.key.key==keys[i])pinKey(std::to_string(i));
      if(event.key.key==SDLK_BACKSPACE)pinKey("DELETE");
      if(event.key.key==SDLK_RETURN||event.key.key==SDLK_KP_ENTER)pinKey("ENTER");
    }
    if(event.type==SDL_EVENT_MOUSE_BUTTON_UP&&event.button.button==SDL_BUTTON_LEFT){click_=true;clickX_=event.button.x;clickY_=event.button.y;}
    return;
  }
  if(event.type==SDL_EVENT_TEXT_INPUT&&textInput_.focused()){textInput_.handlePhysicalText(event.text.text,now);return;}
  if(event.type==SDL_EVENT_KEY_DOWN&&textInput_.focused()){
    const bool selecting=(event.key.mod&SDL_KMOD_SHIFT)!=0;
    if(event.key.key==SDLK_BACKSPACE){textInput_.physicalBackspace(now);return;}if(event.key.key==SDLK_RETURN||event.key.key==SDLK_KP_ENTER){textInput_.physicalEnter(now);return;}
    if(event.key.key==SDLK_LEFT){textInput_.physicalLeft(now,selecting);return;}if(event.key.key==SDLK_RIGHT){textInput_.physicalRight(now,selecting);return;}
    if(event.key.key==SDLK_ESCAPE){dismissInput();return;}
  }
  if(event.type==SDL_EVENT_KEY_DOWN){if(event.key.key==SDLK_F12&&config_.settings().developerMode){dismissInput();developerOpen_=!developerOpen_;}if(event.key.key==SDLK_ESCAPE){if(developerOpen_)developerOpen_=false;else goHome();}}
  if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&event.button.button==SDL_BUTTON_LEFT){if(keyboard_.pointerDown(event.button.x,event.button.y,textInput_,now))return;pointerDown_=true;pointerDragged_=false;pointerY_=event.button.y;}
  if(event.type==SDL_EVENT_MOUSE_MOTION&&pointerDown_&&!textInput_.keyboardVisible()&&(apps_.currentId()=="notes"||apps_.currentId()=="finance")){
    const float dy=event.motion.y-pointerY_;if(std::abs(dy)>1){pointerDragged_=true;if(apps_.currentId()=="finance")financeScroll_=std::max(0.0f,financeScroll_-dy);else if(notesView_==NotesView::List)notesScroll_=std::max(0.0f,notesScroll_-dy);else bodyScroll_=std::max(0.0f,bodyScroll_-dy/28.0f);pointerY_=event.motion.y;}
  }
  if(event.type==SDL_EVENT_MOUSE_WHEEL){if(apps_.currentId()=="finance")financeScroll_=std::max(0.0f,financeScroll_-event.wheel.y*55);else if(apps_.currentId()=="notes"){if(notesView_==NotesView::List)notesScroll_=std::max(0.0f,notesScroll_-event.wheel.y*55);else bodyScroll_=std::max(0.0f,bodyScroll_-event.wheel.y*2);}}
  if(event.type==SDL_EVENT_MOUSE_BUTTON_UP&&event.button.button==SDL_BUTTON_LEFT){if(keyboard_.pointerUp(event.button.x,event.button.y,textInput_,now)){if(!textInput_.focused()&&window_)SDL_StopTextInput(window_);pointerDown_=false;return;}pointerDown_=false;if(!pointerDragged_){click_=true;clickX_=event.button.x;clickY_=event.button.y;}}
}
void Shell::update(double dt){deviceLock_.update(SDL_GetTicks());
  if(pinMode_==PinMode::Saving&&!deviceLock_.busy()){
    if(deviceLock_.revision()!=pinChangeRevision_){pinChanging_=false;pinMode_=PinMode::Unlock;toast("DEVICE PIN CHANGED");}
    else{pinMode_=PinMode::Current;pinMessage_=deviceLock_.status();}
  }
  if(pinScreenVisible())return;
  updateBanks();
  if(auto*app=apps_.current()){app->update(dt);}
  const auto now=SDL_GetTicks();textInput_.update(dt);keyboard_.update(now,textInput_);financeSecurity_.update(now);if(financeJob_.valid()&&financeJob_.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{toast(financeJob_.get());}catch(const std::exception&e){toast(std::string("FINANCE JOB FAILED: ")+e.what());}}if(apps_.currentId()=="finance"&&now/86400000!=financeReminderDay_)financeNotifyBills();if(noteSaveDebounce_.ready(now)){commitNote();noteSaveDebounce_.clear();}if(auto error=notes_.consumeError();!error.empty())toast("NOTE NOT SAVED - WILL REMAIN IN MEMORY");const auto delta=now-lastFrame_;if(delta>0)fps_=fps_*.9+(1000.0/delta)*.1;lastFrame_=now;}
void Shell::render(){SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);SDL_SetRenderDrawColor(renderer_,Theme::Background.r,Theme::Background.g,Theme::Background.b,255);SDL_RenderClear(renderer_);const bool booting=!skipBoot_&&SDL_GetTicks()-started_<2600;if(booting)renderBoot();else if(pinScreenVisible())renderPinScreen();else{renderShell();if(promptAction_)renderPrompt();keyboard_.render(*this,720,1280);}if(!pinScreenVisible()&&!toast_.empty()&&SDL_GetTicks()<toastUntil_){const float y=std::min(1110.0f,textInput_.safeContentBottom(1280)-80);panel({85,y,550,70});label(110,y+22,toast_,2.5f);}if(!capturePath_.empty()){SDL_Surface*surface=SDL_RenderReadPixels(renderer_,nullptr);captureSucceeded_=surface&&SDL_SaveBMP(surface,capturePath_.string().c_str());if(surface)SDL_DestroySurface(surface);capturePath_.clear();}SDL_RenderPresent(renderer_);click_=false;}
bool Shell::captureFrame(const std::filesystem::path&path){capturePath_=path;captureSucceeded_=true;render();return captureSucceeded_;}
void Shell::renderBoot(){const auto elapsed=SDL_GetTicks()-started_;label(245,410,"PINE",10,Theme::Gold);label(190,510,"STARTING PINE OS",3,Theme::Muted);const char*phases[]={"INITIALIZING PLATFORM","LOADING SERVICES","LOADING USER CONFIGURATION","STARTING SHELL"};int shown=std::min(4,static_cast<int>(elapsed/500));for(int i=0;i<shown;++i)label(145,590+i*45,phases[i],2.3f,i==shown-1?Theme::Gold:Theme::Muted);meter({145,800,430,8},std::min(100,static_cast<int>(elapsed*100/2500)));}
void Shell::renderShell(){renderStatusBar();SDL_FlushRenderer(renderer_);if(developerOpen_)renderDeveloperPanel();else if(wifiScreen_)renderWifiScreen();else if(auto*app=apps_.current())app->render(*this);else renderHome();SDL_FlushRenderer(renderer_);if(!textInput_.keyboardVisible())renderNavigation();SDL_FlushRenderer(renderer_);}
void Shell::renderHome(){renderHomeScreen(*this);}void Shell::renderStatusBar(){Pine::renderStatusBar(*this);}void Shell::renderNavigation(){Pine::renderNavigation(*this);}void Shell::renderDeveloperPanel(){Pine::renderDeveloperPanel(*this);}
void Shell::panel(Rect r){roundedFill(renderer_,r,12,Theme::Border);roundedFill(renderer_,{r.x+1,r.y+1,r.w-2,r.h-2},11,Theme::Surface);}
void Shell::coloredPanel(Rect r,SDL_Color fill,SDL_Color border,float radius){if(radius<=0){SDL_SetRenderDrawColor(renderer_,border.r,border.g,border.b,border.a);SDL_FRect f{r.x,r.y,r.w,r.h};SDL_RenderFillRect(renderer_,&f);SDL_SetRenderDrawColor(renderer_,fill.r,fill.g,fill.b,fill.a);SDL_FRect inner{r.x,r.y+1,r.w,r.h-1};SDL_RenderFillRect(renderer_,&inner);return;}roundedFill(renderer_,r,radius,border);roundedFill(renderer_,{r.x+1,r.y+1,r.w-2,r.h-2},std::max(0.0f,radius-1),fill);}
void Shell::label(float x,float y,const std::string&t,float scale,SDL_Color c){drawText(renderer_,x,y,t,scale,c);}void Shell::sublabel(float x,float y,const std::string&t){label(x,y,t,2,Theme::Muted);}
bool Shell::button(Rect r,const std::string&t,bool accent){const auto fill=accent?Theme::Gold:Theme::SurfaceRaised;roundedFill(renderer_,r,9,accent?Theme::GoldSoft:Theme::Border);roundedFill(renderer_,{r.x+1,r.y+1,r.w-2,r.h-2},8,fill);auto tw=textWidth(t,2.4f);label(r.x+(r.w-tw)/2,r.y+(r.h-20)/2,t,2.4f,accent?Theme::OnGold:Theme::Ink);return hit(r);}
void Shell::meter(Rect r,int value){SDL_SetRenderDrawColor(renderer_,Theme::SurfaceRaised.r,Theme::SurfaceRaised.g,Theme::SurfaceRaised.b,255);SDL_FRect bg{r.x,r.y,r.w,r.h};SDL_RenderFillRect(renderer_,&bg);SDL_SetRenderDrawColor(renderer_,Theme::Gold.r,Theme::Gold.g,Theme::Gold.b,255);SDL_FRect fg{r.x,r.y,r.w*std::clamp(value,0,100)/100.f,r.h};SDL_RenderFillRect(renderer_,&fg);}
void Shell::toast(std::string value){toast_=std::move(value);toastUntil_=SDL_GetTicks()+2300;}
bool Shell::launchApp(const std::string&id){
#ifdef PINE_TAB5
  const bool financeClockInvalid=id=="finance"&&!tab5ClockValid();
#endif
  if(pinScreenVisible())return false;
  wifiScreen_=false;

  if(apps_.currentId()=="notes"&&id!="notes")commitNote();
  if(apps_.currentId()=="finance"&&id!="finance")financeSecurity_.onBackground(SDL_GetTicks());
  if(apps_.currentId()=="camera"&&id!="camera")camera_.stop();
  if(apps_.currentId()!=id)toast_.clear();
  dismissInput();
  cancelPrompts();
  deleteNotePrompt_=false;
  const bool opened=apps_.launch(id);
  if(opened&&id=="finance"){
    financeSecurity_.configure(finance_.settings());
    financeSecurity_.onOpen(SDL_GetTicks());
    financeNotifyBills();
#ifdef PINE_TAB5
    if(financeClockInvalid)toast("CLOCK NOT SET - CONNECT WI-FI FOR CORRECT DATES");
#endif
  }
  return opened;
}
void Shell::goHome(){if(pinScreenVisible())return;wifiScreen_=false;cancelPrompts();commitNote();if(apps_.currentId()=="finance")financeSecurity_.onBackground(SDL_GetTicks());if(apps_.currentId()=="camera")camera_.stop();dismissInput();deleteNotePrompt_=false;if(notesView_==NotesView::Editor)notesView_=NotesView::List;apps_.home();}
bool Shell::hasAppOverlay()const {
  return deleteNotePrompt_||financeQuickMenu_||financeTypePicker_||financeTransactionPicker_||financeTransferPicker_||financeFilterPrompt_||financeDashboardPrompt_||financeDeletePrompt_||financeArchivePrompt_||financeErasePrompt_||financeRestorePrompt_||financeImportPrompt_;
}
void Shell::cancelPrompts(){
  dismissInput();promptAction_={};promptSession_.reset();promptText_.clear();
  deleteNotePrompt_=financeQuickMenu_=financeTypePicker_=financeTransactionPicker_=financeTransferPicker_=financeFilterPrompt_=financeDashboardPrompt_=financeDeletePrompt_=financeArchivePrompt_=financeErasePrompt_=financeRestorePrompt_=financeImportPrompt_=false;
}
void Shell::focusInput(TextInputSession&session){if(pinScreenVisible())return;textInput_.focus(session,SDL_GetTicks(),true);if(window_)SDL_StartTextInput(window_);}
void Shell::dismissInput(){textInput_.blur();if(window_)SDL_StopTextInput(window_);}
void Shell::openTextPrompt(const std::string&title,std::string initial,std::function<void(const std::string&)>action,InputType type,std::size_t maxLength){
  if(pinScreenVisible())return;
  dismissInput();
  promptTitle_=title;promptText_=std::move(initial);promptAction_=std::move(action);
  promptSession_=std::make_unique<TextInputSession>(promptText_,type,TextInputSession::Callback{},[this]{if(promptAction_&&!promptText_.empty()){auto action=std::move(promptAction_);auto value=promptText_;promptAction_={};dismissInput();action(value);}},TextInputSession::Callback{},maxLength);focusInput(*promptSession_);
}
void Shell::renderPrompt(){
  renderingPrompt_=true;
  SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(renderer_,0,0,0,210);SDL_FRect dim{0,0,720,1280};SDL_RenderFillRect(renderer_,&dim);
  const float top=std::min(430.0f,textInput_.safeContentBottom(1280)-300);panel({65,top,590,270});label(100,top+38,promptTitle_,4,Theme::Gold);drawTextField({100,top+105,520,65},*promptSession_,"ENTER TEXT");
  if(button({100,top+190,245,60},"CANCEL")){promptAction_={};dismissInput();promptSession_.reset();}
  if(button({375,top+190,245,60},"SAVE",true)&&!promptText_.empty()){auto action=std::move(promptAction_);auto value=promptText_;promptAction_={};dismissInput();promptSession_.reset();action(value);}SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);renderingPrompt_=false;
}
}
