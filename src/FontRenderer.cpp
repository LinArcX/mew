#include "FontRenderer.hpp"
#include "hurmit_font_data.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <cstdio>

FontRenderer::FontRenderer()
{
}

FontRenderer::~FontRenderer()
{
  if (m_pFont && m_pDisplay)
  {
    XftFontClose(m_pDisplay, m_pFont);
    m_pFont = nullptr;
  }
  if (m_ftFace)
  {
    FT_Done_Face(m_ftFace);
    m_ftFace = nullptr;
  }
  if (m_ftLibrary)
  {
    FT_Done_FreeType(m_ftLibrary);
    m_ftLibrary = nullptr;
  }
}

void FontRenderer::load(Display* pDisplay, int screen, double pixelSize)
{
  m_pDisplay = pDisplay;
  m_screen = screen;

  if (FT_Init_FreeType(&m_ftLibrary) != 0)
  {
    fprintf(stderr, "mew: FT_Init_FreeType failed\n");
    return;
  }

  if (FT_New_Memory_Face(
        m_ftLibrary,
        hurmit_ttf,
        static_cast<FT_Long>(hurmit_ttf_len),
        0,
        &m_ftFace) != 0)
  {
    fprintf(stderr, "mew: FT_New_Memory_Face failed\n");
    return;
  }

  FcPattern* pattern = FcPatternCreate();
  FcPatternAddFTFace(pattern, FC_FT_FACE, m_ftFace);
  FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixelSize);
  FcPatternAddBool(pattern, FC_ANTIALIAS, FcTrue);
  FcPatternAddBool(pattern, FC_AUTOHINT, FcFalse);
  FcPatternAddBool(pattern, FC_HINTING, FcTrue);
  FcPatternAddInteger(pattern, FC_HINT_STYLE, FC_HINT_SLIGHT);
  FcPatternAddInteger(pattern, FC_RGBA, FC_RGBA_RGB);
  FcPatternAddInteger(pattern, FC_LCD_FILTER, FC_LCD_DEFAULT);

  FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
  XftDefaultSubstitute(pDisplay, screen, pattern);

  m_pFont = XftFontOpenPattern(pDisplay, pattern);
  if (!m_pFont)
  {
    fprintf(stderr, "mew: XftFontOpenPattern failed\n");
  }
}

void FontRenderer::draw(
  Display* pDisplay,
  int screen,
  Window window,
  int x,
  int y,
  const std::string& text)
{
  if (!m_pFont)
  {
    GC gc = XCreateGC(pDisplay, window, 0, nullptr);
    XSetForeground(pDisplay, gc, 0xffffff);
    XDrawString(pDisplay, window, gc, x, y, text.c_str(), static_cast<int>(text.size()));
    XFreeGC(pDisplay, gc);
    return;
  }

  if (!m_colorReady)
  {
    setColor(0xffffff);
  }

  XftDraw* draw = XftDrawCreate(
    pDisplay,
    window,
    DefaultVisual(pDisplay, screen),
    DefaultColormap(pDisplay, screen));

  XftDrawStringUtf8(
    draw,
    &m_textColor,
    m_pFont,
    x,
    y,
    reinterpret_cast<const FcChar8*>(text.c_str()),
    static_cast<int>(text.size()));

  XftDrawDestroy(draw);
}


void FontRenderer::setColor(unsigned long color)
{
  if (!m_pDisplay)
  {
    return;
  }
  if (m_colorReady)
  {
    XftColorFree(
      m_pDisplay,
      DefaultVisual(m_pDisplay, m_screen),
      DefaultColormap(m_pDisplay, m_screen),
      &m_textColor);
    m_colorReady = false;
  }
  XRenderColor renderColor;
  renderColor.red = static_cast<unsigned short>(((color >> 16) & 0xff) * 257);
  renderColor.green = static_cast<unsigned short>(((color >> 8) & 0xff) * 257);
  renderColor.blue = static_cast<unsigned short>((color & 0xff) * 257);
  renderColor.alpha = 0xffff;
  if (XftColorAllocValue(
        m_pDisplay,
        DefaultVisual(m_pDisplay, m_screen),
        DefaultColormap(m_pDisplay, m_screen),
        &renderColor,
        &m_textColor))
  {
    m_colorReady = true;
  }
}
