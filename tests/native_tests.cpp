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
#include "services/SecurityService.hpp"
#include "services/NotificationService.hpp"
#include "finance/AnalyticsEngine.hpp"
#include "finance/FinanceSecurityManager.hpp"
#include "finance/FinanceService.hpp"
#include "finance/FinancialDataProvider.hpp"
#include "finance/ImportExportManager.hpp"
#include "finance/Money.hpp"
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <format>
#include <iostream>
#include <limits>
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
  std::cerr<<"[TEST] exact money and currencies\n";
  {
    const auto folder=temp/"notes-data"/"notes";
    const auto target=folder/(persistedId+".json");
    std::filesystem::rename(target,target.string()+".bak");
    std::ofstream(folder/"tampered.json")<<R"({"id":"../outside","title":"bad","created_at":1,"updated_at":1})";
    NotesService notes(temp/"notes-data");
    CHECK(notes.get(persistedId)&&notes.get(persistedId)->title=="Field Journal");
    CHECK(notes.list().size()==1);CHECK(!notes.get("../outside"));
    CHECK(std::filesystem::exists(target));
  }
  {
    std::string value="abcde";TextInputSession limited(value,InputType::Text,{}, {}, {},3);
    limited.insertText("x");CHECK(value=="abcde");
    limited.selectAll();limited.insertText("\xC3\xA9\xC3\xA9");CHECK(value=="\xC3\xA9");
    limited.setCursor(1);CHECK(limited.cursor()==0);
    limited.setSelection(1,2);limited.insertText("a");CHECK(value=="a");
    std::string multiline;TextInputSession body(multiline,InputType::Multiline);
    body.insertText("first\r\nsecond\rthird\nfourth");CHECK(multiline=="first\nsecond\nthird\nfourth");
  }
  {using namespace Finance;auto usd=Currency::fromCode("usd");auto ten=parseMoneyMinor("0.10",usd),twenty=parseMoneyMinor("0.20",usd);auto exactSum=Money{*ten,usd}+Money{*twenty,usd};CHECK(ten&&twenty&&exactSum.minor==30);std::int64_t pennies=0;for(int i=0;i<100;++i){auto sum=Money{pennies,usd}+Money{1,usd};pennies=sum.minor;}CHECK(pennies==100);CHECK(parseMoneyMinor("18.57",usd)==1857);CHECK(Currency::fromCode("JPY").minorDigits==0);CHECK(parseMoneyMinor("100",Currency::fromCode("JPY"))==100);CHECK(Currency::fromCode("BHD").minorDigits==3);CHECK(parseMoneyMinor("1.234",Currency::fromCode("BHD"))==1234);CHECK(!parseMoneyMinor("1.2345",Currency::fromCode("BHD")));}
  std::cerr<<"[TEST] finance accounts transactions transfers budgets and persistence\n";
  std::string checkingId,savingsId,customCategoryId,deletedTransactionId;
  auto financeRoot=temp/"finance-data";auto financeStorage=temp/"finance-storage";
  {using namespace Finance;FinanceService finance(financeRoot);auto checking=finance.createAccount("Main Checking","Checking","Example Bank",100000,"USD");checkingId=checking.id;auto savings=finance.createAccount("Savings","Savings","Example Bank",0,"USD");savingsId=savings.id;CHECK(finance.accounts().size()==2);checking.name="Everyday Checking";CHECK(finance.updateAccount(checking));CHECK(finance.account(checking.id)->name=="Everyday Checking");auto income=finance.addTransaction(checking.id,"Income",50000,"Employer","Income",FinanceService::today());auto expense=finance.addTransaction(checking.id,"Expense",2000,"Utility Shop","Utilities",FinanceService::today());CHECK(finance.account(checking.id)->currentBalance==148000);auto transfer=finance.transfer(checking.id,savings.id,20000,FinanceService::today());CHECK(!transfer.first.transferId.empty()&&transfer.first.transferId==transfer.second.transferId);CHECK(finance.account(checking.id)->currentBalance==128000);CHECK(finance.account(savings.id)->currentBalance==20000);CHECK(finance.dashboard().netWorth==148000);auto before=finance.account(checking.id)->currentBalance;bool atomicFailure=false;try{finance.transfer(checking.id,"missing",1000,FinanceService::today());}catch(...){atomicFailure=true;}CHECK(atomicFailure&&finance.account(checking.id)->currentBalance==before);auto disposable=finance.addTransaction(checking.id,"Expense",100,"Test merchant","Other",FinanceService::today());deletedTransactionId=disposable.id;CHECK(finance.deleteTransaction(disposable.id));CHECK(finance.account(checking.id)->currentBalance==before);auto custom=finance.createCategory("Pets","PT");customCategoryId=custom.id;auto pet=finance.addTransaction(checking.id,"Expense",500,"Vet Office","Pets",FinanceService::today(),"checkup");CHECK(finance.renameCategory(custom.id,"Animals"));CHECK(finance.transactions("VET").size()==1);CHECK(finance.transactions("checkup").size()==1);CHECK(finance.transactions("","","Animals").size()==1);CHECK(finance.hideCategory(custom.id,true));CHECK(std::ranges::none_of(finance.categories(),[&](const auto&c){return c.id==custom.id;}));auto foodBudget=finance.setBudget("Food & Dining",FinanceService::currentMonth(),30000);finance.addTransaction(checking.id,"Expense",2500,"Cafe","Food & Dining",FinanceService::today());CHECK(finance.budgetSpent(foodBudget)==2500);CHECK(finance.budgetAvailable(foodBudget)==27500);auto current=std::chrono::year_month(std::chrono::year(std::stoi(FinanceService::currentMonth().substr(0,4))),std::chrono::month(std::stoi(FinanceService::currentMonth().substr(5,2))));auto prior=current-std::chrono::months(1);auto priorMonth=std::format("{:04}-{:02}",int(prior.year()),unsigned(prior.month()));auto priorBudget=finance.setBudget("Entertainment",priorMonth,10000);finance.addTransaction(checking.id,"Expense",7000,"Cinema","Entertainment",priorMonth+"-15");auto rollover=finance.setBudget("Entertainment",FinanceService::currentMonth(),10000,true);CHECK(finance.budgetAvailable(rollover)==13000);auto bill=finance.addBill("Internet",6500,checking.id,"Internet",FinanceService::today(),"Monthly");CHECK(finance.bills().size()==1&&finance.bills()[0].id==bill.id);auto subscription=finance.addSubscription("Music",1099,"Monthly",checking.id,FinanceService::today(),"Subscriptions");CHECK(finance.subscriptions().size()==1&&finance.dashboard().subscriptionMonthly==1099);auto goal=finance.addGoal("Emergency Fund",300000);CHECK(finance.contribute(goal.id,5000));CHECK(finance.goals()[0].currentAmount==5000);auto cash=AnalyticsEngine(finance).cashFlow(priorMonth+"-01",FinanceService::today());CHECK(cash.income==50000);CHECK(cash.expenses>=12000);CHECK(cash.net==cash.income-cash.expenses);CHECK(std::ranges::none_of(finance.transactions(),[](const auto&t){return t.id.empty();}));CHECK(finance.archiveAccount(savings.id));CHECK(finance.accounts().size()==1);CHECK(finance.accounts(true).size()==2);CHECK(finance.archiveAccount(savings.id,false));CHECK(finance.balanceHistory(checking.id).size()>=1);}
  {using namespace Finance;FinanceService finance(financeRoot);CHECK(finance.account(checkingId).has_value());CHECK(finance.account(checkingId)->name=="Everyday Checking");CHECK(!finance.transactions("Employer").empty());CHECK(finance.budgets(FinanceService::currentMonth()).size()>=2);CHECK(finance.bills().size()==1);CHECK(finance.subscriptions().size()==1);CHECK(finance.goals().size()==1);CHECK(!finance.deleteTransaction(deletedTransactionId));auto settings=finance.settings();settings.dashboardMask=5;settings.hideBalances=true;finance.saveSettings(settings);CHECK(finance.settings().dashboardMask==5&&finance.settings().hideBalances);}
  {using namespace Finance;FinanceService finance(financeRoot);auto edge=finance.createAccount("Overflow Guard","Cash","",std::numeric_limits<std::int64_t>::max()-5,"USD");bool overflow=false;try{finance.addTransaction(edge.id,"Income",10,"Overflow","Other",FinanceService::today());}catch(const std::overflow_error&){overflow=true;}CHECK(overflow&&finance.account(edge.id)->currentBalance==std::numeric_limits<std::int64_t>::max()-5);}
  std::cerr<<"[TEST] finance CSV import duplicate export backup restore\n";
  {using namespace Finance;FinanceService finance(financeRoot);FileService files(financeStorage);files.initialize();std::filesystem::create_directories(files.resolve("Documents/Finance"));std::ofstream(files.resolve("Documents/Finance/import.csv"))<<"date,description,amount,category\n"<<FinanceService::today()<<",Grocery Market,-12.34,Groceries\n"<<"bad-date,Broken,not-money,Other\n";ImportExportManager io(finance,files);CsvMapping mapping;mapping.category=3;auto preview=io.previewCsv("Documents/Finance/import.csv",checkingId,mapping);CHECK(preview.size()==2&&preview[0].valid&&!preview[0].duplicate&&!preview[1].valid);auto first=io.importCsv("Documents/Finance/import.csv",checkingId,mapping,true);CHECK(first.imported==1&&first.invalid==1);auto second=io.importCsv("Documents/Finance/import.csv",checkingId,mapping,true);CHECK(second.imported==0&&second.duplicates==1);std::ofstream(files.resolve("Documents/Finance/statement.qfx"))<<"OFXHEADER:100\n<OFX><BANKMSGSRSV1><STMTTRNRS><STMTRS><BANKTRANLIST><STMTTRN><TRNTYPE>DEBIT<DTPOSTED>20260909120000<TRNAMT>-7.50<FITID>pine-fit-1<NAME>Coffee House</STMTTRN></BANKTRANLIST></STMTRS></STMTTRNRS></BANKMSGSRSV1></OFX>";auto ofxPreview=io.previewOfx("Documents/Finance/statement.qfx",checkingId);CHECK(ofxPreview.size()==1&&ofxPreview[0].valid&&!ofxPreview[0].duplicate&&ofxPreview[0].providerId=="pine-fit-1");auto ofxFirst=io.importOfx("Documents/Finance/statement.qfx",checkingId,true);auto ofxSecond=io.importOfx("Documents/Finance/statement.qfx",checkingId,true);CHECK(ofxFirst.imported==1&&ofxSecond.duplicates==1);CHECK(std::filesystem::exists(io.exportTransactionsCsv("Documents/Finance/transactions.csv")));CHECK(std::filesystem::exists(io.exportEverythingJson("Documents/Finance/everything.json")));auto baseline=finance.account(checkingId)->currentBalance;CHECK(std::filesystem::exists(io.createBackup("Documents/Finance/backup.pinefinance")));finance.addTransaction(checkingId,"Expense",999,"After Backup","Other",FinanceService::today());CHECK(finance.account(checkingId)->currentBalance==baseline-999);CHECK(io.restoreBackup("Documents/Finance/backup.pinefinance",true));CHECK(finance.account(checkingId)->currentBalance==baseline);bool guarded=false;try{io.restoreBackup("Documents/Finance/backup.pinefinance",false);}catch(...){guarded=true;}CHECK(guarded);}
  std::cerr<<"[TEST] finance security and offline provider\n";
  {using namespace Finance;SecurityService security(temp/"security-data");CHECK(!security.hasDevicePin());CHECK(!security.setDevicePin("12"));CHECK(security.setDevicePin("4826"));CHECK(security.hasDevicePin()&&security.authenticate("4826")&&!security.authenticate("0000"));FinanceSecurityManager lock(security);FinanceSettings settings;settings.requireAuthentication=true;settings.autoLock="Immediately";lock.configure(settings);lock.onOpen(100);CHECK(lock.locked());CHECK(lock.unlock("4826",101));CHECK(!lock.locked());lock.onBackground(102);CHECK(lock.locked());ManualProvider manual;FinanceSyncManager offline(false);auto result=offline.sync(manual);CHECK(result.success&&result.state=="Manual");}
  std::cerr<<"[TEST] central notification deduplication and database recovery\n";
  {NotificationService notifications(temp/"notification-data");notifications.postOnce("bill-1","Finance","Bill reminder","A bill is due",true);notifications.postOnce("bill-1","Finance","Bill reminder","duplicate",true);CHECK(notifications.list().size()==1&&notifications.list()[0].sensitive);auto corrupt=temp/"corrupt-finance"/"finance";std::filesystem::create_directories(corrupt);std::ofstream(corrupt/"finance.db",std::ios::binary)<<"not a sqlite database";Finance::FinanceService recovered(temp/"corrupt-finance");CHECK(!recovered.database().warning().empty());CHECK(recovered.accounts().empty());bool preserved=false;for(auto&e:std::filesystem::directory_iterator(corrupt))if(e.path().filename().string().find("recovery-")!=std::string::npos)preserved=true;CHECK(preserved);}
  std::cerr<<"[TEST] text editing and multiline\n";
  {std::string value="ac";int changes=0;TextInputSession session(value,InputType::Text,[&]{++changes;});session.setCursor(1);session.insertText("b");CHECK(value=="abc");session.setSelection(1,3);session.replaceSelection("Z");CHECK(value=="aZ");session.deleteBackward();CHECK(value=="a"&&changes==3);session.insertText("\nq");CHECK(value=="aq");std::string body;TextInputSession multiline(body,InputType::Multiline);multiline.insertText("one");multiline.submit();multiline.insertText("two");CHECK(body=="one\ntwo");}
  std::cerr<<"[TEST] text focus and keyboard visibility\n";
  {std::string one,two;int blurred=0;TextInputSession first(one,InputType::Text,{}, {},[&]{++blurred;});TextInputSession second(two,InputType::Search,{}, {},[&]{++blurred;});TextInputManager manager;manager.focus(first,1);manager.update(.2);CHECK(manager.keyboardVisible()&&manager.isFocused(first));manager.focus(second,2);CHECK(blurred==1&&manager.isFocused(second));manager.hideKeyboard(true);manager.update(.2);CHECK(!manager.keyboardVisible()&&manager.focused()==nullptr&&blurred==2);manager.focus(first,100);manager.handlePhysicalText("x",100);CHECK(one=="x"&&!manager.keyboardTargetVisible());manager.setForceSoftwareKeyboard(true);manager.showKeyboard();CHECK(manager.keyboardTargetVisible());}
  std::cerr<<"[TEST] software keyboard shift/caps/symbols/repeat\n";
  {std::string value;TextInputSession session(value,InputType::Text);TextInputManager manager;manager.setForceSoftwareKeyboard(true);manager.focus(session,1);OnScreenKeyboard keyboard;keyboard.press("SHIFT",manager,100);keyboard.press("a",manager,150);CHECK(value=="A"&&keyboard.shiftState()==ShiftState::Inactive);keyboard.press("SHIFT",manager,1000);keyboard.press("SHIFT",manager,1200);CHECK(keyboard.shiftState()==ShiftState::CapsLock);keyboard.press("b",manager,1250);keyboard.press("c",manager,1300);CHECK(value=="ABC");keyboard.press("SHIFT",manager,1400);CHECK(keyboard.shiftState()==ShiftState::Inactive);keyboard.press("123",manager,1500);CHECK(keyboard.layout()==KeyboardLayout::Symbols);keyboard.press("1",manager,1550);keyboard.press("+",manager,1600);CHECK(value=="ABC1+");keyboard.press("ABC",manager,1700);CHECK(keyboard.layout()==KeyboardLayout::Alphabet);keyboard.beginBackspace(manager,2000);CHECK(value=="ABC1");keyboard.update(2449,manager);CHECK(value=="ABC1");keyboard.update(2450,manager);CHECK(value=="ABC");keyboard.update(2530,manager);CHECK(value=="AB");keyboard.endBackspace();keyboard.update(3000,manager);CHECK(value=="AB");}
  std::cerr<<"[TEST] input modes and autosave debounce\n";
  {
    std::string first="abc",second="xyz";TextInputSession a(first,InputType::Text),b(second,InputType::Text);
    TextInputManager manager;manager.setForceSoftwareKeyboard(true);OnScreenKeyboard keyboard;
    manager.focus(a,1);keyboard.beginBackspace(manager,10);manager.focus(b,20);keyboard.update(500,manager);
    CHECK(first=="ab"&&second=="xyz");
    keyboard.beginBackspace(manager,600);manager.hideKeyboard(false);keyboard.update(1100,manager);CHECK(second=="xy");
  }
  {std::string number;TextInputSession numeric(number,InputType::Number);numeric.insertText("12a-3.5");CHECK(number=="12-3.5");Debouncer debounce(750);debounce.mark(1000);CHECK(!debounce.ready(1749));CHECK(debounce.ready(1750));debounce.clear();CHECK(!debounce.dirty());}
  std::cerr<<"[TEST] application manager\n";
  {
    std::string value="old";int called=0;TextInputManager manager;
    TextInputSession reentrant(value,InputType::Text,{}, {},[&]{++called;manager.blur();});
    manager.focus(reentrant,1);manager.blur();CHECK(called==1&&manager.focused()==nullptr);
    std::unique_ptr<TextInputSession> prompt;
    prompt=std::make_unique<TextInputSession>(value,InputType::Text,TextInputSession::Callback{},[&]{prompt.reset();++called;});
    prompt->submit();CHECK(!prompt&&called==2);
    TextInputSession edited(value,InputType::Text);value.clear();edited.deleteBackward();CHECK(value.empty());
  }
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
