#pragma once

#include <SDL3/SDL.h>

namespace Pine {
// Uses the existing RGB565 software framebuffer; never owns an LCD buffer.
// The caller owns the artwork for the duration of startup.
class Tab5Splash {
public:
    explicit Tab5Splash(SDL_Surface* artwork);
    void render(SDL_Renderer* renderer, SDL_Surface* target, float progress) const;
private:
    SDL_Surface* artwork_;
};
}
