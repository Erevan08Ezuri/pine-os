#pragma once
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace Pine {
// Recoverable replacement on Windows and ESP VFS, where rename cannot replace
// an existing file. Keep the last committed version until the new one is closed.
inline void writeAtomicFile(const std::filesystem::path& path, std::string_view content) {
    auto temporary=path; temporary+=".tmp";
    auto backup=path; backup+=".bak";
    { std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
      if(!out) throw std::runtime_error("Cannot open temporary file");
      out.write(content.data(),static_cast<std::streamsize>(content.size()));
      out.close(); if(!out) throw std::runtime_error("Cannot finish temporary file"); }
    if(std::filesystem::exists(path)) {
        std::filesystem::remove(backup);
        std::filesystem::rename(path,backup);
    }
    try { std::filesystem::rename(temporary,path); }
    catch(...) {
        std::error_code ec;
        if(std::filesystem::exists(backup)) std::filesystem::rename(backup,path,ec);
        throw;
    }
    std::error_code ec; std::filesystem::remove(backup,ec);
}
inline void recoverAtomicFile(const std::filesystem::path& path) {
    auto backup=path; backup+=".bak";
    if(!std::filesystem::exists(path)&&std::filesystem::exists(backup))
        std::filesystem::rename(backup,path);
}
}
