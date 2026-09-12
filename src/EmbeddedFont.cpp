#include "EmbeddedFont.hpp"

#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>

EmbeddedFont::~EmbeddedFont()
{
  if (m_colorReady && m_pDisplay)
  {
    XftColorFree(
      m_pDisplay,
      DefaultVisual(m_pDisplay, m_screen),
      DefaultColormap(m_pDisplay, m_screen),
      &m_color);
    m_colorReady = false;
  }

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

bool EmbeddedFont::load(Display* pDisplay, int screen,
                        const unsigned char* pData, size_t len, double pixelSize)
{
  if (!pDisplay || !pData || len == 0)
  {
    return false;
  }

  m_pDisplay = pDisplay;
  m_screen = screen;

  if (FT_Init_FreeType(&m_ftLibrary) != 0)
  {
    return false;
  }
  if (FT_New_Memory_Face(m_ftLibrary, pData, static_cast<FT_Long>(len), 0, &m_ftFace) != 0)
  {
    return false;
  }

  FcPattern* pPattern = FcPatternCreate();
  FcPatternAddFTFace(pPattern, FC_FT_FACE, m_ftFace);
  FcPatternAddDouble(pPattern, FC_PIXEL_SIZE, pixelSize);
  FcPatternAddBool(pPattern, FC_ANTIALIAS, FcTrue);
  FcConfigSubstitute(nullptr, pPattern, FcMatchPattern);
  XftDefaultSubstitute(pDisplay, screen, pPattern);

  m_pFont = XftFontOpenPattern(pDisplay, pPattern);
  return m_pFont != nullptr;
}

void EmbeddedFont::draw(Display* pDisplay, int screen, Window window,
                        int x, int y, const std::string& text)
{
  if (!m_pFont || text.empty())
  {
    return;
  }

  XftDraw* pDraw = XftDrawCreate(
    pDisplay, window,
    DefaultVisual(pDisplay, screen),
    DefaultColormap(pDisplay, screen));

  if (!m_colorReady)
  {
    setColor(0xffffff);
  }

  XftDrawStringUtf8(pDraw, &m_color, m_pFont, x, y,
    reinterpret_cast<const FcChar8*>(text.c_str()),
    static_cast<int>(text.size()));

  //XftColor color;
  //XRenderColor rc;
  //rc.red = 0xffff;
  //rc.green = 0xffff;
  //rc.blue = 0xffff;
  //rc.alpha = 0xffff;
  //XftColorAllocValue(pDisplay, DefaultVisual(pDisplay, screen),
  //                   DefaultColormap(pDisplay, screen), &rc, &color);

  //XftDrawStringUtf8(pDraw, &color, m_pFont, x, y,
  //  reinterpret_cast<const FcChar8*>(text.c_str()),
  //  static_cast<int>(text.size()));

  //XftColorFree(pDisplay, DefaultVisual(pDisplay, screen),
  //             DefaultColormap(pDisplay, screen), &color);
  XftDrawDestroy(pDraw);
}

void EmbeddedFont::setColor(unsigned long color)
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
      &m_color);
    m_colorReady = false;
  }
  XRenderColor rc;
  rc.red   = static_cast<unsigned short>(((color >> 16) & 0xff) * 257);
  rc.green = static_cast<unsigned short>(((color >> 8)  & 0xff) * 257);
  rc.blue  = static_cast<unsigned short>(( color        & 0xff) * 257);
  rc.alpha = 0xffff;
  if (XftColorAllocValue(
        m_pDisplay,
        DefaultVisual(m_pDisplay, m_screen),
        DefaultColormap(m_pDisplay, m_screen),
        &rc,
        &m_color))
  {
    m_colorReady = true;
  }
}
