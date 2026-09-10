#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace Pine {struct FileEntry{std::string name;std::filesystem::path relativePath;bool directory;std::uintmax_t size;std::filesystem::file_time_type modified;};class FileService{public:explicit FileService(std::filesystem::path root);void initialize();const std::filesystem::path&root()const{return root_;}std::filesystem::path resolve(const std::filesystem::path&relative)const;std::vector<FileEntry>listDirectory(const std::filesystem::path&relative)const;bool createDirectory(const std::filesystem::path&);bool rename(const std::filesystem::path&,const std::string&);bool copy(const std::filesystem::path&,const std::filesystem::path&);bool move(const std::filesystem::path&,const std::filesystem::path&);bool remove(const std::filesystem::path&);bool exists(const std::filesystem::path&)const;FileEntry fileInfo(const std::filesystem::path&)const;std::uintmax_t usedBytes()const;private:std::filesystem::path root_;};}
