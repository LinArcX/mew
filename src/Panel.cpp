#include "Panel.hpp"

#include <alsa/asoundlib.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <cstdio>
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

void Panel::create()
{
  Display* d = m_xconn.display();
  int screenW = m_xconn.width();
  int screenH = m_xconn.height();

  XSetWindowAttributes attrs{};
  attrs.override_redirect = True;
  attrs.background_pixel = m_bgColor;
  attrs.event_mask = ExposureMask | ButtonPressMask;

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
  snd_mixer_selem_id_set_name(sid, "Master");

  snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);
  if (!elem)
  {
    snd_mixer_selem_id_set_name(sid, "PCM");
    elem = snd_mixer_find_selem(handle, sid);
  }

  if (elem && snd_mixer_selem_has_playback_switch(elem))
  {
    int muted = 0;
    snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &muted);
    int newState = muted ? 0 : 1;
    snd_mixer_selem_set_playback_switch_all(elem, newState);
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
  strftime(buf, sizeof(buf), " %Y-%B-%d  %H:%M:%S", tm);

  XftFont* pFont = m_font.font();
  int textH = pFont ? (pFont->ascent + pFont->descent) : 12;
  int baseline = (MewConst::panelHeight + textH) / 2 - (pFont ? pFont->descent : 2);

  m_font.draw(d, m_xconn.screen(), m_window, 12, baseline, "");

  updateVolume();
  char volBuf[32];
  if (m_volumeMuted || m_volumePercent < 0)
  {
    snprintf(volBuf, sizeof(volBuf), "󰖁 mute");
  }
  else if (m_volumePercent < 30) {
    snprintf(volBuf, sizeof(volBuf), "󰕿 %d%%", m_volumePercent);
  }
  else if (m_volumePercent < 70) {
    snprintf(volBuf, sizeof(volBuf), "󰖀 %d%%", m_volumePercent);
  }
  else {
    snprintf(volBuf, sizeof(volBuf), "󰕾 %d%%", m_volumePercent);
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
  if (reboot(RB_AUTOBOOT) != 0)
  {
    execl("/bin/reboot", "reboot", static_cast<char*>(nullptr));
    execl("/sbin/reboot", "reboot", static_cast<char*>(nullptr));
    execl("/usr/bin/reboot", "reboot", static_cast<char*>(nullptr));
  }
}

void Panel::doPoweroff()
{
  sync();
  if (reboot(RB_POWER_OFF) != 0)
  {
    execl("/bin/poweroff", "poweroff", static_cast<char*>(nullptr));
    execl("/sbin/poweroff", "poweroff", static_cast<char*>(nullptr));
    execl("/usr/bin/poweroff", "poweroff", static_cast<char*>(nullptr));
  }
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

  if (x > screenW - 330 && x < screenW - 250)
  {
    toggleMute();
    return;
  }

  if (x > screenW - 50)
  {
    toggleDesktop();
  }
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
