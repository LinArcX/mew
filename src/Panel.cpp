#include "Panel.hpp"

#include <alsa/asoundlib.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

Panel::Panel(XConnection& xconn, FontRenderer& font, ClientManager& clients)
  : m_xconn(xconn)
  , m_font(font)
  , m_clients(clients)
{
}

Panel::~Panel()
{
  hideMenus();
  Display* d = m_xconn.display();
  if (!d)
  {
    return;
  }
  if (m_startMenu != None)
  {
    XDestroyWindow(d, m_startMenu);
    m_startMenu = None;
  }
  if (m_powerMenu != None)
  {
    XDestroyWindow(d, m_powerMenu);
    m_powerMenu = None;
  }
  if (m_tooltip != None)
  {
    XDestroyWindow(d, m_tooltip);
    m_tooltip = None;
  }
  if (m_window != None)
  {
    XDestroyWindow(d, m_window);
    m_window = None;
  }
}

void Panel::setBackgroundColor(unsigned long color)
{
  m_bgColor = color;
  if (m_window != None)
  {
    XSetWindowBackground(m_xconn.display(), m_window, m_bgColor);
    draw();
  }
}

void Panel::setItemColor(unsigned long color)
{
  m_itemColor = color;
  draw();
}

void Panel::setHoverColor(unsigned long color)
{
  m_hoverColor = color;
  draw();
}

void Panel::create()
{
  Display* d = m_xconn.display();
  int screenW = m_xconn.width();
  int screenH = m_xconn.height();

  XSetWindowAttributes attrs{};
  attrs.override_redirect = True;
  attrs.background_pixel = m_bgColor;
  attrs.event_mask = ExposureMask | ButtonPressMask | PointerMotionMask | LeaveWindowMask;

  m_window = XCreateWindow(
    d, m_xconn.root(),
    0, screenH - MewConst::panelHeight, screenW, MewConst::panelHeight,
    0,
    CopyFromParent, InputOutput, CopyFromParent,
    CWOverrideRedirect | CWBackPixel | CWEventMask,
    &attrs);

  XMapRaised(d, m_window);
  draw();
}

void Panel::raise()
{
  if (m_window != None)
  {
    XRaiseWindow(m_xconn.display(), m_window);
  }
}

void Panel::updateVolume()
{
  snd_mixer_t* handle = nullptr;
  if (snd_mixer_open(&handle, 0) < 0)
  {
    return;
  }
  if (snd_mixer_attach(handle, "default") < 0)
  {
    snd_mixer_close(handle);
    return;
  }
  if (snd_mixer_selem_register(handle, nullptr, nullptr) < 0)
  {
    snd_mixer_close(handle);
    return;
  }
  if (snd_mixer_load(handle) < 0)
  {
    snd_mixer_close(handle);
    return;
  }

  snd_mixer_selem_id_t* sid = nullptr;
  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, "Master");

  snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);
  if (!elem)
  {
    snd_mixer_selem_id_set_name(sid, "PCM");
    elem = snd_mixer_find_selem(handle, sid);
  }

  if (elem)
  {
    if (snd_mixer_selem_has_playback_switch(elem))
    {
      int muted = 0;
      snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &muted);
      m_volumeMuted = (muted == 0);
    }
    else
    {
      m_volumeMuted = false;
    }

    long minv = 0;
    long maxv = 0;
    long valv = 0;
    snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv);
    snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &valv);
    if (maxv > minv)
    {
      m_volumePercent = static_cast<int>(((valv - minv) * 100) / (maxv - minv));
    }
    else
    {
      m_volumePercent = 0;
    }
  }

  snd_mixer_close(handle);
}

void Panel::toggleMute()
{
  // Prefer amixer (works with Pulse/PipeWire bridges). Fall back to ALSA selem.
  if (std::system("amixer -q set Master toggle 2>/dev/null") == 0
      || std::system("amixer -q set Master playback toggle 2>/dev/null") == 0)
  {
    updateVolume();
    draw();
    return;
  }

  snd_mixer_t* handle = nullptr;
  if (snd_mixer_open(&handle, 0) < 0)
  {
    return;
  }
  if (snd_mixer_attach(handle, "default") < 0)
  {
    snd_mixer_close(handle);
    return;
  }
  snd_mixer_selem_register(handle, nullptr, nullptr);
  snd_mixer_load(handle);

  snd_mixer_selem_id_t* sid = nullptr;
  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  const char* names[] = {"Master", "PCM", "Speaker", "Headphone", nullptr};
  snd_mixer_elem_t* elem = nullptr;
  for (int i = 0; names[i]; ++i)
  {
    snd_mixer_selem_id_set_name(sid, names[i]);
    elem = snd_mixer_find_selem(handle, sid);
    if (elem)
    {
      break;
    }
  }

  if (elem)
  {
    if (snd_mixer_selem_has_playback_switch(elem))
    {
      int on = 0;
      snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &on);
      snd_mixer_selem_set_playback_switch_all(elem, on ? 0 : 1);
    }
    else
    {
      // No mute switch: toggle volume 0 <-> saved level
      long minv = 0;
      long maxv = 0;
      long valv = 0;
      snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv);
      snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &valv);
      if (valv <= minv)
      {
        long mid = minv + (maxv - minv) * 50 / 100;
        snd_mixer_selem_set_playback_volume_all(elem, mid);
      }
      else
      {
        snd_mixer_selem_set_playback_volume_all(elem, minv);
      }
    }
  }

  snd_mixer_close(handle);
  updateVolume();
  draw();
}

void Panel::draw()
{
  if (m_window == None)
  {
    return;
  }

  Display* d = m_xconn.display();
  int screenW = m_xconn.width();
  GC gc = XCreateGC(d, m_window, 0, nullptr);

  XSetForeground(d, gc, m_bgColor);
  XFillRectangle(d, m_window, gc, 0, 0, screenW, MewConst::panelHeight);

  time_t now = time(nullptr);
  struct tm* tm = localtime(&now);
  char buf[64];
  // Full month name, e.g. "2026-September-07  23:57:01"
  // Icons: U+EAB0, U+E641 (Nerd Font PUA)
  char datePart[48];
  strftime(datePart, sizeof(datePart), "%Y-%B-%d", tm);
  char timePart[16];
  strftime(timePart, sizeof(timePart), "%H:%M:%S", tm);
  snprintf(buf, sizeof(buf), "\xee\xaa\xb0 %s \xee\x99\x81 %s", datePart, timePart);

  XftFont* pFont = m_font.font();
  int textH = pFont ? (pFont->ascent + pFont->descent) : 12;
  int baseline = (MewConst::panelHeight + textH) / 2 - (pFont ? pFont->descent : 2);

  // U+EB94 start icon
  m_font.draw(d, m_xconn.screen(), m_window, 12, baseline, "");

  // Hover highlight under interactive zones
  if (m_hoverZone == 0)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_window, gc, 0, 0, 40, MewConst::panelHeight);
  }
  else if (m_hoverZone == 1)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_window, gc, screenW - 340, 0, 95, MewConst::panelHeight);
  }
  else if (m_hoverZone == 2)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_window, gc, screenW - 50, 0, 50, MewConst::panelHeight);
  }

  updateVolume();
  char volBuf[48];
  if (m_volumeMuted || m_volumePercent < 0)
  {
    // U+F0581 mute
    snprintf(volBuf, sizeof(volBuf), "\xf3\xb0\x96\x81 mute");
  }
  else
  {
    snprintf(volBuf, sizeof(volBuf), "%d%%", m_volumePercent);
  }
  m_font.draw(d, m_xconn.screen(), m_window, screenW - 360, baseline, volBuf);
  m_font.draw(d, m_xconn.screen(), m_window, screenW - 305, baseline, buf);
  m_font.draw(d, m_xconn.screen(), m_window, screenW - 20, baseline, "");

  XFreeGC(d, gc);
  m_lastTime = now;
}

void Panel::drawMenuWindow(Window win, const std::vector<std::string>& items, int width)
{
  if (win == None)
  {
    return;
  }

  Display* d = m_xconn.display();
  int height = static_cast<int>(items.size()) * kMenuItemH;
  GC gc = XCreateGC(d, win, 0, nullptr);

  XSetForeground(d, gc, m_bgColor);
  XFillRectangle(d, win, gc, 0, 0, width, height);

  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, win, gc, 0, 0, width - 1, height - 1);

  XftFont* pFont = m_font.font();
  for (size_t i = 0; i < items.size(); ++i)
  {
    int y = static_cast<int>(i) * kMenuItemH;
    int baseline = y + (kMenuItemH + (pFont ? pFont->ascent : 10)) / 2 - 2;
    m_font.draw(d, m_xconn.screen(), win, 12, baseline, items[i]);
  }

  XFreeGC(d, gc);
}

void Panel::hidePowerMenu()
{
  if (m_powerMenu != None && m_powerMenuActive)
  {
    XUnmapWindow(m_xconn.display(), m_powerMenu);
  }
  m_powerMenuActive = false;
}

void Panel::hideMenus()
{
  hidePowerMenu();
  if (m_startMenu != None && m_startMenuActive)
  {
    XUnmapWindow(m_xconn.display(), m_startMenu);
  }
  m_startMenuActive = false;
}

void Panel::drawStartMenu()
{
  if (m_startMenu == None || !m_startMenuActive)
  {
    return;
  }
  static const std::vector<std::string> items = {
    "Apps",
    "KeyBindings",
    "PowerManager  >"
  };
  drawMenuWindow(m_startMenu, items, kStartMenuW);
}

void Panel::drawPowerMenu()
{
  if (m_powerMenu == None || !m_powerMenuActive)
  {
    return;
  }
  static const std::vector<std::string> items = {
    "Reconfigure mew",
    "Reboot",
    "Poweroff",
    "Logout"
  };
  drawMenuWindow(m_powerMenu, items, kPowerMenuW);
}

void Panel::showPowerMenu()
{
  Display* d = m_xconn.display();
  int height = 4 * kMenuItemH;
  int x = kStartMenuW;
  int y = m_xconn.height() - MewConst::panelHeight - height;

  if (m_powerMenu == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = m_bgColor;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    m_powerMenu = XCreateWindow(
      d, m_xconn.root(),
      x, y, kPowerMenuW, height, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_powerMenu, x, y, kPowerMenuW, height);
  }

  XMapRaised(d, m_powerMenu);
  m_powerMenuActive = true;
  drawPowerMenu();
}

void Panel::showStartMenu()
{
  hidePowerMenu();
  Display* d = m_xconn.display();
  int height = 3 * kMenuItemH;
  int x = 0;
  int y = m_xconn.height() - MewConst::panelHeight - height;

  if (m_startMenu == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = m_bgColor;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    m_startMenu = XCreateWindow(
      d, m_xconn.root(),
      x, y, kStartMenuW, height, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_startMenu, x, y, kStartMenuW, height);
  }

  XMapRaised(d, m_startMenu);
  m_startMenuActive = true;
  drawStartMenu();
}

void Panel::doReboot()
{
  sync();
  // logind (works without root if polkit allows), then classic binaries, then syscall
  if (std::system("dbus-send --system --print-reply "
                  "--dest=org.freedesktop.login1 "
                  "/org/freedesktop/login1 "
                  "org.freedesktop.login1.Manager.Reboot boolean:false "
                  ">/dev/null 2>&1") == 0)
  {
    return;
  }
  if (std::system("/usr/bin/reboot >/dev/null 2>&1 &") == 0
      || std::system("reboot >/dev/null 2>&1 &") == 0)
  {
    return;
  }
  reboot(RB_AUTOBOOT);
}

void Panel::doPoweroff()
{
  sync();
  if (std::system("dbus-send --system --print-reply "
                  "--dest=org.freedesktop.login1 "
                  "/org/freedesktop/login1 "
                  "org.freedesktop.login1.Manager.PowerOff boolean:false "
                  ">/dev/null 2>&1") == 0)
  {
    return;
  }
  if (std::system("/usr/bin/poweroff >/dev/null 2>&1 &") == 0
      || std::system("poweroff >/dev/null 2>&1 &") == 0)
  {
    return;
  }
  reboot(RB_POWER_OFF);
}

void Panel::toggleDesktop()
{
  if (!m_desktopShowing)
  {
    for (Client* pClient : m_clients.clients())
    {
      if (!pClient->minimized)
      {
        m_clients.minimize(pClient);
      }
    }
    m_desktopShowing = true;
  }
  else
  {
    for (Client* pClient : m_clients.clients())
    {
      if (pClient->minimized)
      {
        pClient->minimized = false;
        XMapWindow(m_xconn.display(), pClient->frame);
      }
    }
    m_desktopShowing = false;
    if (!m_clients.clients().empty())
    {
      m_clients.focus(m_clients.clients().back());
    }
  }
}

void Panel::handleClick(int x)
{
  int screenW = m_xconn.width();

  // Start button (left)
  if (x < 40)
  {
    if (m_startMenuActive)
    {
      hideMenus();
    }
    else
    {
      showStartMenu();
    }
    return;
  }

  // Volume drawn at screenW-320; clock at screenW-240; desktop at screenW-36
  // Hit volume from just left of the text through before the clock.
  if (x >= screenW - 340 && x < screenW - 245)
  {
    toggleMute();
    return;
  }

  // Desktop icon (far right)
  if (x > screenW - 50)
  {
    toggleDesktop();
  }
}


int Panel::hitTest(int x) const
{
  int screenW = m_xconn.width();
  if (x < 40)
  {
    return 0; // start
  }
  if (x >= screenW - 340 && x < screenW - 245)
  {
    return 1; // volume
  }
  if (x > screenW - 50)
  {
    return 2; // desktop
  }
  return -1;
}

void Panel::hideTooltip()
{
  if (m_tooltip != None)
  {
    XUnmapWindow(m_xconn.display(), m_tooltip);
  }
}

void Panel::showTooltip(int x, const char* text)
{
  Display* d = m_xconn.display();
  int screenH = m_xconn.height();
  int tw = static_cast<int>(std::strlen(text)) * 9 + 16;
  int th = 22;
  int tx = x;
  int ty = screenH - MewConst::panelHeight - th - 4;
  if (tx + tw > m_xconn.width())
  {
    tx = m_xconn.width() - tw;
  }
  if (tx < 0)
  {
    tx = 0;
  }

  if (m_tooltip == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0x1a1a1a;
    attrs.border_pixel = 0x888888;
    attrs.event_mask = ExposureMask;
    m_tooltip = XCreateWindow(
      d, m_xconn.root(),
      tx, ty, tw, th, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_tooltip, tx, ty, tw, th);
  }

  XMapRaised(d, m_tooltip);
  GC gc = XCreateGC(d, m_tooltip, 0, nullptr);
  XSetForeground(d, gc, 0x1a1a1a);
  XFillRectangle(d, m_tooltip, gc, 0, 0, tw, th);
  m_font.draw(d, m_xconn.screen(), m_tooltip, 8, 16, text);
  XFreeGC(d, gc);
}

void Panel::handleMotion(int x)
{
  int zone = hitTest(x);
  if (zone == m_hoverZone)
  {
    return;
  }
  m_hoverZone = zone;
  draw();
  if (zone == 0)
  {
    showTooltip(x, "Start menu");
  }
  else if (zone == 1)
  {
    showTooltip(x, "Volume (click to mute)");
  }
  else if (zone == 2)
  {
    showTooltip(x, "Show desktop");
  }
  else
  {
    hideTooltip();
  }
}

void Panel::handleLeave()
{
  m_hoverZone = -1;
  hideTooltip();
  draw();
}

void Panel::handleStartMenuClick(int y)
{
  int index = y / kMenuItemH;
  if (index == 0)
  {
    hideMenus();
    if (m_onShowLauncher)
    {
      m_onShowLauncher();
    }
  }
  else if (index == 1)
  {
    hideMenus();
    if (m_onShowKeybindings)
    {
      m_onShowKeybindings();
    }
  }
  else if (index == 2)
  {
    if (m_powerMenuActive)
    {
      hidePowerMenu();
    }
    else
    {
      showPowerMenu();
    }
  }
}

void Panel::handlePowerMenuClick(int y)
{
  int index = y / kMenuItemH;
  hideMenus();

  if (index == 0)
  {
    if (m_onReconfigure)
    {
      m_onReconfigure();
    }
  }
  else if (index == 1)
  {
    doReboot();
  }
  else if (index == 2)
  {
    doPoweroff();
  }
  else if (index == 3)
  {
    if (m_onQuit)
    {
      m_onQuit();
    }
  }
}
