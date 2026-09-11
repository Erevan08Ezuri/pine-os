// SQLite VFS for one firmware process. Connection locks are shared in-process.
// Persistent rollback journals + fsync; no WAL, mmap, or inter-process locking.
#include "sqlite3.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include <random>
#include <thread>
#include <chrono>
#endif
namespace {
struct File { sqlite3_file base; int fd,lock; bool removeOnClose; char name[512]; };
std::mutex mutex;
std::map<std::string,std::map<File*,int>> locks;
File* file(sqlite3_file* f){return reinterpret_cast<File*>(f);}
int unlock(sqlite3_file* f,int level) {
    auto* p=file(f);std::lock_guard guard(mutex);
    auto found=locks.find(p->name);
    if(found!=locks.end()){
        if(level==SQLITE_LOCK_NONE)found->second.erase(p);else found->second[p]=level;
        if(found->second.empty())locks.erase(found);
    }
    p->lock=level;return SQLITE_OK;
}
int closeFile(sqlite3_file* f){
    auto* p=file(f);unlock(f,SQLITE_LOCK_NONE);int rc=::close(p->fd);
    if(p->removeOnClose&&::unlink(p->name)!=0&&errno!=ENOENT)rc=-1;
    p->base.pMethods=nullptr;return rc==0?SQLITE_OK:SQLITE_IOERR_CLOSE;
}
int readFile(sqlite3_file* f,void* data,int bytes,sqlite3_int64 offset){
    if(lseek(file(f)->fd,offset,SEEK_SET)<0)return SQLITE_IOERR_SEEK;
    int done=0;
    while(done<bytes){
        auto n=::read(file(f)->fd,static_cast<char*>(data)+done,bytes-done);
        if(n<0){if(errno==EINTR)continue;return SQLITE_IOERR_READ;}
        if(n==0){std::memset(static_cast<char*>(data)+done,0,bytes-done);return SQLITE_IOERR_SHORT_READ;}
        done+=n;
    }
    return SQLITE_OK;
}
int writeFile(sqlite3_file* f,const void* data,int bytes,sqlite3_int64 offset){
    if(lseek(file(f)->fd,offset,SEEK_SET)<0)return SQLITE_IOERR_SEEK;
    int done=0;
    while(done<bytes){
        auto n=::write(file(f)->fd,static_cast<const char*>(data)+done,bytes-done);
        if(n<0&&errno==EINTR)continue;
        if(n<=0)return errno==ENOSPC?SQLITE_FULL:SQLITE_IOERR_WRITE;
        done+=n;
    }
    return SQLITE_OK;
}
int truncateFile(sqlite3_file* f,sqlite3_int64 size){return ftruncate(file(f)->fd,size)==0?SQLITE_OK:SQLITE_IOERR_TRUNCATE;}
int syncFile(sqlite3_file* f,int){return fsync(file(f)->fd)==0?SQLITE_OK:SQLITE_IOERR_FSYNC;}
int fileSize(sqlite3_file* f,sqlite3_int64* size){
    struct stat st{};if(fstat(file(f)->fd,&st))return SQLITE_IOERR_FSTAT;*size=st.st_size;return SQLITE_OK;
}
int lockFile(sqlite3_file* f,int requested){
    auto* p=file(f);std::lock_guard guard(mutex);
    if(requested<=p->lock)return SQLITE_OK;
    auto& group=locks[p->name];
    for(auto [other,level]:group){
        if(other==p)continue;
        if((requested==SQLITE_LOCK_SHARED&&level>=SQLITE_LOCK_PENDING)||
           (requested==SQLITE_LOCK_RESERVED&&level>=SQLITE_LOCK_RESERVED)||
           (requested>=SQLITE_LOCK_PENDING&&level>=SQLITE_LOCK_RESERVED))return SQLITE_BUSY;
    }
    if(requested==SQLITE_LOCK_EXCLUSIVE){
        group[p]=p->lock=SQLITE_LOCK_PENDING;
        for(auto [other,level]:group)if(other!=p&&level>=SQLITE_LOCK_SHARED)return SQLITE_BUSY;
    }
    group[p]=p->lock=requested;return SQLITE_OK;
}
int reserved(sqlite3_file* f,int* out){
    std::lock_guard guard(mutex);*out=0;
    auto group=locks.find(file(f)->name);
    if(group!=locks.end())for(auto [p,level]:group->second)if(level>=SQLITE_LOCK_RESERVED)*out=1;
    return SQLITE_OK;
}
int control(sqlite3_file* f,int op,void* arg){
    if(op==SQLITE_FCNTL_LOCKSTATE){*static_cast<int*>(arg)=file(f)->lock;return SQLITE_OK;}return SQLITE_NOTFOUND;
}
int sector(sqlite3_file*){return 4096;}
int characteristics(sqlite3_file*){return 0;}
const sqlite3_io_methods methods={1,closeFile,readFile,writeFile,truncateFile,syncFile,fileSize,lockFile,unlock,reserved,control,sector,characteristics};
int openFile(sqlite3_vfs*,const char* name,sqlite3_file* out,int flags,int* actual){
    if(!name||std::strlen(name)>=512)return SQLITE_CANTOPEN;
    auto* p=file(out);std::memset(p,0,sizeof(*p));p->fd=-1;
    int access=(flags&SQLITE_OPEN_READONLY)?O_RDONLY:O_RDWR;
    if(flags&SQLITE_OPEN_CREATE)access|=O_CREAT;
    if(flags&SQLITE_OPEN_EXCLUSIVE)access|=O_EXCL;
    p->fd=::open(name,access,0600);if(p->fd<0)return SQLITE_CANTOPEN;
    std::strcpy(p->name,name);p->removeOnClose=flags&SQLITE_OPEN_DELETEONCLOSE;
    p->base.pMethods=&methods;if(actual)*actual=flags;return SQLITE_OK;
}
int deleteFile(sqlite3_vfs*,const char* name,int){
    // LittleFS commits directory metadata on unlink; ESP VFS has no directory fsync.
    return (::unlink(name)==0||errno==ENOENT)?SQLITE_OK:SQLITE_IOERR_DELETE;
}
int accessFile(sqlite3_vfs*,const char* name,int,int* result){
    struct stat st{};*result=::stat(name,&st)==0;
    if(!*result&&errno!=ENOENT&&errno!=ENOTDIR)return SQLITE_IOERR_ACCESS;return SQLITE_OK;
}
int fullPath(sqlite3_vfs*,const char* name,int size,char* out){
    if(!name||name[0]!='/'||std::strlen(name)>=static_cast<size_t>(size))return SQLITE_CANTOPEN;
    std::snprintf(out,size,"%s",name);return SQLITE_OK;
}
int randomBytes(sqlite3_vfs*,int bytes,char* out){
#ifdef ESP_PLATFORM
    esp_fill_random(out,bytes);
#else
    std::random_device r;for(int i=0;i<bytes;++i)out[i]=static_cast<char>(r());
#endif
    return bytes;
}
int sleepMicros(sqlite3_vfs*,int micros){
#ifdef ESP_PLATFORM
    auto ticks=std::max<TickType_t>(1,pdMS_TO_TICKS((micros+999)/1000));
    vTaskDelay(ticks);return ticks*portTICK_PERIOD_MS*1000;
#else
    std::this_thread::sleep_for(std::chrono::microseconds(micros));return micros;
#endif
}
int timeNow(sqlite3_vfs*,double* value){
    timeval t{};gettimeofday(&t,nullptr);*value=2440587.5+(t.tv_sec+t.tv_usec/1000000.0)/86400.0;return SQLITE_OK;
}
sqlite3_vfs vfs={};
}
extern "C" int sqlite3_os_init(){
    vfs.iVersion=1;vfs.szOsFile=sizeof(File);vfs.mxPathname=512;vfs.zName="pine-vfs";
    vfs.xOpen=openFile;vfs.xDelete=deleteFile;vfs.xAccess=accessFile;vfs.xFullPathname=fullPath;
    vfs.xRandomness=randomBytes;vfs.xSleep=sleepMicros;vfs.xCurrentTime=timeNow;
    return sqlite3_vfs_register(&vfs,1);
}
extern "C" int sqlite3_os_end(){return SQLITE_OK;}
