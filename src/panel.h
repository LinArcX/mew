#ifndef MEW_PANEL_H
#define MEW_PANEL_H

#include "audio.h"
#include "windows.h"

#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

namespace Mew
{
  class Panel
  {
    public:
      void createPanel(
        const XftFont * const titleFont,
        Audio * const audio,
        Window root,
        const int screen,
        Display * display,
        Windows * const windows);

      void drawPanel(
        const XftFont * const titleFont,
        Audio * const audio,
        const int screen,
        Display * display,
        Windows * const windows);

      void handlePanelClick(int x,
        const XftFont * const titleFont,
        Audio * const audio,
        const int screen,
        Display * display,
        Windows * const windows);

      void setColorBackground(unsigned long color) noexcept { m_colorBackground = color; }

      [[nodiscard]] unsigned long getColorBackground() const noexcept { return m_colorBackground; }

    private:
      Window m_panel = None;
      const uint16_t m_height = 30;
      unsigned long m_colorBackground = 0x222222;
      unsigned long m_colorText = 0xffffff;
      time_t m_lastTime = 0;
      bool m_desktopShowing = false;
      
      //const int PANEL_HEIGHT = 28;
      //unsigned long COLOR_PANEL_BG = 0x222222;
      //const unsigned long COLOR_PANEL_TEXT = ;

      // to be decided where should put these
      //int screen;
      //Window root;
      //Display* display = nullptr;
  };
}

#endif // MEW_PANEL_H
