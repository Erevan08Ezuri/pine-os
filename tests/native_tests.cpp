#include "core/ApplicationManager.hpp"
#include "core/Configuration.hpp"
#include "core/Debouncer.hpp"
#include "core/Logger.hpp"
#include "input/OnScreenKeyboard.hpp"
#include "input/TextInputManager.hpp"
#include "input/TextInputSession.hpp"
#include "platform/Platform.hpp"
#include "services/BatteryService.hpp"
#include "services/BluetoothService.hpp"
#include "services/FileService.hpp"
#include "services/DisplayService.hpp"
#include "services/NetworkService.hpp"
#include "services/NotesService.hpp"
#include "services/UsbService.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace Pine;
static int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" "#x"\n";++failures;}}while(0)
class TestApp final:public Application{public:explicit TestApp(std::string value):value_(std::move(value)){}std::string id()const override{return value_;}std::string name()const override{return value_;}void onOpen()override{opened=true;}void onClose()override{closed=true;}void render(UiContext&)override{}bool opened=false,closed=false;private:std::string value_;};
int main(){
  auto temp=std::filesystem::temp_directory_path()/"pine-native-tests";std::filesystem::remove_all(temp);std::filesystem::create_directories(temp);Logger::instance().initialize(temp);
  std::cerr<<"[TEST] configuration persistence\n";
  {Configuration c(temp/"config");c.load();CHECK(c.usedFallback());c.settings().volume=41;c.settings().brightness=22;c.save();Configuration loaded(temp/"config");loaded.load();CHECK(loaded.settings().volume==41);CHECK(loaded.settings().brightness==22);}
  std::cerr<<"[TEST] malformed configuration\n";
  {std::ofstream(temp/"config"/"settings.json")<<"{broken";Configuration c(temp/"config");c.load();CHECK(c.usedFallback());CHECK(c.settings().volume==75);}
  std::cerr<<"[TEST] filesystem sandbox\n";
  {FileService f(temp/"storage");f.initialize();CHECK(f.createDirectory("Documents/Test"));std::ofstream(f.resolve("Documents/Test/a.txt"))<<"pine";CHECK(f.rename("Documents/Test/a.txt","b.txt"));CHECK(f.exists("Documents/Test/b.txt"));CHECK(f.copy("Documents/Test/b.txt","Downloads/c.txt"));CHECK(f.move("Downloads/c.txt","Pictures/c.txt"));CHECK(f.remove("Pictures/c.txt"));bool rejected=false;try{(void)f.resolve("../../Windows/System32");}catch(...){rejected=true;}CHECK(rejected);}
  std::cerr<<"[TEST] notes create/edit/sort/search/persistence/delete\n";
  std::string persistedId,pinnedId;
  {NotesService notes(temp/"notes-data");auto first=notes.create();persistedId=first.id;CHECK(notes.update(first.id,"Field Journal","Alpine signal at dawn",false));std::this_thread::sleep_for(std::chrono::milliseconds(2));auto pinned=notes.create();pinnedId=pinned.id;CHECK(notes.update(pinned.id,"Pinned","Important",true));auto sorted=notes.list();CHECK(sorted.size()==2);CHECK(sorted.front().id==pinned.id&&sorted.front().pinned);auto search=notes.list("ALPINE");CHECK(search.size()==1&&search.front().id==first.id);notes.flush();}
  {NotesService notes(temp/"notes-data");auto loaded=notes.get(persistedId);CHECK(loaded&&loaded->title=="Field Journal"&&loaded->body=="Alpine signal at dawn");CHECK(notes.remove(pinnedId));notes.flush();}
  {NotesService notes(temp/"notes-data");CHECK(!notes.get(pinnedId));CHECK(notes.list().size()==1);}
  std::cerr<<"[TEST] text editing and multiline\n";
  {std::string value="ac";int changes=0;TextInputSession session(value,InputType::Text,[&]{++changes;});session.setCursor(1);session.insertText("b");CHECK(value=="abc");session.setSelection(1,3);session.replaceSelection("Z");CHECK(value=="aZ");session.deleteBackward();CHECK(value=="a"&&changes==3);session.insertText("\nq");CHECK(value=="aq");std::string body;TextInputSession multiline(body,InputType::Multiline);multiline.insertText("one");multiline.submit();multiline.insertText("two");CHECK(body=="one\ntwo");}
  std::cerr<<"[TEST] text focus and keyboard visibility\n";
  {std::string one,two;int blurred=0;TextInputSession first(one,InputType::Text,{}, {},[&]{++blurred;});TextInputSession second(two,InputType::Search,{}, {},[&]{++blurred;});TextInputManager manager;manager.focus(first,1);manager.update(.2);CHECK(manager.keyboardVisible()&&manager.isFocused(first));manager.focus(second,2);CHECK(blurred==1&&manager.isFocused(second));manager.hideKeyboard(true);manager.update(.2);CHECK(!manager.keyboardVisible()&&manager.focused()==nullptr&&blurred==2);manager.focus(first,100);manager.handlePhysicalText("x",100);CHECK(one=="x"&&!manager.keyboardTargetVisible());manager.setForceSoftwareKeyboard(true);manager.showKeyboard();CHECK(manager.keyboardTargetVisible());}
  std::cerr<<"[TEST] software keyboard shift/caps/symbols/repeat\n";
  {std::string value;TextInputSession session(value,InputType::Text);TextInputManager manager;manager.setForceSoftwareKeyboard(true);manager.focus(session,1);OnScreenKeyboard keyboard;keyboard.press("SHIFT",manager,100);keyboard.press("a",manager,150);CHECK(value=="A"&&keyboard.shiftState()==ShiftState::Inactive);keyboard.press("SHIFT",manager,1000);keyboard.press("SHIFT",manager,1200);CHECK(keyboard.shiftState()==ShiftState::CapsLock);keyboard.press("b",manager,1250);keyboard.press("c",manager,1300);CHECK(value=="ABC");keyboard.press("SHIFT",manager,1400);CHECK(keyboard.shiftState()==ShiftState::Inactive);keyboard.press("123",manager,1500);CHECK(keyboard.layout()==KeyboardLayout::Symbols);keyboard.press("1",manager,1550);keyboard.press("+",manager,1600);CHECK(value=="ABC1+");keyboard.press("ABC",manager,1700);CHECK(keyboard.layout()==KeyboardLayout::Alphabet);keyboard.beginBackspace(manager,2000);CHECK(value=="ABC1");keyboard.update(2449,manager);CHECK(value=="ABC1");keyboard.update(2450,manager);CHECK(value=="ABC");keyboard.update(2530,manager);CHECK(value=="AB");keyboard.endBackspace();keyboard.update(3000,manager);CHECK(value=="AB");}
  std::cerr<<"[TEST] input modes and autosave debounce\n";
  {std::string number;TextInputSession numeric(number,InputType::Number);numeric.insertText("12a-3.5");CHECK(number=="12-3.5");Debouncer debounce(750);debounce.mark(1000);CHECK(!debounce.ready(1749));CHECK(debounce.ready(1750));debounce.clear();CHECK(!debounce.dirty());}
  std::cerr<<"[TEST] application manager\n";
  {ApplicationManager m;auto app=std::make_unique<TestApp>("files");auto*raw=app.get();m.registerApp(std::move(app));CHECK(m.launch("files"));CHECK(raw->opened);CHECK(m.currentId()=="files");CHECK(m.launch("files"));m.home();CHECK(raw->closed);CHECK(m.current()==nullptr);bool duplicate=false;try{m.registerApp(std::make_unique<TestApp>("files"));}catch(...){duplicate=true;}CHECK(duplicate);}
  std::cerr<<"[TEST] platform factory\n";
  auto platform=createPlatform("desktop");
  std::cerr<<"[TEST] battery\n";
  {BatteryService b(platform->battery());CHECK(b.percentage()==80);b.simulatePercentage(17);b.simulateCharging(true);CHECK(b.percentage()==17&&b.isCharging());b.simulatePercentage(120);CHECK(b.percentage()==100);}
  std::cerr<<"[TEST] bluetooth\n";
  {BluetoothService b(platform->bluetooth());b.startScan();CHECK(b.devices().size()==4);CHECK(b.connect("pine-buds"));CHECK(b.devices()[0].connected);CHECK(b.disconnect("pine-buds"));b.connect("pine-buds");b.setEnabled(false);CHECK(!b.enabled()&&!b.devices()[0].connected);CHECK(!b.connect("pine-buds"));}
  std::cerr<<"[TEST] network\n";
  {NetworkService n(platform->network());n.setWifiEnabled(false);CHECK(!n.wifiEnabled()&&!n.connected());n.setWifiEnabled(true);CHECK(n.connected()&&n.networkName()=="Desktop Host Network");}
  {DisplayService d(platform->display());d.setBrightness(29);CHECK(d.brightness()==29);d.setBrightness(200);CHECK(d.brightness()==100);}
  std::cerr<<"[TEST] usb\n";
  {UsbService u(platform->usb());CHECK(u.mode()==UsbMode::Disconnected);u.cycle();CHECK(u.mode()==UsbMode::ChargingOnly);u.cycle();CHECK(u.mode()==UsbMode::FileTransfer);u.cycle();CHECK(u.mode()==UsbMode::Disconnected);}
  std::cerr<<"[TEST] complete\n";
  if(failures){std::cerr<<failures<<" tests failed\n";return 1;}std::cout<<"All Pine OS native tests passed\n";return 0;
}
