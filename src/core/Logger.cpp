#include "core/Logger.hpp"
#include <chrono>
#include <format>
#include <iostream>

namespace Pine {
Logger& Logger::instance() { static Logger logger; return logger; }
void Logger::initialize(const std::filesystem::path& dataRoot, LogLevel minimum) {
  std::scoped_lock lock(mutex_); minimum_=minimum;
  const auto dir=dataRoot/"logs"; std::filesystem::create_directories(dir); const auto path=dir/"pine.log";
  std::error_code ec; if(std::filesystem::exists(path,ec)&&std::filesystem::file_size(path,ec)>2*1024*1024) {
    std::filesystem::rename(path,dir/"pine.log.old",ec);
    if(ec) std::filesystem::remove(path,ec);
  }
  file_.open(path,std::ios::app);
}
void Logger::log(LogLevel level,const std::string& category,const std::string& message) {
  if(level<minimum_) return;
  static constexpr const char* names[]={"TRACE","DEBUG","INFO","WARN","ERROR"};
  const auto now=std::chrono::system_clock::now();
  const auto line=std::format("[{:%Y-%m-%d %H:%M:%S}][{}][{}] {}",now,names[static_cast<int>(level)],category,message);
  std::scoped_lock lock(mutex_); std::cout<<line<<'\n'; if(file_) { file_<<line<<'\n'; file_.flush(); }
}
}
