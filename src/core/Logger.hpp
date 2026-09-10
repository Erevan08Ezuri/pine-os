#pragma once
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace Pine {
enum class LogLevel { Trace, Debug, Info, Warn, Error };
class Logger {
public:
  static Logger& instance();
  void initialize(const std::filesystem::path& dataRoot, LogLevel minimum = LogLevel::Info);
  void log(LogLevel level, const std::string& category, const std::string& message);
  void trace(const std::string& c, const std::string& m) { log(LogLevel::Trace,c,m); }
  void debug(const std::string& c, const std::string& m) { log(LogLevel::Debug,c,m); }
  void info(const std::string& c, const std::string& m) { log(LogLevel::Info,c,m); }
  void warn(const std::string& c, const std::string& m) { log(LogLevel::Warn,c,m); }
  void error(const std::string& c, const std::string& m) { log(LogLevel::Error,c,m); }
private:
  Logger() = default;
  std::mutex mutex_; std::ofstream file_; LogLevel minimum_{LogLevel::Info};
};
}
