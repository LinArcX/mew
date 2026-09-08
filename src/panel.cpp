#include "panel.h"
#include "core.h"

#include <ctime>

namespace Mew
{
  void Panel::createPanel(
    const XftFont * const titleFont,
    Audio * const audio,
    Window root,
    const int screen,
    Display * display,
    Windows * const windows)
  {
    int screenW = DisplayWidth(display, screen);
    int screen_h = DisplayHeight(display, screen);
  
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = m_colorBackground;
    attrs.event_mask = ExposureMask | ButtonPressMask;
  
    m_panel = XCreateWindow(
      display, root,
      0, screen_h - m_height, screenW, m_height,
      0,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs
    );
  
    XMapRaised(display, m_panel);
    drawPanel(titleFont, audio, screen, display, windows);
  }
  
  void Panel::drawPanel(
    const XftFont * const titleFont,
    Audio * const audio,
    const int screen,
    Display * display,
    Windows * const windows)
  {
    if (m_panel == None) {
      return;
    }
    GC gc = XCreateGC(display, m_panel, 0, nullptr);
    XSetForeground(display, gc, m_colorBackground);
  
    int screenW = DisplayWidth(display, screen);
    XFillRectangle(display, m_panel, gc, 0, 0, screenW, m_height);
  
    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    char buf[64];
    // Full month name, e.g. "2026-September-07  23:57:01"
    strftime(buf, sizeof(buf), " %Y-%B-%d  %H:%M:%S", tm);
  
    int textH = titleFont ? (titleFont->ascent + titleFont->descent) : 12;
    int baseline = (m_height + textH) / 2 - (titleFont ? titleFont->descent : 2);
  
    // Left: Start button
    windows->drawTitleText(m_panel, 10, baseline, " ", screen, display);
  
    // Volume (left of clock)
    audio->updateVolume();
    char volBuf[32];
    if (audio->isVolumeMuted() || audio->getVolumePercent() < 0) {
      snprintf(volBuf, sizeof(volBuf), "󰖁 mute");
    }
    else if (audio->getVolumePercent() < 30) {
      snprintf(volBuf, sizeof(volBuf), "󰕿 %d%%", audio->getVolumePercent());
    }
    else if (audio->getVolumePercent() < 70) {
      snprintf(volBuf, sizeof(volBuf), "󰖀 %d%%", audio->getVolumePercent());
    }
    else {
      snprintf(volBuf, sizeof(volBuf), "󰕾 %d%%", audio->getVolumePercent());
    }
  
    windows->drawTitleText(m_panel, 70, baseline, volBuf, screen, display);
  
    // Right corner: time then Desktop (far right)
    windows->drawTitleText(m_panel, screenW - 305, baseline, buf, screen, display);
    windows->drawTitleText(m_panel, screenW - 20, baseline, "", screen, display);
  
    XFreeGC(display, gc);
    m_lastTime = now;
  }

  void Panel::handlePanelClick(
    int x,
    const XftFont * const titleFont,
    Audio * const audio,
    const int screen,
    Display * display,
    Windows * const windows)
  {
    int screenW = DisplayWidth(display, screen);
  
    // Left: Start menu
    if (x < 40) {
      if (start_menu_active) {
        hide_start_menu();
      }
      else {
        show_start_menu();
      }
      return;
    }
  
    // Volume toggle (approx left of clock)
    if (x > screenW - 330 && x < screenW - 250) {
      audio->toggleMute();
      drawPanel(titleFont, audio, screen, display, windows);
      return;
    }
  
    // Right: Desktop icon (far right)
    if (x > screenW - 50) {
      if (!m_desktopShowing) {
        for (Client* c : core::clients) {
          if (!c->minimized) {
            minimize_client(c);
          }
        }
        m_desktopShowing = true;
      }
      else {
        for (Client* c : clients) {
          if (c->minimized) {
            c->minimized = false;
            XMapWindow(display, c->frame);
          }
        }
        m_desktopShowing = false;
        if (!clients.empty()) {
          focus_client(clients.back());
        }
      }
      return;
    }
  }
}
