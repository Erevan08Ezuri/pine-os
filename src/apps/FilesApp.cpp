#include "apps/Apps.hpp"
#include "core/UiContext.hpp"
#include "ui/Shell.hpp"
#include <chrono>
#include <format>
namespace Pine {void FilesApp::render(UiContext&ui){ui.renderFiles();}
void Shell::renderFiles(){
  label(45,105,"FILES",6,Theme::Gold);std::string crumb="PINE";for(auto&p:currentPath_)crumb+=" > "+p.string();sublabel(48,165,crumb);
  if(button({40,200,190,52},"NEW FOLDER",true))openTextPrompt("NEW FOLDER","",[this](const std::string&name){if(files_.createDirectory(currentPath_/name))toast("FOLDER CREATED");else toast("CREATE FAILED");});
  if(!currentPath_.empty()&&button({245,200,100,52},"UP")){currentPath_=currentPath_.parent_path();selected_.reset();}
  std::vector<FileEntry>entries;try{entries=files_.listDirectory(currentPath_);}catch(const std::exception&e){toast(e.what());}
  float y=275;for(size_t i=0;i<entries.size()&&i<8;++i){auto&e=entries[i];bool chosen=selected_&&selected_->relativePath==e.relativePath;panel({40,y,640,72});label(65,y+18,e.directory?"DIR":"FILE",2,Theme::Gold);label(145,y+17,e.name,2.0f);sublabel(145,y+45,e.directory?"FOLDER":std::to_string(e.size)+" BYTES");if(button({565,y+12,90,48},e.directory?"OPEN":"INFO",chosen)){if(e.directory){currentPath_=e.relativePath;selected_.reset();return;}selected_=e;}if(hit({40,y,510,72}))selected_=e;y+=82;}
  if(entries.empty()){panel({40,300,640,180});label(210,355,"EMPTY FOLDER",4);sublabel(220,410,"CREATE A FOLDER TO BEGIN");}
  if(selected_){panel({40,965,640,150});label(65,985,selected_->name.substr(0,28),2.5f);auto sys=std::chrono::time_point_cast<std::chrono::system_clock::duration>(selected_->modified-std::filesystem::file_time_type::clock::now()+std::chrono::system_clock::now());sublabel(65,1022,std::format("{} BYTES - MODIFIED {:%Y-%m-%d %H:%M}",selected_->size,sys));
    if(button({65,1055,125,45},"RENAME"))openTextPrompt("RENAME",selected_->name,[this,p=selected_->relativePath](const std::string&name){toast(files_.rename(p,name)?"RENAMED":"RENAME FAILED");selected_.reset();});
    if(button({200,1055,100,45},"COPY")){auto p=selected_->relativePath;auto dest=p.parent_path()/(p.stem().string()+" Copy"+p.extension().string());toast(files_.copy(p,dest)?"COPIED":"COPY FAILED");}
    if(button({310,1055,100,45},"MOVE")){auto dest=std::filesystem::path("Documents")/selected_->relativePath.filename();toast(files_.move(selected_->relativePath,dest)?"MOVED TO DOCUMENTS":"MOVE FAILED");selected_.reset();}
    if(button({420,1055,110,45},"DELETE")){toast(files_.remove(selected_->relativePath)?"DELETED":"DELETE FAILED");selected_.reset();}
    if(button({540,1055,110,45},"CLEAR"))selected_.reset();
  }
}}
