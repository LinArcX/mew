#ifndef MEW_WINDOWS_H
#define MEW_WINDOWS_H

#include <string>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

namespace Mew
{
  class Windows
  {
    public:
      void drawTitleText(Window window,
        int x,
        int y,
        const std::string& text,
        const int screen,
        Display * display) noexcept;

      void ensureTitleTextColor(const int screen,
        Display * display) noexcept;

      std::string getWindowTitle(Display * display,
        Window window);
 
      const XftFont * const getTitleFont() const noexcept { return m_titleFont; }

    private:
      XftColor m_titleTextColor;
      bool m_bIstitleTextColorReady = false;

      XftFont* m_titleFont = nullptr;

      const unsigned long COLOR_BORDER = 0x333333;
      const unsigned long COLOR_TITLE  = 0x444444;
      const unsigned long COLOR_BUTTON = 0x555555;
      const unsigned long COLOR_TEXT   = 0xffffff;
  };
}

#endif // MEW_WINDOWS_H
