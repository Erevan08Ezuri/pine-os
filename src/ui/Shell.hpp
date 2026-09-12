#pragma once
#include "core/ApplicationManager.hpp"
#include "core/Configuration.hpp"
#include "core/Debouncer.hpp"
#include "core/UiContext.hpp"
#include "input/OnScreenKeyboard.hpp"
#include "input/TextInputManager.hpp"
#include "services/AudioService.hpp"
#include "services/BatteryService.hpp"
#include "services/BluetoothService.hpp"
#include "services/CameraService.hpp"
#include "services/FileService.hpp"
#include "services/DisplayService.hpp"
#include "services/NetworkService.hpp"
#include "services/NotesService.hpp"
#include "services/NotificationService.hpp"
#include "services/SecurityService.hpp"
#include "finance/FinanceService.hpp"
#include "finance/FinanceSecurityManager.hpp"
#include "finance/ImportExportManager.hpp"
#include "services/SystemService.hpp"
#include "services/UsbService.hpp"
#include "ui/Theme.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Pine {
struct Rect{float x,y,w,h;};
class Shell final:public UiContext{
public:
  Shell(SDL_Window*,SDL_Renderer*,Configuration&,std::unique_ptr<Platform>);
  ~Shell(); bool running()const{return running_;} void handleEvent(const SDL_Event&); void update(double); void render();
  void renderSettings()override;void renderFiles()override;void renderCamera()override;void renderBluetooth()override;void renderNotes()override;void renderFinance()override;
  SDL_Renderer*renderer()const{return renderer_;} bool button(Rect,const std::string&,bool accent=false);void panel(Rect);void coloredPanel(Rect,SDL_Color fill,SDL_Color border,float radius=12);void label(float,float,const std::string&,float=3,SDL_Color=Theme::Ink);void sublabel(float,float,const std::string&);void meter(Rect,int);
  BatteryService&battery(){return battery_;}BluetoothService&bluetooth(){return bluetooth_;}CameraService&camera(){return camera_;}AudioService&audio(){return audio_;}NetworkService&network(){return network_;}UsbService&usb(){return usb_;}DisplayService&display(){return display_;}FileService&files(){return files_;}SystemService&system(){return system_;}NotesService&notes(){return notes_;}Finance::FinanceService&finance(){return finance_;}Configuration&config(){return config_;}ApplicationManager&apps(){return apps_;}TextInputManager&textInput(){return textInput_;}
  double fps()const{return fps_;}void persist();void openTextPrompt(const std::string&,std::string,std::function<void(const std::string&)>,InputType type=InputType::Text,std::size_t maxLength=128);
  bool launchApp(const std::string&);void goHome();void focusInput(TextInputSession&);void dismissInput();
  void showToast(const std::string&value){toast(value);}
  void acceptanceSkipBoot(){skipBoot_=true;} void acceptanceDeveloper(bool value){developerOpen_=value;}
  bool acceptanceLaunch(const std::string&id){return launchApp(id);} void acceptancePath(const std::filesystem::path&p){currentPath_=p;selected_.reset();}
  bool acceptanceCreateNote(const std::string&title,const std::string&body,bool pinned);
  void acceptanceBeginNote(){beginNewNote();}void acceptanceInput(const std::string&text){textInput_.insertFromKeyboard(text);}void acceptanceKey(const std::string&key,std::uint64_t now){keyboard_.press(key,textInput_,now);}void acceptanceDismissKeyboard(){dismissInput();}void acceptanceCloseNote(){closeNoteEditor();}void acceptanceSearch(const std::string&value){notesSearch_=value;searchSession_->setCursor(value.size());}void acceptancePinNote(){notePinned_=true;noteHasChanges_=true;commitNote();}
  void acceptanceShowDelete(){dismissInput();deleteNotePrompt_=true;}void acceptanceConfirmDelete(){confirmDeleteNote();}std::size_t acceptanceNoteCount(){return notes_.list().size();}
  void acceptanceFinanceView(const std::string&);bool acceptanceFinanceExport();bool acceptanceFinanceLock(const std::string&pin);bool acceptanceFinanceUnlock(const std::string&pin){return financeSecurity_.unlock(pin,SDL_GetTicks());}
  bool captureFrame(const std::filesystem::path&path);
private:
  enum class NotesView{List,Editor};
  enum class FinanceView{Home,Accounts,AccountDetail,Activity,Budget,Goals,More,Bills,Subscriptions,Analytics,Calculator,Categories,Settings};
  bool hasAppOverlay()const;void cancelPrompts();
  void renderBoot();void renderShell();void renderHome();void renderStatusBar();void renderNavigation();void renderDeveloperPanel();void renderPrompt();bool hit(Rect)const;void toast(std::string);
  void beginNewNote();void openNote(const std::string&);void commitNote();void closeNoteEditor();void confirmDeleteNote();void drawTextField(Rect,TextInputSession&,const std::string&,bool multiline=false);std::size_t cursorAt(Rect,const TextInputSession&,float,float,bool multiline);
  void renderFinanceHeader(const std::string&);void renderFinanceNav();void renderFinanceHome();void renderFinanceAccounts();void renderFinanceAccountDetail();void renderFinanceActivity();void renderFinanceBudgets();void renderFinanceGoals();void renderFinanceMore();void renderFinanceBills();void renderFinanceSubscriptions();void renderFinanceAnalytics();void renderFinanceCalculator();void renderFinanceCategories();void renderFinanceSettings();void renderFinanceOverlay();
  void beginFinanceAccount(const std::string&);void beginFinanceTransaction();void beginFinanceTransfer();void beginFinanceBudget();void beginFinanceBill();void beginFinanceSubscription();void beginFinanceGoal();void beginFinanceImport();void beginFinanceUnlock();void financeNotifyBills();std::string financeAmount(std::int64_t,const std::string&currency="",bool signedValue=false)const;
  SDL_Window*window_;SDL_Renderer*renderer_;Configuration&config_;std::unique_ptr<Platform>platform_;ApplicationManager apps_;
  BatteryService battery_;CameraService camera_;BluetoothService bluetooth_;AudioService audio_;NetworkService network_;UsbService usb_;DisplayService display_;FileService files_;SystemService system_;NotesService notes_;NotificationService notifications_;SecurityService security_;Finance::FinanceService finance_;Finance::FinanceSecurityManager financeSecurity_;Finance::ImportExportManager financeFiles_;
  TextInputManager textInput_;OnScreenKeyboard keyboard_;
  bool renderingPrompt_{},renderingOverlay_{};
  bool running_{true},developerOpen_{false},click_{false},skipBoot_{false},pointerDown_{false},pointerDragged_{false};float clickX_{},clickY_{},pointerY_{};std::uint64_t started_{},lastFrame_{};double fps_{0};std::string toast_;std::uint64_t toastUntil_{};
  std::filesystem::path currentPath_;std::optional<FileEntry>selected_;std::string promptTitle_,promptText_;std::function<void(const std::string&)>promptAction_;std::unique_ptr<TextInputSession>promptSession_;
  NotesView notesView_{NotesView::List};std::string notesSearch_,editingNoteId_,noteTitle_,noteBody_;bool notePinned_{},deleteNotePrompt_{},noteHasChanges_{};float notesScroll_{},bodyScroll_{};Debouncer noteSaveDebounce_{750};
  std::unique_ptr<TextInputSession>searchSession_,titleSession_,bodySession_;
  FinanceView financeView_{FinanceView::Home};float financeScroll_{};bool financeQuickMenu_{},financeTypePicker_{},financeTransactionPicker_{},financeTransferPicker_{},financeFilterPrompt_{},financeDashboardPrompt_{},financeDeletePrompt_{},financeArchivePrompt_{},financeErasePrompt_{},financeRestorePrompt_{},financeImportPrompt_{};std::string financeSelectedAccount_,financeSelectedTransaction_,financeSearch_,financeDraftType_{"Expense"},financeDraftAccount_,financeDraftTarget_,financeDraftName_,financeDraftInstitution_,financeDraftAmount_,financeDraftCategory_,financeDraftDate_;std::string financeFilterAccount_,financeFilterCategory_,financeFilterType_,financeFilterStatus_,financeFilterFrom_,financeFilterTo_,financeFilterMin_,financeFilterMax_;std::vector<Finance::ImportPreviewRow>financeImportRows_;Finance::CsvMapping financeCsvMapping_;std::unique_ptr<TextInputSession>financeSearchSession_;std::string financeCalculatorResult_;std::uint64_t financeReminderDay_{};
  std::future<std::string> financeJob_;
  std::filesystem::path capturePath_;bool captureSucceeded_{true};
};
void renderHomeScreen(Shell&);void renderStatusBar(Shell&);void renderNavigation(Shell&);void renderDeveloperPanel(Shell&);
}
