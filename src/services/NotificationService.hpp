#pragma once
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
namespace Pine {struct Notification{std::string id,source,title,body;std::int64_t createdAt{};bool sensitive{},read{};};class NotificationService{public:explicit NotificationService(std::filesystem::path dataRoot);void post(std::string source,std::string title,std::string body,bool sensitive=false);void postOnce(std::string id,std::string source,std::string title,std::string body,bool sensitive=false);std::vector<Notification>list()const;void markRead(const std::string&);private:void load();void save()const;std::filesystem::path path_;mutable std::mutex mutex_;std::vector<Notification>items_;};}
