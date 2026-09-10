#include "services/NotesService.hpp"
#include "core/Logger.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

namespace Pine {
using json=nlohmann::json;
std::int64_t NotesService::now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
std::string NotesService::makeId(){static std::atomic_uint64_t sequence{};std::ostringstream out;out<<std::hex<<now()<<'-'<<sequence.fetch_add(1);return out.str();}

NotesService::NotesService(std::filesystem::path dataRoot):root_(std::move(dataRoot)/"notes"){
  try{std::filesystem::create_directories(root_);load();}catch(const std::exception&e){lastError_=std::string("Notes storage unavailable: ")+e.what();Logger::instance().error("NOTES",lastError_);}worker_=std::thread(&NotesService::workerLoop,this);
}
NotesService::~NotesService(){flush();{std::lock_guard lock(mutex_);stopping_=true;}workCv_.notify_one();if(worker_.joinable())worker_.join();}
void NotesService::load(){
  for(const auto&entry:std::filesystem::directory_iterator(root_)){
    if(!entry.is_regular_file()||entry.path().extension()!=".json")continue;
    try{std::ifstream in(entry.path());json j;in>>j;Note n{j.at("id").get<std::string>(),j.value("title",""),j.value("body",""),j.at("created_at").get<std::int64_t>(),j.at("updated_at").get<std::int64_t>(),j.value("pinned",false)};if(!n.id.empty())notes_.push_back(std::move(n));}
    catch(const std::exception&e){Logger::instance().warn("NOTES","Skipped malformed note "+entry.path().filename().string()+": "+e.what());}
  }
  Logger::instance().info("NOTES","Loaded "+std::to_string(notes_.size())+" notes");
}
Note NotesService::create(){Note note;note.id=makeId();note.createdAt=note.updatedAt=now();{std::lock_guard lock(mutex_);notes_.push_back(note);}enqueue({JobType::Save,note,{}});return note;}
bool NotesService::update(const std::string&id,std::string title,std::string body,bool pinned){Note copy;{std::lock_guard lock(mutex_);auto it=std::find_if(notes_.begin(),notes_.end(),[&](const Note&n){return n.id==id;});if(it==notes_.end())return false;it->title=std::move(title);it->body=std::move(body);it->pinned=pinned;it->updatedAt=now();copy=*it;}enqueue({JobType::Save,copy,{}});return true;}
bool NotesService::setPinned(const std::string&id,bool pinned){auto n=get(id);return n&&update(id,n->title,n->body,pinned);}
bool NotesService::remove(const std::string&id){{std::lock_guard lock(mutex_);auto it=std::find_if(notes_.begin(),notes_.end(),[&](const Note&n){return n.id==id;});if(it==notes_.end())return false;notes_.erase(it);}enqueue({JobType::Remove,{},id});return true;}
std::optional<Note>NotesService::get(const std::string&id)const{std::lock_guard lock(mutex_);auto it=std::find_if(notes_.begin(),notes_.end(),[&](const Note&n){return n.id==id;});if(it==notes_.end())return{};return*it;}
std::vector<Note>NotesService::list(const std::string&query)const{
  std::lock_guard lock(mutex_);std::vector<Note>out;std::string needle=query;std::ranges::transform(needle,needle.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
  for(const auto&note:notes_){std::string hay=note.title+'\n'+note.body;std::ranges::transform(hay,hay.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(needle.empty()||hay.find(needle)!=std::string::npos)out.push_back(note);}
  std::ranges::sort(out,[](const Note&a,const Note&b){if(a.pinned!=b.pinned)return a.pinned>b.pinned;if(a.updatedAt!=b.updatedAt)return a.updatedAt>b.updatedAt;return a.id>b.id;});return out;
}
void NotesService::enqueue(Job job){{std::lock_guard lock(mutex_);jobs_.push_back(std::move(job));}workCv_.notify_one();}
void NotesService::writeNote(const Note&n){
  const auto target=root_/(n.id+".json"),temporary=root_/(n.id+".json.tmp"),backup=root_/(n.id+".json.bak");
  const json j={{"id",n.id},{"title",n.title},{"body",n.body},{"created_at",n.createdAt},{"updated_at",n.updatedAt},{"pinned",n.pinned}};
  {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("Cannot open temporary note file");out<<j.dump(2)<<'\n';out.flush();if(!out)throw std::runtime_error("Cannot write temporary note file");}
  std::error_code ec;std::filesystem::remove(backup,ec);ec.clear();if(std::filesystem::exists(target)){std::filesystem::rename(target,backup,ec);if(ec)throw std::runtime_error("Cannot stage existing note: "+ec.message());}
  std::filesystem::rename(temporary,target,ec);if(ec){std::error_code restore;if(std::filesystem::exists(backup))std::filesystem::rename(backup,target,restore);throw std::runtime_error("Cannot commit note: "+ec.message());}std::filesystem::remove(backup,ec);
}
void NotesService::deleteNote(const std::string&id){std::error_code ec;std::filesystem::remove(root_/(id+".json"),ec);if(ec)throw std::runtime_error("Cannot delete note: "+ec.message());std::filesystem::remove(root_/(id+".json.tmp"),ec);std::filesystem::remove(root_/(id+".json.bak"),ec);}
void NotesService::workerLoop(){
  for(;;){Job job;{std::unique_lock lock(mutex_);workCv_.wait(lock,[&]{return stopping_||!jobs_.empty();});if(stopping_&&jobs_.empty())break;job=std::move(jobs_.front());jobs_.pop_front();working_=true;}
    std::string error;for(int attempt=0;attempt<3;++attempt){try{if(job.type==JobType::Save)writeNote(job.note);else deleteNote(job.id);error.clear();break;}catch(const std::exception&e){error=e.what();std::this_thread::sleep_for(std::chrono::milliseconds(50*(attempt+1)));}}
    {std::lock_guard lock(mutex_);if(!error.empty()){lastError_=error;Logger::instance().error("NOTES",error);}working_=false;if(jobs_.empty())idleCv_.notify_all();}
  }
}
void NotesService::flush(){std::unique_lock lock(mutex_);idleCv_.wait(lock,[&]{return jobs_.empty()&&!working_;});}
std::string NotesService::consumeError(){std::lock_guard lock(mutex_);auto result=std::move(lastError_);lastError_.clear();return result;}
}
