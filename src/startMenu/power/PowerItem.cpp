#include "PowerItem.hpp"
#include "../StartMenuRegistry.hpp"

#include <X11/Xutil.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <ctime>

PowerItem::~PowerItem()
{
  hideSubmenu();
  if (m_submenu != None && m_ctx.pXconn)
  {
    XDestroyWindow(m_ctx.pXconn->display(), m_submenu);
    m_submenu = None;
  }
}

bool PowerItem::onActivate(StartMenuContext& ctx,
                           int parentX, int parentY, int parentW)
{
  m_ctx = ctx;
  if (m_submenu != None)
  {
    hideSubmenu();
    return true;
  }
  openSubmenu(ctx.pXconn->display(), parentX + parentW, parentY);
  return true;
}

void PowerItem::openSubmenu(Display* d, int x, int y)
{
  int h = kRowH * kRows;
  if (m_submenu == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0x222222;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    m_submenu = XCreateWindow(
      d, DefaultRootWindow(d),
      x, y, kWidth, h, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_submenu, x, y, kWidth, h);
  }

  XMapRaised(d, m_submenu);
  drawSubmenu(d, DefaultScreen(d));
}

void PowerItem::drawSubmenu(Display* d, int screen)
{
  if (m_submenu == None || !m_ctx.pFont)
  {
    return;
  }
  GC gc = XCreateGC(d, m_submenu, 0, nullptr);
  int h = kRowH * kRows;
  XSetForeground(d, gc, 0x222222);
  XFillRectangle(d, m_submenu, gc, 0, 0, kWidth, h);
  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_submenu, gc, 0, 0, kWidth - 1, h - 1);

  static const char* items[4] = { "Logout", "Reconfigure mew", "Reboot", "Poweroff" };
  XftFont* pFont = m_ctx.pFont->font();
  for (int i = 0; i < 4; ++i)
  {
    int yy = i * kRowH;
    int bl = yy + (kRowH + (pFont ? pFont->ascent : 10)) / 2 - 2;
    m_ctx.pFont->setColor(0xffffff);
    m_ctx.pFont->draw(d, screen, m_submenu, 12, bl, items[i]);
  }
  XFreeGC(d, gc);
}

void PowerItem::doReboot()
{
  sync();
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

void PowerItem::doPoweroff()
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

bool PowerItem::handleSubmenuClick(int y)
{
  int idx = y / kRowH;
  hideSubmenu();
  switch (idx)
  {
    case 0: if (m_ctx.onQuit) m_ctx.onQuit(); break;
    case 1: if (m_ctx.onReconfigure) m_ctx.onReconfigure(); break;
    case 2: doReboot(); break;
    case 3: doPoweroff(); break;
    default: break;
  }
  return true;
}

void PowerItem::hideSubmenu()
{
  if (m_submenu != None && m_ctx.pXconn)
  {
    XUnmapWindow(m_ctx.pXconn->display(), m_submenu);
  }
}

static StartMenuItem* createPower() { return new PowerItem(); }
static StartMenuItemRegistrar s_power("power", createPower);
