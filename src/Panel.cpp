#include "Panel.hpp"
#include <ctime>
#include <algorithm>
#include <fstream>
#include <dirent.h>

#include <X11/XKBlib.h>

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
  if (m_netMenu != None)
  {
    XDestroyWindow(d, m_netMenu);
    m_netMenu = None;
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


void Panel::refreshLayout()
{
  Display* d = m_xconn.display();
  XkbStateRec state{};
  if (XkbGetState(d, XkbUseCoreKbd, &state) == Success)
  {
    m_layoutGroup = static_cast<int>(state.group);
  }

  XkbDescPtr desc = XkbAllocKeyboard();
  if (!desc)
  {
    m_layoutName = "??";
    m_layoutCount = 1;
    return;
  }

  desc->device_spec = XkbUseCoreKbd;
  if (XkbGetNames(d, XkbGroupNamesMask, desc) != Success)
  {
    XkbFreeKeyboard(desc, 0, True);
    m_layoutName = "??";
    return;
  }

  m_layoutCount = 0;
  for (int i = 0; i < XkbNumKbdGroups; ++i)
  {
    if (desc->names && desc->names->groups[i] != None)
    {
      ++m_layoutCount;
    }
  }
  if (m_layoutCount < 1)
  {
    m_layoutCount = 1;
  }

  m_layoutName = "??";
  if (desc->names
      && m_layoutGroup >= 0
      && m_layoutGroup < XkbNumKbdGroups
      && desc->names->groups[m_layoutGroup] != None)
  {
    char* name = XGetAtomName(d, desc->names->groups[m_layoutGroup]);
    if (name)
    {
      // Often "English (US)" — show a short token
      m_layoutName = name;
      if (m_layoutName.size() > 8)
      {
        // Prefer 2-letter codes if present in parentheses: English (US) -> US
        size_t l = m_layoutName.rfind('(');
        size_t r = m_layoutName.rfind(')');
        if (l != std::string::npos && r != std::string::npos && r > l + 1)
        {
          m_layoutName = m_layoutName.substr(l + 1, r - l - 1);
        }
        else
        {
          m_layoutName = m_layoutName.substr(0, 6);
        }
      }
      XFree(name);
    }
  }

  XkbFreeKeyboard(desc, 0, True);
}

void Panel::cycleLayout()
{
  Display* d = m_xconn.display();
  refreshLayout();
  int next = (m_layoutGroup + 1) % m_layoutCount;
  XkbLockGroup(d, XkbUseCoreKbd, static_cast<unsigned>(next));
  refreshLayout();
  draw();
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

  // Hover highlight under interactive zones
  // Layout from right: desktop | clock | volume | language | network | kill
  if (m_hoverZone == 0)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_window, gc, 0, 0, 40, MewConst::panelHeight);
  }
  else if (m_hoverZone == 1)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_window, gc, screenW - 480, 0, 28, MewConst::panelHeight);
  }
  else if (m_hoverZone == 2)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_window, gc, screenW - 450, 0, 70, MewConst::panelHeight);
  }
  else if (m_hoverZone == 3)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_window, gc, screenW - 380, 0, 30, MewConst::panelHeight);
  }
  else if (m_hoverZone == 4)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_window, gc, screenW - 350, 0, 35, MewConst::panelHeight);
  }
  else if (m_hoverZone == 5)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_window, gc, screenW - 25, 0, 30, MewConst::panelHeight);
  }

  // U+EB94 start icon
  m_font.draw(d, m_xconn.screen(), m_window, 12, baseline, "\xee\xae\x94");

  refreshLayout();
  updateVolume();
  refreshNetwork();

  // Kill-switch (green=up, red=down)
  bool ifaceUp = !m_selectedIface.empty() && isInterfaceUp(m_selectedIface);
  const char* killIcon = ifaceUp ? "\xf3\xb0\x8c\xa0" : "\xf3\xb0\x8c\xa1"; // placeholder; color via prefix text
  // Draw colored indicator using GC + short label
  XSetForeground(d, gc, ifaceUp ? 0x22cc44 : 0xcc2222);
  XFillRectangle(d, m_window, gc, screenW - 476, 6, 16, 16);
  m_font.draw(d, m_xconn.screen(), m_window, screenW - 478, baseline, " ");

  // Network interface name
  std::string netLabel = m_selectedIface.empty() ? "net" : m_selectedIface;
  if (netLabel.size() > 8)
  {
    netLabel = netLabel.substr(0, 8);
  }
  m_font.draw(d, m_xconn.screen(), m_window, screenW - 448, baseline, netLabel);

  // Language (left of volume)
  m_font.draw(d, m_xconn.screen(), m_window, screenW - 375, baseline, m_layoutName);

  char volBuf[48];
  if (m_volumeMuted || m_volumePercent < 0)
  {
    // U+F0581 mute
    snprintf(volBuf, sizeof(volBuf), "\xf3\xb0\x96\x81");
  }
  else
  {
    snprintf(volBuf, sizeof(volBuf), "%d%%", m_volumePercent);
  }
  m_font.draw(d, m_xconn.screen(), m_window, screenW - 345, baseline, volBuf);
  m_font.draw(d, m_xconn.screen(), m_window, screenW - 305, baseline, buf);
  m_font.draw(d, m_xconn.screen(), m_window, screenW - 20, baseline, "\xef\x92\xa9");
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
  hideNetworkMenu();
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


void Panel::refreshNetwork()
{
  m_netIfaces.clear();
  // Prefer /sys/class/net (no shell)
  DIR* dir = opendir("/sys/class/net");
  if (dir)
  {
    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr)
    {
      std::string name = ent->d_name;
      if (name == "." || name == ".." || name == "lo")
      {
        continue;
      }
      m_netIfaces.push_back(name);
    }
    closedir(dir);
    std::sort(m_netIfaces.begin(), m_netIfaces.end());
  }

  if (m_selectedIface.empty() ||
      std::find(m_netIfaces.begin(), m_netIfaces.end(), m_selectedIface) == m_netIfaces.end())
  {
    m_selectedIface.clear();
    for (const std::string& n : m_netIfaces)
    {
      if (isInterfaceUp(n))
      {
        m_selectedIface = n;
        break;
      }
    }
    if (m_selectedIface.empty() && !m_netIfaces.empty())
    {
      m_selectedIface = m_netIfaces.front();
    }
  }
}

bool Panel::isInterfaceUp(const std::string& name) const
{
  if (name.empty())
  {
    return false;
  }
  std::string path = "/sys/class/net/" + name + "/operstate";
  std::ifstream f(path);
  if (!f.is_open())
  {
    return false;
  }
  std::string state;
  f >> state;
  return state == "up";
}

void Panel::hideNetworkMenu()
{
  if (m_netMenu != None && m_netMenuActive)
  {
    XUnmapWindow(m_xconn.display(), m_netMenu);
  }
  m_netMenuActive = false;
}

void Panel::drawNetworkMenu()
{
  if (m_netMenu == None || !m_netMenuActive)
  {
    return;
  }
  Display* d = m_xconn.display();
  int height = std::max(1, static_cast<int>(m_netIfaces.size())) * kNetMenuItemH;
  GC gc = XCreateGC(d, m_netMenu, 0, nullptr);
  XSetForeground(d, gc, m_bgColor);
  XFillRectangle(d, m_netMenu, gc, 0, 0, kNetMenuW, height);
  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_netMenu, gc, 0, 0, kNetMenuW - 1, height - 1);

  XftFont* pFont = m_font.font();
  if (m_netIfaces.empty())
  {
    int baseline = (kNetMenuItemH + (pFont ? pFont->ascent : 10)) / 2 - 2;
    m_font.draw(d, m_xconn.screen(), m_netMenu, 12, baseline, "(no interfaces)");
  }
  else
  {
    for (size_t i = 0; i < m_netIfaces.size(); ++i)
    {
      int y = static_cast<int>(i) * kNetMenuItemH;
      if (m_netIfaces[i] == m_selectedIface)
      {
        XSetForeground(d, gc, m_hoverColor);
        XFillRectangle(d, m_netMenu, gc, 2, y + 1, kNetMenuW - 4, kNetMenuItemH - 2);
      }
      std::string line = m_netIfaces[i];
      if (isInterfaceUp(m_netIfaces[i]))
      {
        line += "  [up]";
      }
      else
      {
        line += "  [down]";
      }
      int baseline = y + (kNetMenuItemH + (pFont ? pFont->ascent : 10)) / 2 - 2;
      m_font.draw(d, m_xconn.screen(), m_netMenu, 12, baseline, line);
    }
  }
  XFreeGC(d, gc);
}

void Panel::showNetworkMenu()
{
  refreshNetwork();
  Display* d = m_xconn.display();
  int height = std::max(1, static_cast<int>(m_netIfaces.size())) * kNetMenuItemH;
  int screenH = m_xconn.height();
  int x = m_xconn.width() - 450;
  int y = screenH - MewConst::panelHeight - height;

  if (m_netMenu == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = m_bgColor;
    attrs.event_mask = ExposureMask | ButtonPressMask;
    m_netMenu = XCreateWindow(
      d, m_xconn.root(),
      x, y, kNetMenuW, height, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_netMenu, x, y, kNetMenuW, height);
  }
  XMapRaised(d, m_netMenu);
  m_netMenuActive = true;
  drawNetworkMenu();
}

void Panel::handleNetworkMenuClick(int y)
{
  int index = y / kNetMenuItemH;
  hideNetworkMenu();
  if (index >= 0 && index < static_cast<int>(m_netIfaces.size()))
  {
    m_selectedIface = m_netIfaces[static_cast<size_t>(index)];
    draw();
  }
}

void Panel::toggleKillSwitch()
{
  if (m_selectedIface.empty())
  {
    refreshNetwork();
  }
  if (m_selectedIface.empty())
  {
    return;
  }

  bool up = isInterfaceUp(m_selectedIface);
  std::string cmd;
  if (up)
  {
    // stop internet
    cmd = "ip link set dev " + m_selectedIface + " down >/dev/null 2>&1";
  }
  else
  {
    cmd = "ip link set dev " + m_selectedIface + " up >/dev/null 2>&1";
  }
  // try without sudo, then with sudo -n (non-interactive)
  if (std::system(cmd.c_str()) != 0)
  {
    std::string sudoCmd = "sudo -n " + cmd;
    std::system(sudoCmd.c_str());
  }
  // brief settle
  struct timespec ts = {0, 150 * 1000 * 1000};
  nanosleep(&ts, nullptr);
  draw();
}

void Panel::handleClick(int x)
{
  int zone = hitTest(x);
  if (zone == 0)
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
  if (zone == 1)
  {
    toggleKillSwitch();
    return;
  }
  if (zone == 2)
  {
    if (m_netMenuActive)
    {
      hideNetworkMenu();
    }
    else
    {
      showNetworkMenu();
    }
    return;
  }
  if (zone == 3)
  {
    cycleLayout();
    return;
  }
  if (zone == 4)
  {
    toggleMute();
    return;
  }
  if (zone == 5)
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
  if (x >= screenW - 480 && x < screenW - 452)
  {
    return 1; // kill-switch
  }
  if (x >= screenW - 450 && x < screenW - 380)
  {
    return 2; // network
  }
  if (x >= screenW - 380 && x < screenW - 350)
  {
    return 3; // language
  }
  if (x >= screenW - 350 && x < screenW - 310)
  {
    return 4; // volume
  }
  if (x > screenW - 30)
  {
    return 5; // desktop
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
    showTooltip(x, "Internet kill-switch");
  }
  else if (zone == 2)
  {
    showTooltip(x, "Network interface");
  }
  else if (zone == 3)
  {
    showTooltip(x, "Keyboard layout (click to cycle)");
  }
  else if (zone == 4)
  {
    showTooltip(x, "Volume (click to mute)");
  }
  else if (zone == 5)
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
