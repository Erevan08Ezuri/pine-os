#include "finance/ImportExportManager.hpp"
#include "finance/FinanceService.hpp"
#include "finance/Money.hpp"
#include "core/Date.hpp"
#include <limits>
#include <memory>
#include "services/FileService.hpp"
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <fstream>
#include <format>
#include <sstream>
#include <stdexcept>

namespace Pine::Finance {namespace {
bool csvRecord(std::istream& in,std::string& record) {
    record.clear();std::string line;bool quoted=false,read=false;
    while(std::getline(in,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(read)record+='\n';
        read=true;record+=line;
        for(std::size_t i=0;i<line.size();++i)if(line[i]=='"'){
            if(quoted&&i+1<line.size()&&line[i+1]=='"')++i;else quoted=!quoted;
        }
        if(!quoted)return true;
    }
    if(quoted)throw std::invalid_argument("Unterminated quoted CSV field");
    return read;
}
std::vector<std::string>csvLine(const std::string&line){std::vector<std::string>out;std::string cell;bool quoted=false;for(std::size_t i=0;i<line.size();++i){char c=line[i];if(c=='"'){if(quoted&&i+1<line.size()&&line[i+1]=='"'){cell.push_back('"');++i;}else quoted=!quoted;}else if(c==','&&!quoted){out.push_back(cell);cell.clear();}else cell.push_back(c);}out.push_back(cell);return out;}std::string quote(std::string v){std::string out="\"";for(char c:v){if(c=='\"')out+='\"';out+=c;}return out+'\"';}std::string fingerprint(const std::string&account,const ImportPreviewRow&r){return account+"|"+r.date+"|"+std::to_string(r.amount)+"|"+r.merchant;}bool validDate(const std::string& date){try{Pine::parseIsoDate(date);return true;}catch(...){return false;}}bool knownFingerprint(sqlite3*db,const std::string&value){sqlite3_stmt*s{};sqlite3_prepare_v2(db,"SELECT 1 FROM import_fingerprints WHERE fingerprint=?",-1,&s,nullptr);sqlite3_bind_text(s,1,value.c_str(),-1,SQLITE_TRANSIENT);const bool found=sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);return found;}nlohmann::json accountJson(const Account&a){return{{"id",a.id},{"name",a.name},{"type",a.type},{"institution_name",a.institution},{"current_balance_minor",a.currentBalance},{"available_balance_minor",a.availableBalance},{"currency",a.currency},{"archived",a.archived},{"sync_provider",a.syncProvider}};}nlohmann::json transactionJson(const Transaction&t){return{{"id",t.id},{"account_id",t.accountId},{"type",t.type},{"amount_minor",t.amount},{"currency",t.currency},{"merchant",t.merchant},{"original_description",t.originalDescription},{"description",t.description},{"category",t.category},{"date",t.date},{"status",t.status},{"notes",t.notes},{"transfer_id",t.transferId}};}}
ImportExportManager::ImportExportManager(FinanceService&s,FileService&f):service_(s),files_(f){}
namespace {std::string ofxTag(const std::string&block,const std::string&tag){const auto marker="<"+tag+">";auto start=block.find(marker);if(start==std::string::npos)return{};start+=marker.size();auto end=block.find_first_of("<\r\n",start);return block.substr(start,end==std::string::npos?std::string::npos:end-start);}bool knownProvider(sqlite3*db,const std::string&id,const std::string&account){if(id.empty())return false;sqlite3_stmt*s{};sqlite3_prepare_v2(db,"SELECT 1 FROM transactions WHERE account_id=? AND provider_transaction_id IN(?,?)",-1,&s,nullptr);sqlite3_bind_text(s,1,account.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,id.c_str(),-1,SQLITE_TRANSIENT);const auto scoped=account+"|"+id;sqlite3_bind_text(s,3,scoped.c_str(),-1,SQLITE_TRANSIENT);const bool found=sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);return found;}}
std::vector<ImportPreviewRow>ImportExportManager::previewCsv(const std::filesystem::path&relative,const std::string&accountId,const CsvMapping&m,std::size_t maxRows)const{auto account=service_.account(accountId);if(!account)throw std::invalid_argument("Import account not found");std::ifstream in(files_.resolve(relative));if(!in)throw std::runtime_error("Unable to open CSV");std::vector<ImportPreviewRow>out;std::string line;if(m.firstRowHeader)csvRecord(in,line);while(out.size()<maxRows&&csvRecord(in,line)){auto cells=csvLine(line);ImportPreviewRow row;const auto required=std::max({m.date,m.description,m.amount,m.category.value_or(0)});if(cells.size()<=required){row.error="Missing mapped column";out.push_back(row);continue;}row.date=cells[m.date];row.merchant=cells[m.description];row.category=m.category?cells[*m.category]:"Other";auto parsed=parseMoneyMinor(cells[m.amount],Currency::fromCode(account->currency));if(!parsed||*parsed==0||*parsed==std::numeric_limits<std::int64_t>::min()){row.error="Invalid amount";out.push_back(row);continue;}row.amount=*parsed;row.valid=validDate(row.date);if(!row.valid)row.error="Date must be YYYY-MM-DD";else row.duplicate=knownFingerprint(service_.database().handle(),fingerprint(accountId,row));out.push_back(std::move(row));}return out;}
namespace {
struct SqlStatement {
    sqlite3_stmt* stmt{}; sqlite3* db;
    SqlStatement(sqlite3* d,const char* sql):db(d) { if(sqlite3_prepare_v2(d,sql,-1,&stmt,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(d)); }
    ~SqlStatement(){sqlite3_finalize(stmt);}
    void text(int index,const std::string& value){if(sqlite3_bind_text(stmt,index,value.c_str(),-1,SQLITE_TRANSIENT)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));}
    void done(){if(sqlite3_step(stmt)!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db));}
};
void rememberFingerprint(sqlite3* db,const std::string& fp) {
    SqlStatement s(db,"INSERT INTO import_fingerprints(fingerprint,created_at)VALUES(?,strftime('%s','now')*1000)");s.text(1,fp);s.done();
}
}
ImportResult ImportExportManager::importCsv(const std::filesystem::path& path,const std::string& accountId,const CsvMapping& mapping,bool confirmed) {
    if(!confirmed)throw std::invalid_argument("Import must be confirmed after preview");
    ImportResult result;
    for(const auto& row:previewCsv(path,accountId,mapping,100000)) {
        if(!row.valid){++result.invalid;if(result.errors.size()<20)result.errors.push_back(row.error);continue;}
        try {
            bool duplicate=false;
            service_.database().transaction([&] {
                auto* db=service_.database().handle(); auto fp=fingerprint(accountId,row);
                if(knownFingerprint(db,fp)){duplicate=true;return;}
                service_.addTransaction(accountId,row.amount<0?"Expense":"Income",std::llabs(row.amount),row.merchant,row.category,row.date);
                rememberFingerprint(db,fp);
            });
            if(duplicate)++result.duplicates;else ++result.imported;
        } catch(const std::exception& e){++result.invalid;if(result.errors.size()<20)result.errors.push_back(e.what());}
    }
    return result;
}
std::vector<ImportPreviewRow>ImportExportManager::previewOfx(const std::filesystem::path&relative,const std::string&accountId,std::size_t maxRows)const{auto account=service_.account(accountId);if(!account)throw std::invalid_argument("Import account not found");std::ifstream in(files_.resolve(relative),std::ios::binary);if(!in)throw std::runtime_error("Unable to open OFX/QFX file");std::string content((std::istreambuf_iterator<char>(in)),{});std::vector<ImportPreviewRow>out;std::size_t position=0;while(out.size()<maxRows&&(position=content.find("<STMTTRN>",position))!=std::string::npos){auto end=content.find("</STMTTRN>",position);if(end==std::string::npos)end=content.find("<STMTTRN>",position+10);auto block=content.substr(position,end==std::string::npos?std::string::npos:end-position);position=end==std::string::npos?content.size():end+10;ImportPreviewRow row;auto rawDate=ofxTag(block,"DTPOSTED");if(rawDate.size()>=8)row.date=rawDate.substr(0,4)+"-"+rawDate.substr(4,2)+"-"+rawDate.substr(6,2);row.merchant=ofxTag(block,"NAME");if(row.merchant.empty())row.merchant=ofxTag(block,"MEMO");row.category="Other";row.providerId=ofxTag(block,"FITID");auto amount=parseMoneyMinor(ofxTag(block,"TRNAMT"),Currency::fromCode(account->currency));if(!amount||*amount==0||*amount==std::numeric_limits<std::int64_t>::min())row.error="Invalid OFX amount";else if(!validDate(row.date))row.error="Invalid OFX date";else{row.amount=*amount;row.valid=true;row.duplicate=knownProvider(service_.database().handle(),row.providerId,accountId)||knownFingerprint(service_.database().handle(),fingerprint(accountId,row));}out.push_back(std::move(row));}if(out.empty())throw std::runtime_error("No statement transactions found");return out;}
ImportResult ImportExportManager::importOfx(const std::filesystem::path& path,const std::string& accountId,bool confirmed) {
    if(!confirmed)throw std::invalid_argument("Import must be confirmed after preview");
    ImportResult result;
    for(const auto& row:previewOfx(path,accountId,100000)) {
        if(!row.valid){++result.invalid;continue;}
        try {
            bool duplicate=false;
            service_.database().transaction([&] {
                auto* db=service_.database().handle();const auto fp=fingerprint(accountId,row);
                if(knownProvider(db,row.providerId,accountId)||knownFingerprint(db,fp)){duplicate=true;return;}
                auto t=service_.addTransaction(accountId,row.amount<0?"Expense":"Income",std::llabs(row.amount),row.merchant,"Other",row.date);
                SqlStatement update(db,"UPDATE transactions SET provider_transaction_id=? WHERE id=?");
                update.text(1,row.providerId.empty()?"":accountId+"|"+row.providerId);update.text(2,t.id);update.done();rememberFingerprint(db,fp);
            });
            if(duplicate)++result.duplicates;else ++result.imported;
        } catch(const std::exception& e){++result.invalid;if(result.errors.size()<20)result.errors.push_back(e.what());}
    }
    return result;
}
std::filesystem::path ImportExportManager::exportTransactionsCsv(const std::filesystem::path&relative)const{auto path=files_.resolve(relative);std::filesystem::create_directories(path.parent_path());std::ofstream out(path);if(!out)throw std::runtime_error("Unable to create CSV export");out<<"date,merchant,description,category,type,amount_minor,currency,account_id,status,notes\n";std::size_t offset=0;while(true){auto page=service_.transactions("","","","","","","",{}, {},1000,offset);for(auto&t:page)out<<quote(t.date)<<','<<quote(t.merchant)<<','<<quote(t.description)<<','<<quote(t.category)<<','<<quote(t.type)<<','<<t.amount<<','<<quote(t.currency)<<','<<quote(t.accountId)<<','<<quote(t.status)<<','<<quote(t.notes)<<'\n';if(page.size()<1000)break;offset+=page.size();}return path;}
std::filesystem::path ImportExportManager::exportEverythingJson(const std::filesystem::path&relative)const{auto path=files_.resolve(relative);std::filesystem::create_directories(path.parent_path());nlohmann::json root{{"format","pine-finance-export"},{"version",1},{"accounts",nlohmann::json::array()},{"transactions",nlohmann::json::array()},{"budgets",nlohmann::json::array()},{"bills",nlohmann::json::array()},{"subscriptions",nlohmann::json::array()},{"goals",nlohmann::json::array()}};for(auto&a:service_.accounts(true))root["accounts"].push_back(accountJson(a));std::size_t offset=0;while(true){auto page=service_.transactions("","","","","","","",{}, {},1000,offset);for(auto&t:page)root["transactions"].push_back(transactionJson(t));if(page.size()<1000)break;offset+=page.size();}for(auto&b:service_.budgets())root["budgets"].push_back({{"id",b.id},{"category",b.category},{"month",b.month},{"limit_minor",b.limit},{"rollover",b.rollover}});for(auto&b:service_.bills())root["bills"].push_back({{"name",b.name},{"amount_minor",b.amount},{"next_due_date",b.nextDueDate},{"frequency",b.frequency}});for(auto&s:service_.subscriptions(false))root["subscriptions"].push_back({{"name",s.name},{"price_minor",s.price},{"frequency",s.frequency},{"next_charge",s.nextCharge},{"active",s.active}});for(auto&g:service_.goals(true))root["goals"].push_back({{"name",g.name},{"target_amount",g.targetAmount},{"current_amount",g.currentAmount},{"target_date",g.targetDate},{"completed",g.completed}});std::ofstream(path)<<root.dump(2);return path;}
namespace {
using DatabasePtr=std::unique_ptr<sqlite3,decltype(&sqlite3_close)>;
DatabasePtr openBackup(const std::filesystem::path& path,int flags) {
    sqlite3* raw{};const int code=sqlite3_open_v2(path.string().c_str(),&raw,flags,nullptr);
    DatabasePtr db(raw,sqlite3_close);
    if(code!=SQLITE_OK)throw std::runtime_error("Unable to open backup");
    return db;
}
void copyDatabase(sqlite3* destination,sqlite3* source) {
    auto* backup=sqlite3_backup_init(destination,"main",source,"main");
    if(!backup)throw std::runtime_error("Unable to initialize database copy");
    const int step=sqlite3_backup_step(backup,-1);
    const int finish=sqlite3_backup_finish(backup);
    if(step!=SQLITE_DONE||finish!=SQLITE_OK)throw std::runtime_error("Database copy did not complete");
}
std::vector<std::string> schema(sqlite3* db) {
    SqlStatement s(db,"SELECT type,name,sql FROM sqlite_master WHERE name NOT LIKE 'sqlite_%' ORDER BY type,name");
    std::vector<std::string> out;int rc;
    while((rc=sqlite3_step(s.stmt))==SQLITE_ROW)for(int i=0;i<3;++i){auto value=sqlite3_column_text(s.stmt,i);out.emplace_back(value?reinterpret_cast<const char*>(value):"");}
    if(rc!=SQLITE_DONE)throw std::runtime_error("Cannot read backup schema");
    return out;
}
}
std::filesystem::path ImportExportManager::createBackup(const std::filesystem::path& relative)const {
    std::scoped_lock lock(const_cast<FinanceDatabase&>(service_.database()).mutex());
    auto path=files_.resolve(relative);std::filesystem::create_directories(path.parent_path());
    auto destination=openBackup(path,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
    copyDatabase(destination.get(),service_.database().handle());return path;
}
bool ImportExportManager::restoreBackup(const std::filesystem::path& relative,bool confirmed) {
    if(!confirmed)throw std::invalid_argument("Restore must be explicitly confirmed");
    std::scoped_lock lock(service_.database().mutex());
    auto source=openBackup(files_.resolve(relative),SQLITE_OPEN_READONLY);
    SqlStatement check(source.get(),"PRAGMA integrity_check");
    if(sqlite3_step(check.stmt)!=SQLITE_ROW||std::string(reinterpret_cast<const char*>(sqlite3_column_text(check.stmt,0)))!="ok")
        throw std::runtime_error("Backup integrity check failed");
    if(schema(source.get())!=schema(service_.database().handle()))
        throw std::runtime_error("Backup is not a compatible Pine Finance database");
    SqlStatement version(source.get(),"PRAGMA user_version");
    if(sqlite3_step(version.stmt)!=SQLITE_ROW||sqlite3_column_int(version.stmt,0)!=service_.database().schemaVersion())
        throw std::runtime_error("Backup schema version is not supported");
    SqlStatement keys(source.get(),"PRAGMA foreign_key_check");
    if(sqlite3_step(keys.stmt)!=SQLITE_DONE)throw std::runtime_error("Backup contains broken account references");
    copyDatabase(service_.database().handle(),source.get());return true;
}
}
