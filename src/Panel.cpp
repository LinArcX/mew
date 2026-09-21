#include "Panel.hpp"
#include "logo_data.h"

#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "stb_image.h"
#include <X11/Xutil.h>

#include <sys/reboot.h>
#include <linux/reboot.h>

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
  for (StartMenuItem* pItem : m_startItems)
  {
    delete pItem;
  }
  m_startItems.clear();

  m_pHoverPlugin = nullptr;

  for (PanelPlugin* pPlugin : m_plugins)
  {
    delete pPlugin;
  }
  m_plugins.clear();
  if (m_startMenu != None)
  {
    XDestroyWindow(d, m_startMenu);
    m_startMenu = None;
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
  unsigned char* data = stbi_load_from_memory(logo_png,
    static_cast<int>(logo_png_len),
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
  m_plugins.clear();

  std::vector<Config::PanelPluginEntry> entries;
  if (m_pConfig)
  {
    entries = m_pConfig->panelPlugins();
  }

  for (const Config::PanelPluginEntry& e : entries)
  {
    PanelPlugin* pPlugin = PanelPluginRegistry::instance().createOne(e.id, m_xconn, m_font);
    if (!pPlugin)
    {
      continue;
    }

    PanelPosition pos = pPlugin->anchorRight() ? PanelPosition::Right : PanelPosition::Left;
    if      (e.position == "left")   pos = PanelPosition::Left;
    else if (e.position == "center") pos = PanelPosition::Center;
    else if (e.position == "right")  pos = PanelPosition::Right;

    pPlugin->setPosition(pos);
    m_plugins.push_back(pPlugin);
  }

  std::vector<std::string> menuIds =
    m_pConfig ? m_pConfig->startMenuItems() : std::vector<std::string>{"apps","keybindings","power"};
  m_startItems = StartMenuRegistry::instance().create(menuIds);

  for (PanelPlugin* pPlugin : m_plugins)
  {
    if (m_pConfig)
    {
      pPlugin->configure(*m_pConfig);
    }
  }
  draw();
}

StartMenuContext Panel::makeStartMenuContext()
{
  StartMenuContext ctx;
  ctx.pXconn = &m_xconn;
  ctx.pFont = &m_font;
  ctx.pConfig = m_pConfig;
  ctx.onShowLauncher = m_onShowLauncher;
  ctx.onShowKeybindings = m_onShowKeybindings;
  ctx.onQuit = m_onQuit;
  ctx.onReconfigure = m_onReconfigure;
  return ctx;
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

   // Pre-compute group widths for center and right.
  int centerTotal = 0;
  int rightTotal = 0;
  for (PanelPlugin* pPlugin : m_plugins)
  {
    int w = pPlugin->width();
    if (pPlugin->position() == PanelPosition::Center) centerTotal += w;
    else if (pPlugin->position() == PanelPosition::Right) rightTotal += w;
  }

  int leftCursor   = 44;
  int centerCursor = (screenW - centerTotal) / 2;
  int rightCursor  = screenW - rightTotal;

  for (PanelPlugin* pPlugin : m_plugins)
  {
    int w = pPlugin->width();
    int x = 0;
    switch (pPlugin->position())
    {
      case PanelPosition::Left:   x = leftCursor;   leftCursor += w;   break;
      case PanelPosition::Center: x = centerCursor; centerCursor += w; break;
      case PanelPosition::Right:  x = rightCursor;  rightCursor += w;  break;
    }

    if (pPlugin == m_pHoverPlugin)
    {
      XSetForeground(d, gc, m_hoverColor);
      XFillRectangle(d, m_backBuffer, gc, x, 0, w, MewConst::panelHeight);
    }

    pPlugin->draw(d, m_backBuffer, x, baseline);
  }

  XCopyArea(d, m_backBuffer, m_window, gc, 0, 0, screenW, MewConst::panelHeight, 0, 0);
  XFreeGC(d, gc);
}

void Panel::hideMenus()
{
  if (m_startMenu != None && m_startMenuActive)
    XUnmapWindow(m_xconn.display(), m_startMenu);

  if (m_startMenuActive)
    XUngrabKeyboard(m_xconn.display(), CurrentTime);

  m_startMenuActive = false;

}

void Panel::drawStartMenu()
{
  if (m_startMenu == None || !m_startMenuActive)
  {
    return;
  }

  Display* d = m_xconn.display();
  int totalH = 0;
  for (StartMenuItem* pItem : m_startItems)
  {
    totalH += pItem->height();
  }
  if (totalH <= 0)
  {
    return;
  }

  GC gc = XCreateGC(d, m_startMenu, 0, nullptr);
  XSetForeground(d, gc, m_bgColor);
  XFillRectangle(d, m_startMenu, gc, 0, 0, kStartMenuW, totalH);
  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_startMenu, gc, 0, 0, kStartMenuW - 1, totalH - 1);

  XftFont* pFont = m_font.font();
  int y = 0;
  for (StartMenuItem* pItem : m_startItems)
  {
    int h = pItem->height();
    int bl = y + (h + (pFont ? pFont->ascent : 10)) / 2 - 2;
    m_font.setColor(m_itemColor);
    m_font.draw(d, m_xconn.screen(), m_startMenu, 12, bl, pItem->label());
    y += h;
  }

  XFreeGC(d, gc);
}

void Panel::showStartMenu()
{
  hideItemSubmenus();

  Display* d = m_xconn.display();
  int totalH = 0;
  for (StartMenuItem* pItem : m_startItems)
  {
    totalH += pItem->height();
  }
  if (totalH <= 0)
  {
    return;
  }

  int x = 0;
  int y = m_xconn.height() - MewConst::panelHeight - totalH;
  m_startMenuY = y;

  if (m_startMenu == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = m_bgColor;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    m_startMenu = XCreateWindow(
      d, m_xconn.root(),
      x, y, kStartMenuW, totalH, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_startMenu, x, y, kStartMenuW, totalH);
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

void Panel::handleClick(int x)
{

   int centerTotal = 0;
  int rightTotal = 0;
  for (PanelPlugin* pPlugin : m_plugins)
  {
    int w = pPlugin->width();
    if (pPlugin->position() == PanelPosition::Center) centerTotal += w;
    else if (pPlugin->position() == PanelPosition::Right) rightTotal += w;
  }

  int leftCursor   = 44;
  int centerCursor = (m_xconn.width() - centerTotal) / 2;
  int rightCursor  = m_xconn.width() - rightTotal;

  for (PanelPlugin* pPlugin : m_plugins)
  {
    int w = pPlugin->width();
    int wx = 0;
    switch (pPlugin->position())
    {
      case PanelPosition::Left:   wx = leftCursor;   leftCursor += w;   break;
      case PanelPosition::Center: wx = centerCursor; centerCursor += w; break;
      case PanelPosition::Right:  wx = rightCursor;  rightCursor += w;  break;
    }

    if (x >= wx && x < wx + w)
    {
      if (!pPlugin->handleLocalClick(x - wx, wx))
      {
        pPlugin->onClick(wx);
      }
      draw();
      return;
    }
  }
  // Start button.
  if (x < 40)
  {
    if (m_startMenuActive) hideMenus();
    else                   showStartMenu();
    return;
  }

  // Anywhere else on the panel closes menus.
  if (m_startMenuActive) hideMenus();
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

void Panel::tick()
{
  bool needRedraw = false;
  for (PanelPlugin* pPlugin : m_plugins)
  {
    if (pPlugin->tick())
      needRedraw = true;
  }
  if (needRedraw)
    draw();

}

void Panel::handleMotion(int x)
{

    int centerTotal = 0;
  int rightTotal = 0;
  for (PanelPlugin* pPlugin : m_plugins)
  {
    int w = pPlugin->width();
    if (pPlugin->position() == PanelPosition::Center) centerTotal += w;
    else if (pPlugin->position() == PanelPosition::Right) rightTotal += w;
  }

  int leftCursor   = 44;
  int centerCursor = (m_xconn.width() - centerTotal) / 2;
  int rightCursor  = m_xconn.width() - rightTotal;

  PanelPlugin* pOver = nullptr;
  int hoverX = 0;

  for (PanelPlugin* pPlugin : m_plugins)
  {
    int w = pPlugin->width();
    int wx = 0;
    switch (pPlugin->position())
    {
      case PanelPosition::Left:   wx = leftCursor;   leftCursor += w;   break;
      case PanelPosition::Center: wx = centerCursor; centerCursor += w; break;
      case PanelPosition::Right:  wx = rightCursor;  rightCursor += w;  break;
    }

    if (x >= wx && x < wx + w)
    {
      pOver = pPlugin;
      hoverX = wx;
      break;
    }
  }
  if (pOver != m_pHoverPlugin)
  {
    if (m_pHoverPlugin) m_pHoverPlugin->onUnhover();
    m_pHoverPlugin = pOver;
    if (pOver) pOver->onHover(hoverX);
    draw();
  }

  if (pOver)
  {
    std::string tip = pOver->tooltip();
    if (!tip.empty()) showTooltip(x, tip.c_str());
    else              hideTooltip();
    return;
  }

  // Start button only (zones for old built-ins are gone).
  if (x < 40)
  {
    if (m_hoverZone != 0) { m_hoverZone = 0; draw(); }
    showTooltip(x, "Start menu");
  }
  else
  {
    if (m_hoverZone != -1) { m_hoverZone = -1; draw(); }
    hideTooltip();
  }
}

bool Panel::handleEscape()
{
  for (PanelPlugin* pPlugin : m_plugins)
  {
    if (pPlugin->handleEscape())
      return true;
  }
  return false;

}
void Panel::handleLeave()
{
  if (m_pHoverPlugin) { m_pHoverPlugin->onUnhover(); m_pHoverPlugin = nullptr; }
  m_hoverZone = -1;
  hideTooltip();
  draw();

}

void Panel::handleStartMenuClick(int y)
{
  int acc = 0;
  for (StartMenuItem* pItem : m_startItems)
  {
    int h = pItem->height();
    if (y >= acc && y < acc + h)
    {
      StartMenuContext ctx = makeStartMenuContext();
      bool stayOpen = pItem->onActivate(ctx, 0, m_startMenuY, kStartMenuW);
      if (!stayOpen)
      {
        hideMenus();
      }
      return;
    }
    acc += h;
  }
}

bool Panel::isItemSubmenuWindow(Window w) const
{
  for (StartMenuItem* pItem : m_startItems)
  {
    if (pItem->submenuWindow() == w && w != None)
    {
      return true;
    }
  }
  return false;
}

bool Panel::handleItemSubmenuClick(Window w, int y)
{
  for (StartMenuItem* pItem : m_startItems)
  {
    if (pItem->submenuWindow() == w)
    {
      return pItem->handleSubmenuClick(y);
    }
  }
  return false;
}

void Panel::drawItemSubmenu(Window w)
{
  for (StartMenuItem* pItem : m_startItems)
  {
    if (pItem->submenuWindow() == w)
    {
      pItem->drawSubmenu(m_xconn.display(), m_xconn.screen());
      return;
    }
  }
}

void Panel::hideItemSubmenus()
{
  for (StartMenuItem* pItem : m_startItems)
  {
    pItem->hideSubmenu();
  }
}

