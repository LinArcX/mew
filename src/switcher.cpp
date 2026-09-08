#include "switcher.h"

namespace Mew
{
  void Switcher::hide(Display* display)
  {
    if (switcher != None && switcher_active) {
      XUnmapWindow(display, switcher);
    }
  
    // NEW: release the keyboard grab we took in show()
    if (switcher_active) {
      XUngrabKeyboard(display, CurrentTime);
    }
  
    switcher_active = false;
    switcher_list.clear();
  }
  
  void Switcher::draw(const int screen,
    Display* display,
    Windows* const windows)
  {
    if (switcher == None || !switcher_active || switcher_list.empty()) {
      return;
    }
  
    GC gc = XCreateGC(display, switcher, 0, nullptr);
    int height = SWITCHER_PAD * 2 + (int)switcher_list.size() * SWITCHER_LINE_H;
  
    // Background
    XSetForeground(display, gc, COLOR_SWITCHER_BG);
    XFillRectangle(display, switcher, gc, 0, 0, SWITCHER_WIDTH, height);
  
    // Border
    XSetForeground(display, gc, COLOR_SWITCHER_BORDER);
    XDrawRectangle(display, switcher, gc, 0, 0, SWITCHER_WIDTH - 1, height - 1);
  
    for (size_t i = 0; i < switcher_list.size(); ++i) {
      int y = SWITCHER_PAD + (int)i * SWITCHER_LINE_H;
  
      if (i == switcher_index) {
        XSetForeground(display, gc, COLOR_SWITCHER_HL);
        XFillRectangle(display, switcher, gc,
                       4, y,
                       SWITCHER_WIDTH - 8, SWITCHER_LINE_H);
      }
  
      std::string title = windows->getWindowTitle(display, switcher_list[i]->window);
      if (title.size() > 48) {
        title = title.substr(0, 45) + "...";
      }
  
      int baseline = y + (SWITCHER_LINE_H + (windows->getTitleFont() ? windows->getTitleFont()->ascent : 10)) / 2 - 2;
      windows->drawTitleText(switcher, SWITCHER_PAD + 6, baseline, title, screen, display);
    }
  
    XFreeGC(display, gc);
  }
  
  void Switcher::show(Window root,
    const int screen,
    Display* display,
    Windows* const windows)
  {
    if (switcher_list.empty()) {
      return;
    }
  
    int height = SWITCHER_PAD * 2 + (int)switcher_list.size() * SWITCHER_LINE_H;
    int screen_w = DisplayWidth(display, screen);
    int screen_h = DisplayHeight(display, screen);
    int x = (screen_w - SWITCHER_WIDTH) / 2;
    int y = (screen_h - height) / 2;
  
    if (switcher == None) {
      XSetWindowAttributes attrs{};
      attrs.override_redirect = True;
      attrs.background_pixel  = COLOR_SWITCHER_BG;
      attrs.border_pixel      = COLOR_SWITCHER_BORDER;
      attrs.event_mask        = ExposureMask;
  
      switcher = XCreateWindow(display, root,
        x, y, SWITCHER_WIDTH, height,
        1,  // border width
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
        &attrs);
    }
    else {
      XMoveResizeWindow(display, switcher, x, y, SWITCHER_WIDTH, height);
    }
    XMapRaised(display, switcher);
  
    // NEW: grab the keyboard so we reliably see the Alt release
    // regardless of which window has input focus.
    XGrabKeyboard(display, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);
    switcher_active = true;
    draw(screen, display, windows);
  }
}
