#include "platform/tab5/Tab5Splash.hpp"
#include "ui/Font.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Pine {
namespace {
void pill(SDL_Renderer* renderer, float x, float y, float width, SDL_Color color) {
    constexpr float radius=4;
    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,255);
    for(int row=0;row<8;++row){
        const float dy=row+0.5f-radius;
        const float inset=radius-std::sqrt(radius*radius-dy*dy);
        if(width>2*inset)SDL_RenderLine(renderer,x+inset,y+row,x+width-inset-1,y+row);
    }
}
}
Tab5Splash::Tab5Splash(SDL_Surface* artwork):artwork_(artwork){
    if(!artwork_||artwork_->w!=720||artwork_->h!=1280)
        throw std::runtime_error("Tab5 splash artwork must be 720x1280");
}
void Tab5Splash::render(SDL_Renderer* renderer, SDL_Surface* target, float progress) const {
    if(!renderer||!target||target->w!=720||target->h!=1280)
        throw std::runtime_error("Tab5 splash framebuffer unavailable");
    // Flush queued SDL draws before writing to the software framebuffer.
    if(!SDL_FlushRenderer(renderer)||!SDL_BlitSurface(artwork_,nullptr,target,nullptr))
        throw std::runtime_error(SDL_GetError());
    pill(renderer,236,862,248,{35,34,31,255});
    const float width=248*std::clamp(progress,0.0f,1.0f);
    if(width>0)pill(renderer,236,862,width,{174,148,83,255});
    drawText(renderer,24,1230,"Developed by Carter B. bell",2.25f,{158,154,145,255});
    if(!SDL_RenderPresent(renderer))throw std::runtime_error(SDL_GetError());
}
}
