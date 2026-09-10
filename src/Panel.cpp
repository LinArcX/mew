#include "Panel.hpp"
#include "mew_icon_data.h"

#include <ctime>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <dirent.h>

#include "stb_image.h"
#include <X11/Xutil.h>
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
  if (m_backBuffer != None)
  {
    XFreePixmap(d, m_backBuffer);
    m_backBuffer = None;
  }
  if (m_window != None)
  {
    XDestroyWindow(d, m_window);
    m_window = None;
  }
}

void Panel::loadStartIcon()
{
  if (!m_startIconRgba.empty())
  {
    return;
  }

  int w = 0;
  int h = 0;
  int ch = 0;
  unsigned char* data = stbi_load_from_memory(mew_icon_png,
    static_cast<int>(mew_icon_png_len),
    &w, &h, &ch, 4);
  if (!data || w <= 0 || h <= 0)
  {
    if (data)
    {
      stbi_image_free(data);
    }
    fprintf(stderr, "mew: could not decode embedded mew.png\n");
    return;
  }


  //if (!m_startIconRgba.empty())
  //{
  //  return;
  //}

  //int w = 0;
  //int h = 0;
  //int ch = 0;
  //unsigned char* data = stbi_load("assets/mew.png", &w, &h, &ch, 4);
  //if (!data || w <= 0 || h <= 0)
  //{
  //  if (data)
  //  {
  //    stbi_image_free(data);
  //  }
  //  fprintf(stderr, "mew: could not load assets/mew.png\n");
  //  return;
  //}

  // If the PNG has no alpha, treat its black background as transparent
  // (unpremultiply from black).
  bool hasAlpha = false;
  for (int i = 0; i < w * h; ++i)
  {
    if (data[i * 4 + 3] != 255)
    {
      hasAlpha = true;
      break;
    }
  }
  if (!hasAlpha)
  {
    for (int i = 0; i < w * h; ++i)
    {
      unsigned char* p = data + i * 4;
      unsigned char mx = p[0];
      if (p[1] > mx) mx = p[1];
      if (p[2] > mx) mx = p[2];
      if (mx == 0)
      {
        p[0] = p[1] = p[2] = 0;
        p[3] = 0;
      }
      else
      {
        p[0] = static_cast<unsigned char>(p[0] * 255 / mx);
        p[1] = static_cast<unsigned char>(p[1] * 255 / mx);
        p[2] = static_cast<unsigned char>(p[2] * 255 / mx);
        p[3] = mx;
      }
    }
  }

  // Box-filter downscale to kStartIconSize x kStartIconSize (premultiplied by alpha).
  const int size = kStartIconSize;
  m_startIconRgba.assign(static_cast<size_t>(size) * size * 4, 0);
  for (int py = 0; py < size; ++py)
  {
    for (int px = 0; px < size; ++px)
    {
      int x0 = px * w / size;
      int y0 = py * h / size;
      int x1 = (px + 1) * w / size;
      int y1 = (py + 1) * h / size;
      if (x1 <= x0) x1 = x0 + 1;
      if (y1 <= y0) y1 = y0 + 1;
      unsigned int r = 0;
      unsigned int g = 0;
      unsigned int b = 0;
      unsigned int a = 0;
      unsigned int n = 0;
      for (int sy = y0; sy < y1; ++sy)
      {
        for (int sx = x0; sx < x1; ++sx)
        {
          const unsigned char* s = data + (sy * w + sx) * 4;
          // Premultiply by alpha so averaging is correct.
          r += static_cast<unsigned int>(s[0]) * s[3] / 255;
          g += static_cast<unsigned int>(s[1]) * s[3] / 255;
          b += static_cast<unsigned int>(s[2]) * s[3] / 255;
          a += s[3];
          ++n;
        }
      }
      if (n == 0) n = 1;
      unsigned char* d = m_startIconRgba.data() + (py * size + px) * 4;
      d[0] = static_cast<unsigned char>(r / n);
      d[1] = static_cast<unsigned char>(g / n);
      d[2] = static_cast<unsigned char>(b / n);
      d[3] = static_cast<unsigned char>(a / n);
    }
  }

  stbi_image_free(data);
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
  loadStartIcon();
  draw();
}

void Panel::raise()
{
  if (m_window != None)
  {
    XRaiseWindow(m_xconn.display(), m_window);
  }
}

void Panel::setVisible(bool visible)
{
  if (m_window == None)
  {
    return;
  }
  if (visible)
  {
    XMapRaised(m_xconn.display(), m_window);
  }
  else
  {
    XUnmapWindow(m_xconn.display(), m_window);
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

  // Prefer short codes from setxkbmap layout list (us,ir -> IR)
  {
    FILE* pipe = popen("setxkbmap -query 2>/dev/null", "r");
    if (pipe)
    {
      char line[256];
      std::string layoutsLine;
      while (fgets(line, sizeof(line), pipe))
      {
        if (std::strncmp(line, "layout:", 7) == 0)
        {
          layoutsLine = line + 7;
          break;
        }
      }
      pclose(pipe);
      // trim
      size_t start = layoutsLine.find_first_not_of(" \t\n\r");
      size_t end = layoutsLine.find_last_not_of(" \t\n\r");
      if (start != std::string::npos)
      {
        layoutsLine = layoutsLine.substr(start, end - start + 1);
        std::vector<std::string> codes;
        std::string cur;
        for (char c : layoutsLine)
        {
          if (c == ',')
          {
            if (!cur.empty())
            {
              codes.push_back(cur);
              cur.clear();
            }
          }
          else if (c != ' ')
          {
            cur.push_back(c);
          }
        }
        if (!cur.empty())
        {
          codes.push_back(cur);
        }
        if (!codes.empty())
        {
          m_layoutCount = static_cast<int>(codes.size());
          int idx = m_layoutGroup;
          if (idx < 0)
          {
            idx = 0;
          }
          if (idx >= m_layoutCount)
          {
            idx = m_layoutCount - 1;
          }
          m_layoutName = codes[static_cast<size_t>(idx)];
          for (char& c : m_layoutName)
          {
            if (c >= 'a' && c <= 'z')
            {
              c = static_cast<char>(c - 32);
            }
          }
        }
      }
    }
  }

  if (m_layoutName == "??" && desc->names
      && m_layoutGroup >= 0
      && m_layoutGroup < XkbNumKbdGroups
      && desc->names->groups[m_layoutGroup] != None)
  {
    char* name = XGetAtomName(d, desc->names->groups[m_layoutGroup]);
    if (name)
    {
      m_layoutName = name;
      size_t l = m_layoutName.rfind('(');
      size_t r = m_layoutName.rfind(')');
      if (l != std::string::npos && r != std::string::npos && r > l + 1)
      {
        m_layoutName = m_layoutName.substr(l + 1, r - l - 1);
      }
      else if (m_layoutName.size() > 6)
      {
        m_layoutName = m_layoutName.substr(0, 6);
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

void Panel::resolveVolumeControl(std::string& device, std::string& control) const
{
  device.clear();
  control = "Master";

  if (!m_pConfig)
  {
    return;
  }

  // Derive the real mixer control (and optional -D device) from whatever
  // amixer command the user actually configured for volume keys, instead of
  // assuming a "Master" control exists and is wired to the real output.
  for (const KeyBinding& binding : m_pConfig->keybindings())
  {
    if (binding.command.find("amixer") == std::string::npos)
    {
      continue;
    }

    std::istringstream iss(binding.command);
    std::string token;
    std::string foundDevice;
    std::string foundControl;
    while (iss >> token)
    {
      if (token == "-D" && (iss >> token))
      {
        foundDevice = token;
      }
      else if ((token == "set" || token == "sset") && (iss >> token))
      {
        foundControl = token;
      }
    }

    if (!foundControl.empty())
    {
      device = foundDevice;
      control = foundControl;
      return;
    }
  }
}

void Panel::updateVolume()
{
  std::string device;
  std::string control;
  resolveVolumeControl(device, control);

  std::string primaryCmd = device.empty()
    ? ("amixer sget " + control + " 2>/dev/null")
    : ("amixer -D " + device + " sget " + control + " 2>/dev/null");

  std::string fullCmd = primaryCmd
    + " || amixer -D pulse sget Master 2>/dev/null"
      " || amixer sget Master 2>/dev/null"
      " || amixer sget PCM 2>/dev/null";

  // Prefer parsing amixer output (whatever control the user's keybindings
  // actually use, falling back to pulse / default / PCM)
  FILE* pipe = popen(fullCmd.c_str(), "r");
  if (pipe)
  {
    char line[256];
    bool found = false;
    bool anyChannelOff = false;
    bool anySwitchSeen = false;
    while (fgets(line, sizeof(line), pipe))
    {
      // look for [42%] and [on]/[off]
      char* pct = std::strchr(line, '[');
      if (!pct)
      {
        continue;
      }
      int v = 0;
      if (std::sscanf(pct, "[%d%%]", &v) == 1)
      {
        m_volumePercent = v;
        found = true;
      }
      if (std::strstr(line, "[off]"))
      {
        anyChannelOff = true;
        anySwitchSeen = true;
        found = true;
      }
      else if (std::strstr(line, "[on]"))
      {
        anySwitchSeen = true;
        found = true;
      }
    }
    pclose(pipe);
    if (found)
    {
      if (anySwitchSeen)
      {
        m_volumeMuted = anyChannelOff;
      }
      return;
    }
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


void Panel::runAudioCommand(const char* keyName)
{
  std::string cmd;
  if (m_pConfig)
  {
    for (const KeyBinding& b : m_pConfig->keybindings())
    {
      if (b.display.find(keyName) != std::string::npos)
      {
        cmd = b.command;
        break;
      }
    }
  }
  if (cmd.empty())
  {
    std::string device;
    std::string control;
    resolveVolumeControl(device, control);
    std::string devicePart = device.empty() ? "" : ("-D " + device + " ");

    if (std::strcmp(keyName, "XF86AudioMute") == 0)
    {
      cmd = "amixer " + devicePart + "set " + control + " toggle";
    }
    else if (std::strcmp(keyName, "XF86AudioRaiseVolume") == 0)
    {
      cmd = "amixer " + devicePart + "set " + control + " 5%+";
    }
    else if (std::strcmp(keyName, "XF86AudioLowerVolume") == 0)
    {
      cmd = "amixer " + devicePart + "set " + control + " 5%-";
    }
  }
  if (!cmd.empty())
  {
    std::system((cmd + " >/dev/null 2>&1").c_str());
  }
  // allow mixer to settle
  struct timespec ts = {0, 80 * 1000 * 1000};
  nanosleep(&ts, nullptr);
  updateVolume();
  draw();
}

void Panel::toggleMute()
{
  runAudioCommand("XF86AudioMute");
}

void Panel::draw()
{
  if (m_window == None)
  {
    return;
  }
  Display* d = m_xconn.display();
  int screenW = m_xconn.width();

  if (m_backBuffer == None || m_backBufferW != screenW)
  {
    if (m_backBuffer != None)
    {
      XFreePixmap(d, m_backBuffer);
    }
    m_backBuffer = XCreatePixmap(d, m_window, screenW, MewConst::panelHeight, DefaultDepth(d, m_xconn.screen()));
    m_backBufferW = screenW;
  }

  GC gc = XCreateGC(d, m_backBuffer, 0, nullptr);
  XSetForeground(d, gc, m_bgColor);
  XFillRectangle(d, m_backBuffer, gc, 0, 0, screenW, MewConst::panelHeight);
  time_t now = time(nullptr);
  struct tm* tm = localtime(&now);
  char buf[64];
  char datePart[48];
  strftime(datePart, sizeof(datePart), "%Y-%B-%d", tm);
  char timePart[16];
  strftime(timePart, sizeof(timePart), "%H:%M:%S", tm);
  snprintf(buf, sizeof(buf), "\xee\xaa\xb0 %s \xee\x99\x81 %s", datePart, timePart);
  XftFont* pFont = m_font.font();
  int textH = pFont ? (pFont->ascent + pFont->descent) : 12;
  int baseline = (MewConst::panelHeight + textH) / 2 - (pFont ? pFont->descent : 2);
  m_font.setColor(m_itemColor);

  if (m_hoverZone == 0)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_backBuffer, gc, 0, 0, 40, MewConst::panelHeight);
  }
  else if (m_hoverZone == 1)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_backBuffer, gc, screenW - 480, 0, 28, MewConst::panelHeight);
  }
  else if (m_hoverZone == 2)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_backBuffer, gc, screenW - 450, 0, 70, MewConst::panelHeight);
  }
  else if (m_hoverZone == 3)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_backBuffer, gc, screenW - 380, 0, 30, MewConst::panelHeight);
  }
  else if (m_hoverZone == 4)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_backBuffer, gc, screenW - 350, 0, 35, MewConst::panelHeight);
  }
  else if (m_hoverZone == 5)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_backBuffer, gc, screenW - 25, 0, 30, MewConst::panelHeight);
  }
  else if (m_hoverZone == 6)
  {
    XSetForeground(d, gc, m_hoverColor);
    XFillRectangle(d, m_backBuffer, gc, screenW - 305, 0, 275, MewConst::panelHeight);
  }

  if (!m_startIconRgba.empty())
  {
    const int sz = kStartIconSize;
    unsigned char bgR = static_cast<unsigned char>((m_bgColor >> 16) & 0xff);
    unsigned char bgG = static_cast<unsigned char>((m_bgColor >> 8) & 0xff);
    unsigned char bgB = static_cast<unsigned char>(m_bgColor & 0xff);

    char* xdata = static_cast<char*>(std::malloc(static_cast<size_t>(sz) * sz * 4));
    if (xdata)
    {
      for (int i = 0; i < sz * sz; ++i)
      {
        const unsigned char* s = m_startIconRgba.data() + i * 4;
        unsigned int a = s[3];
        // s[0..2] are premultiplied by alpha.
        unsigned int inv = 255 - a;
        unsigned char outB = static_cast<unsigned char>(s[2] + bgB * inv / 255);
        unsigned char outG = static_cast<unsigned char>(s[1] + bgG * inv / 255);
        unsigned char outR = static_cast<unsigned char>(s[0] + bgR * inv / 255);
        char* dst = xdata + i * 4;
        dst[0] = static_cast<char>(outB);
        dst[1] = static_cast<char>(outG);
        dst[2] = static_cast<char>(outR);
        dst[3] = 0;
      }
      XImage* img = XCreateImage(
        d, DefaultVisual(d, m_xconn.screen()),
        DefaultDepth(d, m_xconn.screen()), ZPixmap, 0,
        xdata, sz, sz, 32, 0);
      if (img)
      {
        const int ix = (MewConst::panelHeight - sz) / 2;  // centered vertically
        const int iy = (MewConst::panelHeight - sz) / 2;
        XPutImage(d, m_backBuffer, gc, img, 0, 0, ix, iy, sz, sz);
        XDestroyImage(img);  // frees xdata
      }
      else
      {
        std::free(xdata);
      }
    }
  }
  else
  {
    // Fallback if the icon failed to load.
    m_font.draw(d, m_xconn.screen(), m_backBuffer, 12, baseline, "\xee\xae\x94");
  }

  refreshLayout();
  updateVolume();
  refreshNetwork();

  bool ifaceUp = !m_selectedIface.empty() && isInterfaceUp(m_selectedIface);
  XSetForeground(d, gc, ifaceUp ? 0x22cc44 : 0xcc2222);
  XFillRectangle(d, m_backBuffer, gc, screenW - 476, 6, 16, 16);
  m_font.draw(d, m_xconn.screen(), m_backBuffer, screenW - 478, baseline, " ");

  std::string netLabel = m_selectedIface.empty() ? "net" : m_selectedIface;
  if (netLabel.size() > 8)
  {
    netLabel = netLabel.substr(0, 8);
  }
  m_font.draw(d, m_xconn.screen(), m_backBuffer, screenW - 448, baseline, netLabel);
  m_font.draw(d, m_xconn.screen(), m_backBuffer, screenW - 375, baseline, m_layoutName);

  char volBuf[48];
  if (m_volumeMuted || m_volumePercent < 0)
  {
    snprintf(volBuf, sizeof(volBuf), "\xf3\xb0\x96\x81");
  }
  else
  {
    snprintf(volBuf, sizeof(volBuf), "%d%%", m_volumePercent);
  }
  m_font.draw(d, m_xconn.screen(), m_backBuffer, screenW - 345, baseline, volBuf);
  m_font.draw(d, m_xconn.screen(), m_backBuffer, screenW - 305, baseline, buf);
  m_font.draw(d, m_xconn.screen(), m_backBuffer, screenW - 20, baseline, "\xef\x92\xa9");

  XCopyArea(d, m_backBuffer, m_window, gc, 0, 0, screenW, MewConst::panelHeight, 0, 0);
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

void Panel::openPowerMenu()
{
  if (!m_startMenuActive)
  {
    showStartMenu();
  }
  showPowerMenu();
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
  if (std::system("/sbin/reboot >/dev/null 2>&1 &") == 0
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
  if (std::system("/sbin/poweroff >/dev/null 2>&1 &") == 0
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
  if (index < 0 || index >= static_cast<int>(m_netIfaces.size()))
  {
    return;
  }
  std::string prev = m_selectedIface;
  m_selectedIface = m_netIfaces[static_cast<size_t>(index)];

  // Selecting a down interface: disconnect previous if it was up
  if (!isInterfaceUp(m_selectedIface) && !prev.empty() && isInterfaceUp(prev))
  {
    std::string cmd = "ip link set dev " + prev + " down";
    if (std::system((cmd + " >/dev/null 2>&1").c_str()) != 0)
    {
      // CAP_NET_ADMIN via pkexec (polkit) then non-interactive sudo
      if (std::system(("pkexec " + cmd + " >/dev/null 2>&1").c_str()) != 0)
      {
        std::system(("sudo -n " + cmd + " >/dev/null 2>&1").c_str());
      }
    }
  }
  draw();
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
    cmd = "ip link set dev " + m_selectedIface + " down";
  }
  else
  {
    cmd = "ip link set dev " + m_selectedIface + " up";
  }
  // try without privileges, then pkexec (polkit), then sudo -n
  if (std::system((cmd + " >/dev/null 2>&1").c_str()) != 0)
  {
    if (std::system(("pkexec " + cmd + " >/dev/null 2>&1").c_str()) != 0)
    {
      std::system(("sudo -n " + cmd + " >/dev/null 2>&1").c_str());
    }
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
  if (x >= screenW - 305 && x < screenW - 30)
  {
    return 6; // clock (tooltip only)
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
  else if (zone == 6)
  {
    showTooltip(x, "Date and time");
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
