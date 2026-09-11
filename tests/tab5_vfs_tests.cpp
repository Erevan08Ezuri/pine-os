#include "sqlite3.h"
#include "core/Date.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void sql(sqlite3* db,const char* text){if(sqlite3_exec(db,text,nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));}
int main(){
    auto root=std::filesystem::temp_directory_path()/("pine-vfs-"+std::to_string(getpid()));
    std::filesystem::create_directories(root);auto path=(root/"finance.db").string();
    sqlite3 *a{},*b{};require(sqlite3_open(path.c_str(),&a)==SQLITE_OK,"open");
    sql(a,"PRAGMA journal_mode=DELETE;PRAGMA synchronous=FULL;CREATE TABLE t(v INTEGER);INSERT INTO t VALUES(41);");
    sql(a,"BEGIN;UPDATE t SET v=99;ROLLBACK;");
    require(sqlite3_open(path.c_str(),&b)==SQLITE_OK,"second connection");
    sql(a,"BEGIN IMMEDIATE;");
    require(sqlite3_exec(b,"BEGIN IMMEDIATE;",nullptr,nullptr,nullptr)==SQLITE_BUSY,"writer lock");
    sql(a,"ROLLBACK;");
    sql(b,"BEGIN;SELECT * FROM t;");
    sql(a,"BEGIN IMMEDIATE;UPDATE t SET v=42;");
    require(sqlite3_exec(a,"COMMIT;",nullptr,nullptr,nullptr)==SQLITE_BUSY,"reader blocks commit");
    sql(b,"COMMIT;");sql(a,"COMMIT;");sqlite3_close(b);sqlite3_close(a);
    require(sqlite3_open(path.c_str(),&a)==SQLITE_OK,"reopen");
    sqlite3_stmt* stmt{};sqlite3_prepare_v2(a,"SELECT v FROM t",-1,&stmt,nullptr);
    require(sqlite3_step(stmt)==SQLITE_ROW&&sqlite3_column_int(stmt,0)==42,"persistence/rollback");
    sqlite3_finalize(stmt);sqlite3_close(a);
    require((Pine::parseIsoDate("2024-03-01")-Pine::parseIsoDate("2024-02-28")).count()==2,"leap day");
    bool invalid=false;try{Pine::parseIsoDate("2025-02-29");}catch(const std::invalid_argument&){invalid=true;}
    require(invalid,"invalid date rejected");std::filesystem::remove_all(root);
    std::cout<<"Tab5 storage and date tests passed\n";
}
