#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace Pine {
struct Note {
  std::string id,title,body;
  std::int64_t createdAt{},updatedAt{};
  bool pinned{};
};

class NotesService {
public:
  explicit NotesService(std::filesystem::path dataRoot);
  ~NotesService();
  NotesService(const NotesService&)=delete;NotesService&operator=(const NotesService&)=delete;
  Note create();
  bool update(const std::string&id,std::string title,std::string body,bool pinned);
  bool setPinned(const std::string&id,bool pinned);
  bool remove(const std::string&id);
  std::optional<Note> get(const std::string&id)const;
  std::vector<Note> list(const std::string&query={})const;
  void flush();
  std::string consumeError();
  const std::filesystem::path&root()const{return root_;}
private:
  enum class JobType{Save,Remove};struct Job{JobType type;Note note;std::string id;};
  static std::int64_t now();static std::string makeId();
  void load();void enqueue(Job);void workerLoop();void writeNote(const Note&);void deleteNote(const std::string&);
  std::filesystem::path root_;mutable std::mutex mutex_;std::condition_variable workCv_,idleCv_;
  std::vector<Note>notes_;std::deque<Job>jobs_;std::thread worker_;bool stopping_{},working_{};std::string lastError_;
};
}
