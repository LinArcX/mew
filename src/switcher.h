#ifndef MEW_SWITCHER_H
#define MEW_SWITCHER_H

#include "core.h"
#include "windows.h"

#include <vector>

#include <X11/Xlib.h>

namespace Mew
{
  class Switcher
  {
    public:
      void draw(
        const int screen,
        Display* display,
        Windows * const windows);

      void show(
        Window root,
        const int screen,
        Display* display,
        Windows* const windows);

      void hide(Display * display);
 
    private:
      Window switcher = None;
      bool switcher_active = false;
      size_t switcher_index = 0;
      std::vector<Client*> switcher_list;
      
      const int SWITCHER_WIDTH = 420;
      const int SWITCHER_LINE_H = 30;
      const int SWITCHER_PAD = 12;
      const unsigned long COLOR_SWITCHER_BG     = 0x1e1e1e;
      const unsigned long COLOR_SWITCHER_BORDER = 0x555555;
      const unsigned long COLOR_SWITCHER_HL     = 0x0a64c8;
      const unsigned long COLOR_SWITCHER_TEXT   = 0xffffff;
  };
}

#endif // MEW_SWITCHER_H
