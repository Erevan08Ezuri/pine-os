#include "apps/Apps.hpp"
#include "core/Logger.hpp"
#include "core/UiContext.hpp"
#include "ui/Font.hpp"
#include "ui/Shell.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <string_view>
#include <vector>

namespace Pine {
namespace {
struct TextLine{std::size_t start{},end{};std::string text;};
std::vector<TextLine>wrapText(const std::string&value,float width,float scale){
  std::vector<TextLine>lines;std::size_t start=0;std::string current;
  for(std::size_t i=0;i<value.size();++i){const char c=value[i];if(c=='\n'){lines.push_back({start,i,current});start=i+1;current.clear();continue;}auto candidate=current+c;if(!current.empty()&&textWidth(candidate,scale)>width){lines.push_back({start,i,current});start=i;current.assign(1,c);}else current=std::move(candidate);}
  lines.push_back({start,value.size(),current});return lines;
}
std::string preview(std::string text){for(char&c:text)if(c=='\n'||c=='\r'||c=='\t')c=' ';if(text.size()>72)text=text.substr(0,69)+"...";return text.empty()?"EMPTY NOTE":text;}
std::string displayTitle(const Note&note){return note.title.empty()?"Untitled Note":note.title;}
std::string modifiedLabel(std::int64_t milliseconds){const auto point=std::chrono::system_clock::time_point(std::chrono::milliseconds(milliseconds));return std::format("{:%b %d  %H:%M}",point);}
}

void NotesApp::render(UiContext&ui){ui.renderNotes();}

void Shell::beginNewNote(){commitNote();auto note=notes_.create();editingNoteId_=note.id;noteTitle_.clear();noteBody_.clear();notePinned_=false;noteHasChanges_=false;deleteNotePrompt_=false;bodyScroll_=0;titleSession_->setCursor(0);bodySession_->setCursor(0);notesView_=NotesView::Editor;focusInput(*titleSession_);Logger::instance().info("NOTES","Created note "+note.id);}
void Shell::openNote(const std::string&id){commitNote();auto note=notes_.get(id);if(!note)return;editingNoteId_=note->id;noteTitle_=note->title;noteBody_=note->body;notePinned_=note->pinned;noteHasChanges_=false;deleteNotePrompt_=false;bodyScroll_=0;titleSession_->setCursor(noteTitle_.size());bodySession_->setCursor(noteBody_.size());notesView_=NotesView::Editor;dismissInput();}
void Shell::commitNote(){if(editingNoteId_.empty()||!noteHasChanges_)return;if(notes_.update(editingNoteId_,noteTitle_,noteBody_,notePinned_)){noteHasChanges_=false;noteSaveDebounce_.clear();Logger::instance().debug("NOTES","Autosaved note "+editingNoteId_);}else toast("NOTE COULD NOT BE SAVED");}
void Shell::closeNoteEditor(){commitNote();dismissInput();notesView_=NotesView::List;deleteNotePrompt_=false;editingNoteId_.clear();}
void Shell::confirmDeleteNote(){if(editingNoteId_.empty())return;const auto id=editingNoteId_;if(notes_.remove(id)){Logger::instance().info("NOTES","Deleted note "+id);noteHasChanges_=false;editingNoteId_.clear();notesView_=NotesView::List;deleteNotePrompt_=false;dismissInput();toast("NOTE DELETED");}else toast("DELETE FAILED");}
bool Shell::acceptanceCreateNote(const std::string&title,const std::string&body,bool pinned){auto note=notes_.create();const bool ok=notes_.update(note.id,title,body,pinned);notes_.flush();return ok;}

void Shell::drawTextField(Rect r,TextInputSession&session,const std::string&placeholder,bool multiline){
  const bool active=textInput_.isFocused(session);coloredPanel(r,Theme::Surface,active?Theme::Gold:Theme::Border,10);const float scale=2.35f,lineHeight=30,x=r.x+16,y=r.y+13,maxWidth=r.w-32;
  if(session.value().empty()&&!active){label(x,y,placeholder,scale,Theme::Muted);return;}
  if(!multiline){const std::string display=session.type()==InputType::Password?std::string(session.value().size(),'*'):session.value();float offset=0;const float cursorWidth=textWidth(display.substr(0,std::min(session.cursor(),display.size())),scale);if(cursorWidth>maxWidth-8)offset=cursorWidth-(maxWidth-8);label(x-offset,y,display,scale);if(active&&(SDL_GetTicks()/500)%2==0){SDL_SetRenderDrawColor(renderer_,Theme::Gold.r,Theme::Gold.g,Theme::Gold.b,255);SDL_RenderLine(renderer_,x+cursorWidth-offset,y,x+cursorWidth-offset,y+24);}return;}
  auto lines=wrapText(session.value(),maxWidth,scale);int cursorLine=0;for(std::size_t i=0;i<lines.size();++i)if(session.cursor()>=lines[i].start&&session.cursor()<=lines[i].end){cursorLine=static_cast<int>(i);break;}const int visible=std::max(1,static_cast<int>((r.h-22)/lineHeight));if(active){if(cursorLine<bodyScroll_)bodyScroll_=static_cast<float>(cursorLine);if(cursorLine>=bodyScroll_+visible)bodyScroll_=static_cast<float>(cursorLine-visible+1);}const int first=std::clamp(static_cast<int>(bodyScroll_),0,std::max(0,static_cast<int>(lines.size())-visible));bodyScroll_=static_cast<float>(first);
  for(int row=0;row<visible&&first+row<static_cast<int>(lines.size());++row){const auto&line=lines[first+row];const float lineY=y+row*lineHeight;label(x,lineY,line.text,scale);if(active&&first+row==cursorLine&&(SDL_GetTicks()/500)%2==0){const auto within=std::clamp(session.cursor(),line.start,line.end)-line.start;const float cursorX=x+textWidth(line.text.substr(0,within),scale);SDL_SetRenderDrawColor(renderer_,Theme::Gold.r,Theme::Gold.g,Theme::Gold.b,255);SDL_RenderLine(renderer_,cursorX,lineY,cursorX,lineY+24);}}
}

std::size_t Shell::cursorAt(Rect r,const TextInputSession&session,float px,float py,bool multiline){
  const float scale=2.35f,maxWidth=r.w-32;if(!multiline){float offset=0;const float current=textWidth(session.value().substr(0,session.cursor()),scale);if(current>maxWidth-8)offset=current-(maxWidth-8);const float local=std::max(0.0f,px-(r.x+16)+offset);for(std::size_t i=0;i<=session.value().size();++i)if(textWidth(session.value().substr(0,i),scale)>=local)return i;return session.value().size();}
  auto lines=wrapText(session.value(),maxWidth,scale);const int row=std::clamp(static_cast<int>((py-(r.y+13))/30)+static_cast<int>(bodyScroll_),0,std::max(0,static_cast<int>(lines.size())-1));const auto&line=lines[row];const float local=std::max(0.0f,px-(r.x+16));for(std::size_t i=0;i<=line.text.size();++i)if(textWidth(line.text.substr(0,i),scale)>=local)return line.start+i;return line.end;
}

void Shell::renderNotes(){
  if(notesView_==NotesView::List){
    label(45,100,"NOTES",5,Theme::Gold);sublabel(48,151,"OFFLINE NOTES");const Rect search{40,185,415,62};drawTextField(search,*searchSession_,"SEARCH NOTES");if(hit(search)){searchSession_->setCursor(cursorAt(search,*searchSession_,clickX_,clickY_,false));focusInput(*searchSession_);}if(button({470,185,210,62},"NEW NOTE",true)){dismissInput();beginNewNote();return;}
    const auto results=notes_.list(notesSearch_);const float listTop=275,listBottom=1165,cardHeight=126,gap=12;const float maxScroll=std::max(0.0f,results.size()*(cardHeight+gap)-(listBottom-listTop));notesScroll_=std::clamp(notesScroll_,0.0f,maxScroll);
    if(results.empty()){panel({80,360,560,230});label(240,410,notesSearch_.empty()?"NO NOTES YET":"NO MATCHING NOTES",3.5f);sublabel(210,465,notesSearch_.empty()?"YOUR NOTES STAY ON THIS DEVICE":"TRY A DIFFERENT SEARCH");if(notesSearch_.empty()&&button({190,515,340,58},"CREATE NOTE",true)){dismissInput();beginNewNote();}return;}
    for(std::size_t i=0;i<results.size();++i){const float y=listTop+i*(cardHeight+gap)-notesScroll_;if(y+cardHeight<listTop||y>listBottom)continue;const Rect card{40,y,640,cardHeight};panel(card);label(65,y+20,displayTitle(results[i]).substr(0,42),3.0f);if(results[i].pinned)label(570,y+22,"PIN",2,Theme::Gold);sublabel(65,y+59,preview(results[i].body));label(65,y+91,modifiedLabel(results[i].updatedAt),1.85f,Theme::Muted);if(hit(card)){dismissInput();openNote(results[i].id);return;}}
    if(click_&&!hit(search))dismissInput();
    return;
  }

  label(45,92,"NOTE EDITOR",4.5f,Theme::Gold);if(button({40,145,125,54},"BACK")){closeNoteEditor();return;}if(button({180,145,190,54},notePinned_?"UNPIN":"PIN",notePinned_)){notePinned_=!notePinned_;noteHasChanges_=true;commitNote();}if(button({515,145,165,54},"DELETE")){dismissInput();deleteNotePrompt_=true;}
  const Rect title{40,220,640,68};drawTextField(title,*titleSession_,"NOTE TITLE");const float bodyBottom=std::min(1165.0f,textInput_.safeContentBottom(1280)-18);const Rect body{40,305,640,std::max(170.0f,bodyBottom-305)};drawTextField(body,*bodySession_,"START WRITING...",true);
  if(hit(title)){titleSession_->setCursor(cursorAt(title,*titleSession_,clickX_,clickY_,false));focusInput(*titleSession_);}else if(hit(body)){bodySession_->setCursor(cursorAt(body,*bodySession_,clickX_,clickY_,true));focusInput(*bodySession_);}else if(click_&&!deleteNotePrompt_)dismissInput();
  if(deleteNotePrompt_){SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(renderer_,0,0,0,215);SDL_FRect dim{0,72,720,1118};SDL_RenderFillRect(renderer_,&dim);panel({75,410,570,255});label(145,458,"DELETE THIS NOTE?",4,Theme::Gold);sublabel(150,515,"THIS ACTION CANNOT BE UNDONE");if(button({110,575,220,62},"CANCEL"))deleteNotePrompt_=false;if(button({390,575,220,62},"DELETE",true))confirmDeleteNote();SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);}
}
}
