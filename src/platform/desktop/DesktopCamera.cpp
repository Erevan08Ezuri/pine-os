#include "platform/desktop/DesktopCamera.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <vector>
namespace Pine {
bool DesktopCamera::capturePhoto(const std::filesystem::path& path){
  if(!available_)return false;constexpr int w=640,h=480,row=w*3,fileSize=54+row*h;std::filesystem::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);if(!out)return false;
  std::array<unsigned char,54> header{};auto put32=[&](int offset,std::uint32_t value){for(int i=0;i<4;++i)header[offset+i]=static_cast<unsigned char>((value>>(i*8))&0xff);};header[0]='B';header[1]='M';put32(2,fileSize);header[10]=54;header[14]=40;put32(18,w);put32(22,h);header[26]=1;header[28]=24;put32(34,row*h);out.write(reinterpret_cast<char*>(header.data()),header.size());
  std::vector<unsigned char> scan(row);for(int y=0;y<h;++y){for(int x=0;x<w;++x){scan[x*3]=static_cast<unsigned char>((x+y)%256);scan[x*3+1]=static_cast<unsigned char>(70+(y*120/h));scan[x*3+2]=static_cast<unsigned char>(30+(x*160/w));}out.write(reinterpret_cast<char*>(scan.data()),scan.size());}return static_cast<bool>(out);
}
}
