#include "WindowSwitcher.hpp"

#include "stb_image.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>

WindowSwitcher::WindowSwitcher(XConnection& xconn, FontRenderer& font, ClientManager& clients)
  : m_xconn(xconn)
  , m_font(font)
  , m_clients(clients)
{
}

WindowSwitcher::~WindowSwitcher()
{
  hide();
  clearIconCache();
  if (m_window != None && m_xconn.display())
  {
    XDestroyWindow(m_xconn.display(), m_window);
    m_window = None;
  }
}

bool WindowSwitcher::isAltHeld() const
{
  char keys[32];
  XQueryKeymap(m_xconn.display(), keys);

  auto keyPressed = [&](KeySym sym) -> bool
  {
    KeyCode code = XKeysymToKeycode(m_xconn.display(), sym);
    if (code == 0)
    {
      return false;
    }
    return (keys[code / 8] & (1 << (code % 8))) != 0;
  };

  return keyPressed(XK_Alt_L) || keyPressed(XK_Alt_R);
}

std::string WindowSwitcher::titleFor(Client* pClient) const
{
  if (!pClient)
  {
    return "Untitled";
  }

  Display* d = m_xconn.display();
  char* name = nullptr;
  if (XFetchName(d, pClient->window, &name) && name)
  {
    std::string title(name);
    XFree(name);
    return title.empty() ? "Untitled" : title;
  }

  XTextProperty prop;
  if (XGetWMName(d, pClient->window, &prop) && prop.value)
  {
    std::string title(reinterpret_cast<char*>(prop.value));
    XFree(prop.value);
    return title.empty() ? "Untitled" : title;
  }
  return "Untitled";
}

void WindowSwitcher::clearIconCache()
{
  Display* d = m_xconn.display();
  if (d)
  {
    for (auto& kv : m_iconCache)
    {
      if (kv.second != None)
      {
        XFreePixmap(d, kv.second);
      }
    }
  }
  m_iconCache.clear();
}

std::string WindowSwitcher::classForWindow(Window window) const
{
  Display* d = m_xconn.display();
  if (!d || window == None)
  {
    return "";
  }
  XClassHint hint{};
  if (XGetClassHint(d, window, &hint))
  {
    std::string result;
    if (hint.res_class && hint.res_class[0] != '\0')
    {
      result = hint.res_class;
    }
    else if (hint.res_name && hint.res_name[0] != '\0')
    {
      result = hint.res_name;
    }
    if (hint.res_name)
    {
      XFree(hint.res_name);
    }
    if (hint.res_class)
    {
      XFree(hint.res_class);
    }
    return result;
  }
  return "";
}

std::string WindowSwitcher::resolveIconPath(const std::string& icon) const
{
  if (icon.empty())
  {
    return "";
  }
  if (icon[0] == '/')
  {
    std::ifstream test(icon);
    if (test.good())
    {
      return icon;
    }
    return "";
  }
  const char* bases[] = {
    "/usr/share/icons/hicolor/64x64/apps/",
    "/usr/share/icons/hicolor/48x48/apps/",
    "/usr/share/icons/hicolor/32x32/apps/",
    "/usr/share/icons/hicolor/scalable/apps/",
    "/usr/share/icons/Adwaita/64x64/apps/",
    "/usr/share/icons/Adwaita/48x48/apps/",
    "/usr/share/icons/Adwaita/32x32/apps/",
    "/usr/share/icons/Yaru/64x64/apps/",
    "/usr/share/icons/Yaru/48x48/apps/",
    "/usr/share/icons/Yaru/32x32/apps/",
    "/usr/share/pixmaps/",
    "/usr/share/icons/",
    nullptr
  };
  const char* exts[] = {".png", ".svg", ".xpm", "", nullptr};
  for (int i = 0; bases[i]; ++i)
  {
    for (int e = 0; exts[e]; ++e)
    {
      std::string p = std::string(bases[i]) + icon + exts[e];
      std::ifstream test(p);
      if (test.good())
      {
        return p;
      }
    }
  }
  return "";
}

Pixmap WindowSwitcher::loadPixmapFromPath(Display* d, Window drawable, const std::string& path)
{
  if (!d || drawable == None || path.empty())
  {
    return None;
  }
  const int size = kIconSize;
  auto it = m_iconCache.find(0);
  (void)it;

  int iw = 0;
  int ih = 0;
  int ch = 0;
  unsigned char* data = stbi_load(path.c_str(), &iw, &ih, &ch, 4);
  if (!data || iw <= 0 || ih <= 0)
  {
    if (data)
    {
      stbi_image_free(data);
    }
    return None;
  }

  char* xdata = static_cast<char*>(std::malloc(static_cast<size_t>(size * size * 4)));
  if (!xdata)
  {
    stbi_image_free(data);
    return None;
  }
  for (int py = 0; py < size; ++py)
  {
    for (int px = 0; px < size; ++px)
    {
      int x0 = px * iw / size;
      int y0 = py * ih / size;
      int x1 = (px + 1) * iw / size;
      int y1 = (py + 1) * ih / size;
      if (x1 <= x0)
      {
        x1 = x0 + 1;
      }
      if (y1 <= y0)
      {
        y1 = y0 + 1;
      }
      unsigned int r = 0;
      unsigned int g = 0;
      unsigned int b = 0;
      unsigned int n = 0;
      for (int sy = y0; sy < y1; ++sy)
      {
        for (int sx = x0; sx < x1; ++sx)
        {
          unsigned char* s = data + (sy * iw + sx) * 4;
          r += s[0];
          g += s[1];
          b += s[2];
          ++n;
        }
      }
      if (n == 0)
      {
        n = 1;
      }
      char* dst = xdata + (py * size + px) * 4;
      dst[0] = static_cast<char>(b / n);
      dst[1] = static_cast<char>(g / n);
      dst[2] = static_cast<char>(r / n);
      dst[3] = 0;
    }
  }
  stbi_image_free(data);

  Pixmap pm = XCreatePixmap(d, drawable, size, size, DefaultDepth(d, m_xconn.screen()));
  if (pm == None)
  {
    std::free(xdata);
    return None;
  }
  XImage* image = XCreateImage(
    d, DefaultVisual(d, m_xconn.screen()), DefaultDepth(d, m_xconn.screen()),
    ZPixmap, 0, xdata, size, size, 32, 0);
  if (!image)
  {
    std::free(xdata);
    XFreePixmap(d, pm);
    return None;
  }
  GC gc = XCreateGC(d, pm, 0, nullptr);
  XPutImage(d, pm, gc, image, 0, 0, 0, 0, size, size);
  XFreeGC(d, gc);
  XDestroyImage(image);
  return pm;
}

bool WindowSwitcher::tryNetWmIcon(Display* d, Window window, Pixmap& outPixmap)
{
  if (!d || window == None)
  {
    return false;
  }
  outPixmap = None;
  Atom netWmIcon = XInternAtom(d, "_NET_WM_ICON", False);
  if (netWmIcon == None)
  {
    return false;
  }
  Atom actualType = None;
  int actualFormat = 0;
  unsigned long nitems = 0;
  unsigned long bytesAfter = 0;
  unsigned char* prop = nullptr;
  if (XGetWindowProperty(d, window, netWmIcon, 0, 0x7fffffff, False, XA_CARDINAL,
      &actualType, &actualFormat, &nitems, &bytesAfter, &prop) != Success)
  {
    return false;
  }
  if (actualType != XA_CARDINAL || actualFormat != 32 || !prop || nitems < 3)
  {
    if (prop)
    {
      XFree(prop);
    }
    return false;
  }
  unsigned long* ldata = reinterpret_cast<unsigned long*>(prop);
  unsigned long* bestPixels = nullptr;
  int bestW = 0;
  int bestH = 0;
  int bestDiff = 1000000;
  size_t offset = 0;
  while (offset + 2 < nitems)
  {
    unsigned long w = ldata[offset];
    unsigned long h = ldata[offset + 1];
    if (w == 0 || h == 0 || w > 512 || h > 512)
    {
      break;
    }
    if (offset + 2 + w * h > nitems)
    {
      break;
    }
    int diff = std::abs(static_cast<int>(w) - kIconSize) + std::abs(static_cast<int>(h) - kIconSize);
    if (diff < bestDiff)
    {
      bestDiff = diff;
      bestW = static_cast<int>(w);
      bestH = static_cast<int>(h);
      bestPixels = ldata + offset + 2;
    }
    offset += 2 + w * h;
  }
  if (!bestPixels || bestW <= 0 || bestH <= 0)
  {
    XFree(prop);
    return false;
  }
  const int size = kIconSize;
  char* xdata = static_cast<char*>(std::malloc(static_cast<size_t>(size * size * 4)));
  if (!xdata)
  {
    XFree(prop);
    return false;
  }
  for (int py = 0; py < size; ++py)
  {
    for (int px = 0; px < size; ++px)
    {
      int sx = px * bestW / size;
      int sy = py * bestH / size;
      if (sx >= bestW)
      {
        sx = bestW - 1;
      }
      if (sy >= bestH)
      {
        sy = bestH - 1;
      }
      unsigned long argb = bestPixels[sy * bestW + sx];
      unsigned int a = (argb >> 24) & 0xff;
      unsigned int r = (argb >> 16) & 0xff;
      unsigned int g = (argb >> 8) & 0xff;
      unsigned int b = argb & 0xff;
      if (a < 255)
      {
        r = (r * a + 0x1e * (255 - a)) / 255;
        g = (g * a + 0x1e * (255 - a)) / 255;
        b = (b * a + 0x1e * (255 - a)) / 255;
      }
      char* dst = xdata + (py * size + px) * 4;
      dst[0] = static_cast<char>(b);
      dst[1] = static_cast<char>(g);
      dst[2] = static_cast<char>(r);
      dst[3] = 0;
    }
  }
  XFree(prop);
  Pixmap pm = XCreatePixmap(d, m_window != None ? m_window : m_xconn.root(), size, size, DefaultDepth(d, m_xconn.screen()));
  if (pm == None)
  {
    std::free(xdata);
    return false;
  }
  XImage* image = XCreateImage(
    d, DefaultVisual(d, m_xconn.screen()), DefaultDepth(d, m_xconn.screen()),
    ZPixmap, 0, xdata, size, size, 32, 0);
  if (!image)
  {
    std::free(xdata);
    XFreePixmap(d, pm);
    return false;
  }
  GC gc = XCreateGC(d, pm, 0, nullptr);
  XPutImage(d, pm, gc, image, 0, 0, 0, 0, size, size);
  XFreeGC(d, gc);
  XDestroyImage(image);
  outPixmap = pm;
  return true;
}

bool WindowSwitcher::tryDesktopIcon(Display* d, Window window, Pixmap& outPixmap)
{
  if (!d || window == None)
  {
    return false;
  }
  outPixmap = None;
  std::string cls = classForWindow(window);
  if (cls.empty())
  {
    return false;
  }
  std::string path = resolveIconPath(cls);
  if (path.empty())
  {
    std::string lower = cls;
    std::transform(lower.begin(), lower.end(), lower.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    path = resolveIconPath(lower);
  }
  if (path.empty())
  {
    std::string titleCase = cls;
    if (!titleCase.empty())
    {
      titleCase[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(titleCase[0])));
    }
    path = resolveIconPath(titleCase);
  }
  if (path.empty())
  {
    return false;
  }
  Pixmap pm = loadPixmapFromPath(d, m_window != None ? m_window : m_xconn.root(), path);
  if (pm == None)
  {
    return false;
  }
  outPixmap = pm;
  return true;
}

Pixmap WindowSwitcher::getIcon(Client* pClient)
{
  if (!pClient)
  {
    return None;
  }
  Window win = pClient->window;
  auto it = m_iconCache.find(win);
  if (it != m_iconCache.end() && it->second != None)
  {
    return it->second;
  }
  Display* d = m_xconn.display();
  if (!d)
  {
    return None;
  }
  Pixmap pm = None;
  if (tryNetWmIcon(d, win, pm) && pm != None)
  {
    m_iconCache[win] = pm;
    return pm;
  }
  if (tryDesktopIcon(d, win, pm) && pm != None)
  {
    m_iconCache[win] = pm;
    return pm;
  }
  return None;
}

void WindowSwitcher::drawIcon(Display* d, Window win, int x, int y, Pixmap icon)
{
  if (!d || win == None || icon == None)
  {
    return;
  }
  GC gc = XCreateGC(d, win, 0, nullptr);
  XCopyArea(d, icon, win, gc, 0, 0, kIconSize, kIconSize, x, y);
  XFreeGC(d, gc);
}

void WindowSwitcher::hide()
{
  if (m_window != None && m_active)
  {
    XUnmapWindow(m_xconn.display(), m_window);
  }
  if (m_active)
  {
    XUngrabKeyboard(m_xconn.display(), CurrentTime);
  }
  m_active = false;
  m_list.clear();
  clearIconCache();
}

void WindowSwitcher::draw()
{
  if (m_window == None || !m_active || m_list.empty())
  {
    return;
  }

  Display* d = m_xconn.display();
  int height = kPad * 2 + static_cast<int>(m_list.size()) * kLineH;
  GC gc = XCreateGC(d, m_window, 0, nullptr);

  XSetForeground(d, gc, MewConst::colorSwitcherBg);
  XFillRectangle(d, m_window, gc, 0, 0, kWidth, height);

  XSetForeground(d, gc, MewConst::colorSwitcherBorder);
  XDrawRectangle(d, m_window, gc, 0, 0, kWidth - 1, height - 1);

  XftFont* pFont = m_font.font();
  const int iconX = kPad + 6;
  const int iconY0 = kPad;
  const int textX = iconX + kIconSize + 6;
  for (size_t i = 0; i < m_list.size(); ++i)
  {
    int y = kPad + static_cast<int>(i) * kLineH;
    if (i == m_index)
    {
      XSetForeground(d, gc, MewConst::colorSwitcherHl);
      XFillRectangle(d, m_window, gc, 4, y, kWidth - 8, kLineH);
    }

    Pixmap icon = getIcon(m_list[i]);
    int iconY = y + (kLineH - kIconSize) / 2;
    if (icon != None)
    {
      drawIcon(d, m_window, iconX, iconY, icon);
    }
    else
    {
      XSetForeground(d, gc, 0x3a3a3a);
      XFillRectangle(d, m_window, gc, iconX, iconY, kIconSize, kIconSize);
    }

    std::string title = titleFor(m_list[i]);
    if (title.size() > 40)
    {
      title = title.substr(0, 37) + "...";
    }

    int baseline = y + (kLineH + (pFont ? pFont->ascent : 10)) / 2 - 2;
    m_font.draw(d, m_xconn.screen(), m_window, textX, baseline, title);
  }

  XFreeGC(d, gc);
}

void WindowSwitcher::show()
{
  if (m_list.empty())
  {
    return;
  }

  Display* d = m_xconn.display();
  int height = kPad * 2 + static_cast<int>(m_list.size()) * kLineH;
  int x = (m_xconn.width() - kWidth) / 2;
  int y = (m_xconn.height() - height) / 2;

  if (m_window == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = MewConst::colorSwitcherBg;
    attrs.border_pixel = MewConst::colorSwitcherBorder;
    attrs.event_mask = ExposureMask;

    m_window = XCreateWindow(
      d, m_xconn.root(),
      x, y, kWidth, height, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_window, x, y, kWidth, height);
  }

  XMapRaised(d, m_window);
  XGrabKeyboard(d, m_xconn.root(), False, GrabModeAsync, GrabModeAsync, CurrentTime);
  m_active = true;
  draw();
}

void WindowSwitcher::syncMru()
{
  auto& clients = m_clients.clients();

  // Drop entries that are no longer managed
  m_mru.erase(
    std::remove_if(m_mru.begin(), m_mru.end(),
      [&](Client* pClient)
      {
        return std::find(clients.begin(), clients.end(), pClient) == clients.end();
      }),
    m_mru.end());

  // Append newly managed clients at the end (least recently used)
  for (Client* pClient : clients)
  {
    if (std::find(m_mru.begin(), m_mru.end(), pClient) == m_mru.end())
    {
      m_mru.push_back(pClient);
    }
  }

  // Fold in any focus that happened outside the switcher
  Client* pFocused = m_clients.focusedClient();
  if (pFocused)
  {
    auto it = std::find(m_mru.begin(), m_mru.end(), pFocused);
    if (it != m_mru.end() && it != m_mru.begin())
    {
      m_mru.erase(it);
      m_mru.insert(m_mru.begin(), pFocused);
    }
  }
}

void WindowSwitcher::cycle(bool reverse)
{
  syncMru();
  m_list = m_mru;

  if (m_list.empty())
  {
    hide();
    return;
  }

  if (!m_active)
  {
    Client* pCurrent = m_clients.focusedClient();
    size_t start = 0;
    if (pCurrent)
    {
      auto it = std::find(m_list.begin(), m_list.end(), pCurrent);
      if (it != m_list.end())
      {
        start = static_cast<size_t>((it - m_list.begin() + 1) % static_cast<long>(m_list.size()));
      }
    }
    m_index = start;
    show();
  }
  else
  {
    if (reverse)
    {
      m_index = (m_index == 0) ? m_list.size() - 1 : m_index - 1;
    }
    else
    {
      m_index = (m_index + 1) % m_list.size();
    }
    draw();
  }
}

void WindowSwitcher::commit()
{
  if (!m_list.empty() && m_index < m_list.size())
  {
    Client* pSelected = m_list[m_index];
    m_clients.focus(pSelected);

    auto it = std::find(m_mru.begin(), m_mru.end(), pSelected);
    if (it != m_mru.end())
    {
      m_mru.erase(it);
    }
    m_mru.insert(m_mru.begin(), pSelected);
  }
  hide();
}
