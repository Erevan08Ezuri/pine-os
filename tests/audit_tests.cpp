#include "core/Configuration.hpp"
#include "finance/FinanceService.hpp"
#include "finance/ImportExportManager.hpp"
#include "finance/FinanceSecurityManager.hpp"
#include "finance/Money.hpp"
#include "services/FileService.hpp"
#include "services/SecurityService.hpp"
#include "ui/Shell.hpp"
#include "ui/Font.hpp"
#include "core/Utf8.hpp"
#include <cmath>
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <functional>

using namespace Pine;
using namespace Pine::Finance;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action) { bool rejected=false; try { action(); } catch(const std::exception&) { rejected=true; } require(rejected,"operation should have been rejected"); }
int main() {
    const auto root=std::filesystem::temp_directory_path()/std::filesystem::path("pine-audit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    int failures=0;
    auto test=[&](const char* name, const std::function<void()>& work) {
        try { work(); std::cout<<"PASS "<<name<<'\n'; }
        catch(const std::exception& e) { ++failures; std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; }
    };
    test("storage root aliases cannot be removed, renamed or moved",[&] {
        FileService files(root/"sandbox"); files.initialize();
        require(!files.remove("."),"remove(.) erased the storage root");
        require(!files.remove("Documents/.."),"normalized root erased");
        require(!files.rename(".","escaped"),"storage root renamed outside sandbox");
        require(!files.move(".","Downloads/root"),"storage root moved");
        require(!files.copy("Documents","Documents/recursive-copy"),"directory copied recursively into itself");
        std::ofstream(files.resolve("Documents/a"))<<"old";
        std::ofstream(files.resolve("Documents/b"))<<"keep";
        require(!files.rename("Documents/a","b"),"rename overwrote existing file");
    });
    test("budget upsert returns persisted identity",[&] {
        FinanceService f(root/"budget"); auto a=f.setBudget("Food","2026-09",1000); auto b=f.setBudget("Food","2026-09",2000);
        require(a.id==b.id,"updated budget returned a phantom ID"); require(f.deleteBudget(b.id),"cannot delete updated budget");
    });
    test("financial totals never add yen minor units to dollar cents",[&] {
        FinanceService f(root/"currencies");auto usd=f.createAccount("USD","Cash","",10000,"USD");auto jpy=f.createAccount("JPY","Cash","",99999,"JPY");
        f.addTransaction(jpy.id,"Expense",999,"Shop","Other",FinanceService::today());
        require(f.dashboard().assets==10000&&f.dashboard().spendingMonth==0,"mixed currency totals combined");
        auto settings=f.settings();settings.defaultCurrency="JPY";f.saveSettings(settings);
        require(f.dashboard().assets==99000&&f.dashboard().spendingMonth==999,"selected currency totals incorrect");
    });
    test("UTF-8 text is measured as codepoints instead of individual bytes",[&] {
        const std::string text="\xC3\xA9\xF0\x9F\x8C\xB2";std::size_t pos=0;
        require(nextUtf8(text,pos)==0xe9&&pos==2,"accent decoding failed");
        require(nextUtf8(text,pos)==0x1f332&&pos==text.size(),"four-byte decoding failed");
        auto fonts=std::filesystem::path(SDL_GetBasePath())/"assets/fonts";
        require(initializeFonts(fonts/"Inter-Regular.ttf",fonts/"Inter-SemiBold.ttf"),"fonts unavailable");
        const bool correct=std::abs(textWidth("\xC3\xA9",3)-textWidth("e",3))<1;
        shutdownFonts();require(correct,"accent measured as two glyphs");
    });
    test("calendar and amount validation",[&] {
        FinanceService f(root/"validation"); auto a=f.createAccount("Cash","Cash","",1000);
        rejects([&]{f.addTransaction(a.id,"Expense",1,"","","2026-02-30");});
        rejects([&]{f.addBill("Bad",1,a.id,"Other","garbage","Monthly");});
        rejects([&]{f.setBudget("Food","2026-13",1);});
        rejects([&]{f.addGoal("Negative",-100);});
    });
    test("deleting transactions cannot overflow account balances",[&] {
        FinanceService f(root/"delete-overflow"); auto a=f.createAccount("Cash","Cash","",std::numeric_limits<std::int64_t>::max()-5);
        auto expense=f.addTransaction(a.id,"Expense",100,"","",FinanceService::today());
        f.addTransaction(a.id,"Income",100,"","",FinanceService::today());
        rejects([&]{f.deleteTransaction(expense.id);});
        require(f.transactions().size()==2,"failed reversal deleted a transaction");
    });
    test("downward reconciliation is atomic and records the final snapshot",[&] {
        FinanceService f(root/"reconcile"); auto a=f.createAccount("Cash","Cash","",std::numeric_limits<std::int64_t>::max()-5);
        auto t=f.reconcile(a.id,a.currentBalance-1000,FinanceService::today());
        require(t.amount==-1000,"wrong reconciliation delta");
        require(f.account(a.id)->currentBalance==a.currentBalance-1000,"wrong balance");
        require(f.balanceHistory(a.id).front().balance==a.currentBalance-1000,"snapshot records intermediate balance");
    });
    test("CSV duplicates within one statement are not booked twice",[&] {
        FinanceService f(root/"csv"); auto a=f.createAccount("Cash","Cash","",1000);
        FileService files(root/"csv-files"); files.initialize(); ImportExportManager io(f,files);
        std::ofstream(files.resolve("Documents/import.csv"))<<"date,merchant,amount\r\n2026-09-12,Shop,-1.00\r\n2026-09-12,Shop,-1.00\r\n2026-02-30,Bad,-1.00\r\n";
        auto preview=io.previewCsv("Documents/import.csv",a.id,{});
        require(!preview[2].valid,"invalid calendar date passed preview");
        auto r=io.importCsv("Documents/import.csv",a.id,{},true);
        require(r.imported==1&&r.duplicates==1&&r.invalid==1,"incorrect duplicate import counts");
        require(f.account(a.id)->currentBalance==900,"duplicate charged account twice");
    });
    test("import failure rolls back both ledger and balance",[&] {
        FinanceService f(root/"import-failure");auto a=f.createAccount("Cash","Cash","",1000);
        FileService files(root/"failure-files");files.initialize();ImportExportManager io(f,files);
        std::ofstream(files.resolve("Documents/import.csv"))<<"date,merchant,amount\n2026-09-12,Shop,-1.00\n";
        f.database().execute("CREATE TRIGGER fail_import BEFORE INSERT ON import_fingerprints BEGIN SELECT RAISE(ABORT,'simulated storage failure'); END;");
        const auto result=io.importCsv("Documents/import.csv",a.id,{},true);
        require(result.invalid==1&&result.imported==0,"failed import reported success");
        require(f.account(a.id)->currentBalance==1000&&f.transactions().empty(),"failed import left partial financial writes");
    });
    test("OFX IDs are scoped per account and duplicates skipped in same file",[&] {
        FinanceService f(root/"ofx");auto a=f.createAccount("One","Cash","",1000),b=f.createAccount("Two","Cash","",1000);
        FileService files(root/"ofx-files");files.initialize();ImportExportManager io(f,files);
        const std::string row="<STMTTRN><DTPOSTED>20260912<TRNAMT>-1.00<FITID>1<NAME>Shop</STMTTRN>";
        std::ofstream(files.resolve("Documents/import.ofx"))<<"<OFX>"<<row<<row<<"</OFX>";
        const auto one=io.importOfx("Documents/import.ofx",a.id,true),two=io.importOfx("Documents/import.ofx",b.id,true);
        require(one.imported==1&&one.duplicates==1&&two.imported==1&&two.duplicates==1,"OFX deduplication crossed account boundaries");
    });
    test("quoted multiline CSV preserves record boundaries",[&] {
        FinanceService f(root/"quoted");auto a=f.createAccount("Cash","Cash","",0);
        FileService files(root/"quoted-files");files.initialize();ImportExportManager io(f,files);CsvMapping mapping;mapping.category=3;
        std::ofstream(files.resolve("Documents/import.csv"))<<"date,merchant,amount,category\r\n2026-09-12,\"Shop,\\\"\"\nTwo\",-1.00,Other\r\n";
        auto rows=io.previewCsv("Documents/import.csv",a.id,mapping);
        require(rows.size()==1&&rows[0].valid&&rows[0].category=="Other","multiline CSV was split into broken records");
    });
    test("exact money subtraction handles signed limits",[&] {
        const auto min=std::numeric_limits<std::int64_t>::min();
        require((Money{min,{}}-Money{min,{}}).minor==0,"valid minimum subtraction rejected");
        rejects([&]{(void)(Money{0,{}}-Money{min,{}});});
        require(!parseMoneyMinor(".",{}),"bare decimal accepted as zero");
    });
    test("newer Finance database is preserved",[&] {
        {FinanceService f(root/"newer");f.createAccount("Keep","Cash","",1000);f.database().execute("PRAGMA user_version=99");}
        {FinanceDatabase f(root/"newer");require(f.memoryFallback(),"newer schema was opened for incompatible writes");}
        sqlite3* raw{};auto path=(root/"newer/finance/finance.db").string();sqlite3_open(path.c_str(),&raw);
        sqlite3_stmt* stmt{};sqlite3_prepare_v2(raw,"SELECT name FROM accounts",-1,&stmt,nullptr);
        require(sqlite3_step(stmt)==SQLITE_ROW,"newer database contents lost");sqlite3_finalize(stmt);sqlite3_close(raw);
    });
    test("unrelated SQLite backup cannot replace Finance",[&] {
        FinanceService f(root/"restore"); auto a=f.createAccount("Keep","Cash","",1000);
        FileService files(root/"restore-files"); files.initialize(); ImportExportManager io(f,files);
        sqlite3* db{}; auto path=files.resolve("Documents/unrelated.db").string(); sqlite3_open(path.c_str(),&db);
        sqlite3_exec(db,"CREATE TABLE unrelated(x);",nullptr,nullptr,nullptr); sqlite3_close(db);
        rejects([&]{io.restoreBackup("Documents/unrelated.db",true);});
        require(f.account(a.id)->currentBalance==1000,"restore destroyed original data");
    });
    test("foreground Finance stays unlocked until background timeout",[&] {
        SecurityService s(root/"lock"); s.setDevicePin("4826"); FinanceSecurityManager lock(s); FinanceSettings settings;
        settings.requireAuthentication=true; settings.autoLock="Immediately"; lock.configure(settings); lock.onOpen(100);
        require(lock.unlock("4826",101),"unlock failed"); lock.update(102); require(!lock.locked(),"relocked on next foreground update");
        lock.onBackground(103); require(lock.locked(),"immediate background lock failed");
    });
    test("settings backup recovers interrupted replacement",[&] {
        Configuration c(root/"config"); c.load(); c.settings().volume=37; c.save();
        std::filesystem::rename(root/"config/settings.json",root/"config/settings.json.bak");
        Configuration recovered(root/"config"); recovered.load(); require(recovered.settings().volume==37,"lost last committed settings");
    });
    test("malformed PIN digest fails authentication safely",[&] {
        std::filesystem::create_directories(root/"bad-pin");
        std::ofstream(root/"bad-pin/security.json")<<R"({"salt":"abcd","pin_hash":"x"})";
        SecurityService s(root/"bad-pin"); require(!s.authenticate("4826"),"malformed PIN accepted");
    });
    test("modal prompts block underlying app buttons and cancel on Home",[&] {
        require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization failed");
        auto* surface=SDL_CreateSurface(720,1280,SDL_PIXELFORMAT_RGB565);
        auto* renderer=SDL_CreateSoftwareRenderer(surface);
        require(renderer!=nullptr,"software renderer unavailable");
        {
            Configuration config(root/"modal");config.load();
            Shell shell(nullptr,renderer,config,createPlatform("desktop"));shell.acceptanceSkipBoot();
            while(shell.deviceLock().busy()){SDL_Delay(5);shell.update(.005);}
            require(shell.deviceLock().unlock("123456",SDL_GetTicks()),"device unlock request failed");
            while(shell.deviceLock().busy()){SDL_Delay(5);shell.update(.005);}
            require(!shell.deviceLock().locked(),"device still locked");
            bool submitted=false;shell.openTextPrompt("MODAL","test",[&](const std::string&){submitted=true;});
            SDL_Event click{};click.type=SDL_EVENT_MOUSE_BUTTON_UP;click.button.button=SDL_BUTTON_LEFT;
            click.button.x=150;click.button.y=425;shell.handleEvent(click);shell.render();
            require(shell.apps().current()==nullptr,"click passed through prompt and launched app");
            shell.goHome();require(shell.textInput().focused()==nullptr,"prompt kept focus after Home");
            shell.render();require(!submitted,"navigation submitted abandoned prompt");
        }
        SDL_DestroyRenderer(renderer);SDL_DestroySurface(surface);SDL_Quit();
    });
    std::filesystem::remove_all(root);
    return failures?1:0;
}
