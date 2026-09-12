#pragma once

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>

/**
 * @brief Xft font loaded from an in-memory TTF/OTF byte array.
 */
class EmbeddedFont
{
public:
  EmbeddedFont() = default;
  ~EmbeddedFont();

  bool load(Display* pDisplay, int screen,
            const unsigned char* pData, size_t len, double pixelSize);

  XftFont* font() const { return m_pFont; }
  int advanceWidth() const { return m_pFont ? m_pFont->max_advance_width : 0; }

  void draw(Display* pDisplay, int screen, Window window,
            int x, int y, const std::string& text);

  void setColor(unsigned long color);
private:
  FT_Library m_ftLibrary = nullptr;
  FT_Face m_ftFace = nullptr;
  XftFont* m_pFont = nullptr;
  Display* m_pDisplay = nullptr;
  int m_screen = 0;
  XftColor m_color{};
  bool m_colorReady = false;
};
