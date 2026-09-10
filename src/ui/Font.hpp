#pragma once

#include <SDL3/SDL.h>
#include <filesystem>
#include <string>

namespace Pine {
bool initializeFonts(const std::filesystem::path& regular, const std::filesystem::path& semibold);
void shutdownFonts();
bool fontsReady();
void drawText(SDL_Renderer*, float x, float y, const std::string&, float scale, SDL_Color);
float textWidth(const std::string&, float scale);
}
