#include "ui/Font.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace Pine {
namespace {
struct Face { std::vector<unsigned char> bytes; stbtt_fontinfo info{}; bool ready{}; };
struct Glyph { SDL_Texture* texture{}; int width{}, height{}, xOffset{}, yOffset{}; float advance{}; };
Face regularFace, semiboldFace;
std::unordered_map<std::uint64_t, Glyph> glyphs;
SDL_Renderer* cachedRenderer{};

bool loadFace(Face& face, const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) return false;
  const auto size = input.tellg();
  if (size <= 0) return false;
  face.bytes.resize(static_cast<std::size_t>(size));
  input.seekg(0);
  input.read(reinterpret_cast<char*>(face.bytes.data()), size);
  face.ready = input.good() && stbtt_InitFont(&face.info, face.bytes.data(), 0) != 0;
  return face.ready;
}
Face& faceFor(float scale) { return scale >= 3.5f && semiboldFace.ready ? semiboldFace : regularFace; }
int pixelHeight(float scale) { return std::max(10, static_cast<int>(std::lround(scale * 8.0f))); }

Glyph& glyphFor(SDL_Renderer* renderer, Face& face, int height, unsigned char codepoint) {
  cachedRenderer = renderer;
  const bool bold = &face == &semiboldFace;
  const std::uint64_t key = (static_cast<std::uint64_t>(bold) << 40) |
                            (static_cast<std::uint64_t>(height) << 16) | codepoint;
  if (auto found = glyphs.find(key); found != glyphs.end()) return found->second;
  Glyph glyph;
  const float fontScale = stbtt_ScaleForPixelHeight(&face.info, static_cast<float>(height));
  int advance{}, bearing{};
  stbtt_GetCodepointHMetrics(&face.info, codepoint, &advance, &bearing);
  glyph.advance = advance * fontScale;
  int xMax{}, yMax{};
  stbtt_GetCodepointBitmapBox(&face.info, codepoint, fontScale, fontScale,
                              &glyph.xOffset, &glyph.yOffset, &xMax, &yMax);
  glyph.width = xMax - glyph.xOffset;
  glyph.height = yMax - glyph.yOffset;
  if (glyph.width > 0 && glyph.height > 0) {
    std::vector<unsigned char> alpha(static_cast<std::size_t>(glyph.width * glyph.height));
    stbtt_MakeCodepointBitmap(&face.info, alpha.data(), glyph.width, glyph.height, glyph.width,
                              fontScale, fontScale, codepoint);
    SDL_Surface* surface = SDL_CreateSurface(glyph.width, glyph.height, SDL_PIXELFORMAT_RGBA32);
    if (surface) {
      const auto* format = SDL_GetPixelFormatDetails(surface->format);
      for (int y = 0; y < glyph.height; ++y) {
        auto* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(surface->pixels) + y * surface->pitch);
        for (int x = 0; x < glyph.width; ++x)
          row[x] = SDL_MapRGBA(format, nullptr, 255, 255, 255,
                              alpha[static_cast<std::size_t>(y * glyph.width + x)]);
      }
      glyph.texture = SDL_CreateTextureFromSurface(renderer, surface);
      SDL_DestroySurface(surface);
      if (glyph.texture) SDL_SetTextureBlendMode(glyph.texture, SDL_BLENDMODE_BLEND);
    }
  }
  return glyphs.emplace(key, glyph).first->second;
}
}

bool initializeFonts(const std::filesystem::path& regular, const std::filesystem::path& semibold) {
  shutdownFonts();
  return loadFace(regularFace, regular) && loadFace(semiboldFace, semibold);
}
void shutdownFonts() {
  for (auto& [key, glyph] : glyphs) if (glyph.texture) SDL_DestroyTexture(glyph.texture);
  glyphs.clear(); cachedRenderer = nullptr; regularFace = {}; semiboldFace = {};
}
bool fontsReady() { return regularFace.ready && semiboldFace.ready; }

void drawText(SDL_Renderer* renderer, float x, float y, const std::string& text, float scale, SDL_Color color) {
  if (!renderer || !fontsReady()) return;
  Face& face = faceFor(scale);
  const int height = pixelHeight(scale);
  const float fontScale = stbtt_ScaleForPixelHeight(&face.info, static_cast<float>(height));
  int ascent{}, descent{}, gap{};
  stbtt_GetFontVMetrics(&face.info, &ascent, &descent, &gap);
  const float baseline = y + ascent * fontScale;
  float pen = x;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto codepoint = static_cast<unsigned char>(text[i]);
    auto& glyph = glyphFor(renderer, face, height, codepoint);
    if (glyph.texture) {
      SDL_SetTextureColorMod(glyph.texture, color.r, color.g, color.b);
      SDL_SetTextureAlphaMod(glyph.texture, color.a);
      SDL_FRect target{pen + glyph.xOffset, baseline + glyph.yOffset,
                       static_cast<float>(glyph.width), static_cast<float>(glyph.height)};
      SDL_RenderTexture(renderer, glyph.texture, nullptr, &target);
    }
    pen += glyph.advance;
    if (i + 1 < text.size()) pen += stbtt_GetCodepointKernAdvance(&face.info, codepoint,
      static_cast<unsigned char>(text[i + 1])) * fontScale;
  }
}

float textWidth(const std::string& text, float scale) {
  if (!fontsReady()) return static_cast<float>(text.size()) * 6.0f * scale;
  Face& face = faceFor(scale);
  const float fontScale = stbtt_ScaleForPixelHeight(&face.info, static_cast<float>(pixelHeight(scale)));
  float width = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    int advance{}, bearing{};
    const auto codepoint = static_cast<unsigned char>(text[i]);
    stbtt_GetCodepointHMetrics(&face.info, codepoint, &advance, &bearing);
    width += advance * fontScale;
    if (i + 1 < text.size()) width += stbtt_GetCodepointKernAdvance(&face.info, codepoint,
      static_cast<unsigned char>(text[i + 1])) * fontScale;
  }
  return width;
}
}
