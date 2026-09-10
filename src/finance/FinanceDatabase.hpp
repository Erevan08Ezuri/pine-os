#pragma once
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
struct sqlite3;
namespace Pine::Finance {
class FinanceDatabase{
public:
  explicit FinanceDatabase(std::filesystem::path dataRoot);~FinanceDatabase();
  FinanceDatabase(const FinanceDatabase&)=delete;FinanceDatabase&operator=(const FinanceDatabase&)=delete;
  sqlite3*handle()const{return db_;}std::mutex&mutex(){return mutex_;}const std::filesystem::path&path()const{return path_;}bool memoryFallback()const{return memoryFallback_;}const std::string&warning()const{return warning_;}
  void transaction(const std::function<void()>&work);void execute(const std::string&sql);int schemaVersion()const;
private:void migrate();void openMemoryFallback(const std::string&reason);sqlite3*db_{};std::filesystem::path path_;mutable std::mutex mutex_;bool memoryFallback_{};std::string warning_;
};
}
