#include "finance/FinanceDatabase.hpp"
#include "core/Logger.hpp"
#include <sqlite3.h>
#include <chrono>
#include <format>
#include <stdexcept>

namespace Pine::Finance {
namespace {
#ifdef PINE_TAB5
constexpr auto storagePragmas="PRAGMA foreign_keys=ON; PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-512;";
#else
constexpr auto storagePragmas="PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;";
#endif
}

namespace {void check(int code,sqlite3*db,const char*context){if(code!=SQLITE_OK&&code!=SQLITE_DONE&&code!=SQLITE_ROW)throw std::runtime_error(std::string(context)+": "+sqlite3_errmsg(db));}}
FinanceDatabase::FinanceDatabase(std::filesystem::path root){try{std::filesystem::create_directories(root/"finance");path_=root/"finance"/"finance.db";const auto opened=sqlite3_open_v2(path_.string().c_str(),&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,nullptr);if(opened!=SQLITE_OK){const std::string reason=db_?sqlite3_errmsg(db_):"storage unavailable";if(db_)sqlite3_close(db_);db_=nullptr;openMemoryFallback(reason);return;}sqlite3_busy_timeout(db_,5000);try{execute(storagePragmas);migrate();}catch(const std::exception&e){sqlite3_close(db_);db_=nullptr;auto recovery=path_;recovery+=std::format(".recovery-{}",std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());std::error_code ec;std::filesystem::rename(path_,recovery,ec);if(ec){openMemoryFallback(std::string("Database corrupt; original preserved: ")+e.what());return;}warning_="Damaged database preserved as "+recovery.filename().string();check(sqlite3_open_v2(path_.string().c_str(),&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,nullptr),db_,"Create recovery database");sqlite3_busy_timeout(db_,5000);execute(storagePragmas);migrate();Logger::instance().error("FINANCE",warning_);}}catch(const std::exception&e){if(db_)sqlite3_close(db_);db_=nullptr;openMemoryFallback(e.what());}}
void FinanceDatabase::openMemoryFallback(const std::string&reason){if(db_)sqlite3_close(db_);db_=nullptr;check(sqlite3_open_v2(":memory:",&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,nullptr),db_,"Open emergency Finance database");memoryFallback_=true;warning_="Finance storage unavailable; changes are temporarily in memory: "+reason;sqlite3_busy_timeout(db_,5000);execute("PRAGMA foreign_keys=ON");migrate();Logger::instance().error("FINANCE",warning_);}
FinanceDatabase::~FinanceDatabase(){if(db_)sqlite3_close(db_);}
void FinanceDatabase::execute(const std::string&sql){char*error=nullptr;const auto code=sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,&error);if(code!=SQLITE_OK){std::string message=error?error:sqlite3_errmsg(db_);sqlite3_free(error);throw std::runtime_error(message);}}
void FinanceDatabase::transaction(const std::function<void()>&work){std::scoped_lock lock(mutex_);execute("BEGIN IMMEDIATE");try{work();execute("COMMIT");}catch(...){try{execute("ROLLBACK");}catch(...){}throw;}}
int FinanceDatabase::schemaVersion()const{sqlite3_stmt*stmt{};check(sqlite3_prepare_v2(db_,"PRAGMA user_version",-1,&stmt,nullptr),db_,"Read schema");const int v=sqlite3_step(stmt)==SQLITE_ROW?sqlite3_column_int(stmt,0):0;sqlite3_finalize(stmt);return v;}
void FinanceDatabase::migrate(){std::scoped_lock lock(mutex_);if(schemaVersion()>=1)return;execute(R"SQL(
BEGIN IMMEDIATE;
CREATE TABLE accounts(id TEXT PRIMARY KEY,name TEXT NOT NULL,type TEXT NOT NULL,institution_name TEXT NOT NULL DEFAULT '',current_balance INTEGER NOT NULL,available_balance INTEGER NOT NULL,currency TEXT NOT NULL,color_or_icon TEXT NOT NULL DEFAULT 'gold',created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL,archived INTEGER NOT NULL DEFAULT 0,sync_provider TEXT NOT NULL DEFAULT 'manual',sync_status TEXT NOT NULL DEFAULT 'Manual',last_sync_time INTEGER NOT NULL DEFAULT 0,credit_limit INTEGER NOT NULL DEFAULT 0,statement_balance INTEGER NOT NULL DEFAULT 0,minimum_payment INTEGER NOT NULL DEFAULT 0,apr_basis_points INTEGER NOT NULL DEFAULT 0,payment_due_date TEXT NOT NULL DEFAULT '');
CREATE TABLE categories(id TEXT PRIMARY KEY,name TEXT NOT NULL UNIQUE,icon TEXT NOT NULL,built_in INTEGER NOT NULL,hidden INTEGER NOT NULL DEFAULT 0);
CREATE TABLE transactions(id TEXT PRIMARY KEY,account_id TEXT NOT NULL REFERENCES accounts(id),type TEXT NOT NULL,amount INTEGER NOT NULL,currency TEXT NOT NULL,merchant TEXT NOT NULL DEFAULT '',original_description TEXT NOT NULL DEFAULT '',description TEXT NOT NULL DEFAULT '',category TEXT NOT NULL DEFAULT 'Other',date TEXT NOT NULL,created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL,status TEXT NOT NULL DEFAULT 'Posted',notes TEXT NOT NULL DEFAULT '',transfer_id TEXT NOT NULL DEFAULT '',recurring_id TEXT NOT NULL DEFAULT '',provider_transaction_id TEXT NOT NULL DEFAULT '');
CREATE TABLE budgets(id TEXT PRIMARY KEY,category TEXT NOT NULL,month TEXT NOT NULL,limit_minor INTEGER NOT NULL,rollover INTEGER NOT NULL DEFAULT 0,warning_percent INTEGER NOT NULL DEFAULT 80,UNIQUE(category,month));
CREATE TABLE bills(id TEXT PRIMARY KEY,name TEXT NOT NULL,amount_minor INTEGER NOT NULL,account_id TEXT NOT NULL DEFAULT '',category TEXT NOT NULL,next_due_date TEXT NOT NULL,frequency TEXT NOT NULL,autopay INTEGER NOT NULL DEFAULT 0,reminder_enabled INTEGER NOT NULL DEFAULT 1,reminder_days INTEGER NOT NULL DEFAULT 3,notes TEXT NOT NULL DEFAULT '');
CREATE TABLE subscriptions(id TEXT PRIMARY KEY,name TEXT NOT NULL,price_minor INTEGER NOT NULL,frequency TEXT NOT NULL,account_id TEXT NOT NULL DEFAULT '',next_charge TEXT NOT NULL,category TEXT NOT NULL,active INTEGER NOT NULL DEFAULT 1);
CREATE TABLE goals(id TEXT PRIMARY KEY,name TEXT NOT NULL,target_amount INTEGER NOT NULL,current_amount INTEGER NOT NULL,target_date TEXT NOT NULL DEFAULT '',linked_account_id TEXT NOT NULL DEFAULT '',created_at INTEGER NOT NULL,completed INTEGER NOT NULL DEFAULT 0);
CREATE TABLE goal_contributions(id TEXT PRIMARY KEY,goal_id TEXT NOT NULL REFERENCES goals(id) ON DELETE CASCADE,amount_minor INTEGER NOT NULL,created_at INTEGER NOT NULL);
CREATE TABLE balance_snapshots(id INTEGER PRIMARY KEY AUTOINCREMENT,account_id TEXT NOT NULL REFERENCES accounts(id),balance INTEGER NOT NULL,timestamp INTEGER NOT NULL);
CREATE TABLE finance_settings(key TEXT PRIMARY KEY,value TEXT NOT NULL);
CREATE TABLE import_fingerprints(fingerprint TEXT PRIMARY KEY,created_at INTEGER NOT NULL);
CREATE TABLE sync_connections(id TEXT PRIMARY KEY,provider TEXT NOT NULL,state TEXT NOT NULL,last_sync INTEGER NOT NULL DEFAULT 0,token_reference TEXT NOT NULL DEFAULT '');
CREATE INDEX idx_transactions_account_date ON transactions(account_id,date DESC);
CREATE INDEX idx_transactions_date ON transactions(date DESC);
CREATE INDEX idx_transactions_category ON transactions(category);
CREATE INDEX idx_transactions_merchant ON transactions(merchant);
CREATE INDEX idx_transactions_transfer ON transactions(transfer_id);
CREATE UNIQUE INDEX idx_transactions_provider_id ON transactions(provider_transaction_id) WHERE provider_transaction_id<>'';
CREATE INDEX idx_bills_due ON bills(next_due_date);
CREATE INDEX idx_subscriptions_charge ON subscriptions(next_charge);
CREATE INDEX idx_snapshots_account_time ON balance_snapshots(account_id,timestamp DESC);
PRAGMA user_version=1;
COMMIT;
)SQL");Logger::instance().info("FINANCE","Database schema initialized");}
}
