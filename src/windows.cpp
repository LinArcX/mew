#include "windows.h"

namespace Mew
{
  void Windows::drawTitleText(
    Window window,
    int x,
    int y,
    const std::string& text,
    const int screen,
    Display * display) noexcept
  {
    if (!m_titleFont) {
      // Fallback so a load failure doesn't leave titles blank.
      GC gc = XCreateGC(display, window, 0, nullptr);
      XSetForeground(display, gc, COLOR_TEXT);
      XDrawString(display, window, gc, x, y, text.c_str(), (int)text.size());
      XFreeGC(display, gc);
      return;
    }
  
    ensureTitleTextColor(screen, display);
  
    XftDraw* draw = XftDrawCreate(
      display, window,
      DefaultVisual(display, screen),
      DefaultColormap(display, screen));
  
    XftDrawStringUtf8(
      draw, &m_titleTextColor, m_titleFont,
      x, y,
      (const FcChar8*)text.c_str(), (int)text.size());
  
    XftDrawDestroy(draw);
  }

  void Windows::ensureTitleTextColor(
    const int screen,
    Display * display) noexcept
  {
    if (m_bIstitleTextColorReady) {
      return;
    }
  
    XRenderColor render_color;
    render_color.red   = 0xffff;
    render_color.green = 0xffff;
    render_color.blue  = 0xffff;
    render_color.alpha = 0xffff;
  
    XftColorAllocValue(
      display, DefaultVisual(display, screen),
      DefaultColormap(display, screen),
      &render_color, &m_titleTextColor);
  
    m_bIstitleTextColorReady = true;
  }

  std::string Windows::getWindowTitle(Display * display, Window window)
  {
    char* name = nullptr;
    if (XFetchName(display, window, &name) && name) {
      std::string title(name);
      XFree(name);
      return title.empty() ? "Untitled" : title;
    }
  
    XTextProperty prop;
    if (XGetWMName(display, window, &prop) && prop.value) {
      std::string title(reinterpret_cast<char*>(prop.value));
      XFree(prop.value);
      return title.empty() ? "Untitled" : title;
    }
    return "Untitled";
  }
}
