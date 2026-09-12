#include "platform/tab5/Tab5Splash.hpp"
#include "ui/Font.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
    try{
        const std::filesystem::path assets=PINE_TEST_ASSET_ROOT;
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(Pine::initializeFonts(assets/"fonts/Inter-Regular.ttf",assets/"fonts/Inter-SemiBold.ttf"),"fonts");
        auto* artwork=SDL_LoadBMP((assets/"splash/pineos-gold-tab5.bmp").string().c_str());
        Pine::Tab5Splash splash(artwork);
        require(artwork->format==SDL_PIXELFORMAT_RGB565,"asset must use native RGB565");
        SDL_Surface* buffers[2]={SDL_CreateSurface(720,1280,SDL_PIXELFORMAT_RGB565),SDL_CreateSurface(720,1280,SDL_PIXELFORMAT_RGB565)};
        SDL_Renderer* renderers[2]={SDL_CreateSoftwareRenderer(buffers[0]),SDL_CreateSoftwareRenderer(buffers[1])};
        const auto pixel=[&](int buffer,int x,int y){return reinterpret_cast<Uint16*>(static_cast<Uint8*>(buffers[buffer]->pixels)+y*buffers[buffer]->pitch)[x];};
        const auto* format=SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGB565);
        const auto gold=SDL_MapRGB(format,nullptr,174,148,83);
        const auto track=SDL_MapRGB(format,nullptr,35,34,31);
        splash.render(renderers[0],buffers[0],-1);
        require(pixel(0,250,866)==track,"empty bar");
        splash.render(renderers[1],buffers[1],0.5f);
        require(pixel(1,250,866)==gold&&pixel(1,450,866)==track,"half bar");
        require(pixel(0,250,866)==track,"must not write to other framebuffer");
        for(int buffer=0;buffer<2;++buffer){
            int creditPixels=0;
            for(int y=1230;y<1254;++y)for(int x=24;x<300;++x){
                Uint8 r,g,b;
                SDL_GetRGB(pixel(buffer,x,y),format,nullptr,&r,&g,&b);
                if(r>70&&g>70&&b>60)++creditPixels;
            }
            require(creditPixels>150,"credit must appear on both alternating renderers");
        }
        require(Pine::textWidth("Developed by Carter B. bell",2.25f)<672,"credit must fit screen margins");
        if(argc>1)require(SDL_SaveBMP(buffers[1],argv[1]),SDL_GetError());
        splash.render(renderers[0],buffers[0],2);
        require(pixel(0,450,866)==gold,"complete bar");
        // Reusing a buffer must erase its previous progress.
        splash.render(renderers[0],buffers[0],0);
        require(pixel(0,450,866)==track,"stale bar pixels");
        Pine::shutdownFonts();
        for(int i=0;i<2;++i){SDL_DestroyRenderer(renderers[i]);SDL_DestroySurface(buffers[i]);}
        SDL_DestroySurface(artwork);SDL_Quit();
        std::cout<<"Tab5 splash RGB565 rendering passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
