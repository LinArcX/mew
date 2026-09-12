#include "PowerWindow.hpp"

#include <X11/keysym.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <unistd.h>

namespace
{
  struct PowerItem
  {
    const char* name;
    int action;
  };

  const PowerItem kItems[] = {
    {"Reconfigure mew", 0},
    {"Reboot",          1},
    {"Poweroff",        2},
    {"Logout",          3}
  };

  constexpr int kItemCount = static_cast<int>(sizeof(kItems) / sizeof(kItems[0]));

  void doReboot()
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

  void doPoweroff()
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
}

PowerWindow::PowerWindow(XConnection& xconn, FontRenderer& font)
  : m_xconn(xconn)
  , m_font(font)
{
}

PowerWindow::~PowerWindow()
{
  hide();
  if (m_window != None && m_xconn.display())
  {
    XDestroyWindow(m_xconn.display(), m_window);
    m_window = None;
  }
}

int PowerWindow::windowHeight() const
{
  return kPad * 2 + kLineH + kMaxVisible * kLineH + 8;
}

void PowerWindow::filter()
{
  m_filtered.clear();
  std::string q = m_query;
  for (char& c : q)
  {
    if (c >= 'A' && c <= 'Z')
    {
      c = static_cast<char>(c + 32);
    }
  }

  for (int i = 0; i < kItemCount; ++i)
  {
    std::string n = kItems[i].name;
    for (char& c : n)
    {
      if (c >= 'A' && c <= 'Z')
      {
        c = static_cast<char>(c + 32);
      }
    }
    if (q.empty() || n.find(q) != std::string::npos)
    {
      m_filtered.push_back(i);
    }
  }

  if (m_index >= m_filtered.size())
  {
    m_index = m_filtered.empty() ? 0 : m_filtered.size() - 1;
  }
}

void PowerWindow::ensureVisible()
{
  if (m_filtered.empty())
  {
    m_scroll = 0;
    m_index = 0;
    return;
  }
  if (m_index >= m_filtered.size())
  {
    m_index = m_filtered.size() - 1;
  }
  if (m_index < m_scroll)
  {
    m_scroll = m_index;
  }
  if (m_index >= m_scroll + static_cast<size_t>(kMaxVisible))
  {
    m_scroll = m_index - static_cast<size_t>(kMaxVisible) + 1;
  }
}

void PowerWindow::hide()
{
  if (m_window != None && m_active)
  {
    XUnmapWindow(m_xconn.display(), m_window);
    XUngrabKeyboard(m_xconn.display(), CurrentTime);
  }
  m_active = false;
  m_query.clear();
  m_index = 0;
  m_scroll = 0;
}

void PowerWindow::draw()
{
  if (m_window == None || !m_active)
  {
    return;
  }

  Display* d = m_xconn.display();
  int height = windowHeight();
  GC gc = XCreateGC(d, m_window, 0, nullptr);

  XSetForeground(d, gc, 0x1e1e1e);
  XFillRectangle(d, m_window, gc, 0, 0, kWidth, height);

  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_window, gc, 0, 0, kWidth - 1, height - 1);

  XSetForeground(d, gc, 0x2a2a2a);
  XFillRectangle(d, m_window, gc, kPad, kPad, kWidth - kPad * 2, kLineH);

  XftFont* pFont = m_font.font();
  std::string prompt = "> " + m_query + "_";
  int baseline = kPad + (kLineH + (pFont ? pFont->ascent : 10)) / 2 - 2;
  m_font.draw(d, m_xconn.screen(), m_window, kPad + 8, baseline, prompt);

  int y0 = kPad + kLineH + 4;
  size_t end = std::min(m_filtered.size(), m_scroll + static_cast<size_t>(kMaxVisible));
  for (size_t i = m_scroll; i < end; ++i)
  {
    int row = static_cast<int>(i - m_scroll);
    int y = y0 + row * kLineH;
    if (i == m_index)
    {
      XSetForeground(d, gc, 0x0a64c8);
      XFillRectangle(d, m_window, gc, 4, y, kWidth - 8, kLineH);
    }
    int itemIdx = m_filtered[i];
    int bl = y + (kLineH + (pFont ? pFont->ascent : 10)) / 2 - 2;
    m_font.draw(d, m_xconn.screen(), m_window, kPad + 8, bl, kItems[itemIdx].name);
  }

  XFreeGC(d, gc);
}

void PowerWindow::show()
{
  m_query.clear();
  m_index = 0;
  m_scroll = 0;
  filter();

  Display* d = m_xconn.display();
  int height = windowHeight();
  int x = (m_xconn.width() - kWidth) / 2;
  int y = (m_xconn.height() - height) / 3;

  if (m_window == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0x1e1e1e;
    attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

    m_window = XCreateWindow(
      d, m_xconn.root(),
      x, y, kWidth, height, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_window, x, y, kWidth, height);
  }

  XMapRaised(d, m_window);
  XClearWindow(d, m_window);
  XGrabKeyboard(d, m_window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  m_active = true;
  draw();
}

void PowerWindow::execute(int itemIndex)
{
  if (itemIndex < 0 || itemIndex >= kItemCount)
  {
    return;
  }

  int action = kItems[itemIndex].action;
  hide();

  if (action == 0)
  {
    if (m_onReconfigure)
    {
      m_onReconfigure();
    }
  }
  else if (action == 1)
  {
    doReboot();
  }
  else if (action == 2)
  {
    doPoweroff();
  }
  else if (action == 3)
  {
    if (m_onQuit)
    {
      m_onQuit();
    }
  }
}

void PowerWindow::handleKey(XKeyEvent* pEvent)
{
  if (!pEvent)
  {
    return;
  }

  KeySym sym = XLookupKeysym(pEvent, 0);
  char buf[8] = {};
  XLookupString(pEvent, buf, sizeof(buf) - 1, &sym, nullptr);

  if (sym == XK_Escape)
  {
    hide();
    return;
  }
  if (sym == XK_Return)
  {
    if (!m_filtered.empty() && m_index < m_filtered.size())
    {
      execute(m_filtered[m_index]);
    }
    return;
  }
  if (sym == XK_Up || sym == XK_KP_Up)
  {
    if (!m_filtered.empty())
    {
      if (m_index > 0)
      {
        --m_index;
      }
      else
      {
        m_index = m_filtered.size() - 1;
      }
      ensureVisible();
    }
    draw();
    return;
  }
  if (sym == XK_Down || sym == XK_KP_Down)
  {
    if (!m_filtered.empty())
    {
      if (m_index + 1 < m_filtered.size())
      {
        ++m_index;
      }
      else
      {
        m_index = 0;
      }
      ensureVisible();
    }
    draw();
    return;
  }
  if (sym == XK_BackSpace)
  {
    if (!m_query.empty())
    {
      m_query.pop_back();
      m_index = 0;
      m_scroll = 0;
      filter();
      ensureVisible();
      draw();
    }
    return;
  }

  if (buf[0] >= 32 && buf[0] < 127)
  {
    m_query.push_back(buf[0]);
    m_index = 0;
    m_scroll = 0;
    filter();
    ensureVisible();
    draw();
  }
}

void PowerWindow::handleClick(XButtonEvent* pEvent)
{
  if (!pEvent)
  {
    return;
  }

  int y0 = kPad + kLineH + 4;
  if (pEvent->y < y0)
  {
    return;
  }

  size_t row = static_cast<size_t>((pEvent->y - y0) / kLineH);
  size_t idx = m_scroll + row;
  if (idx < m_filtered.size() && row < static_cast<size_t>(kMaxVisible))
  {
    m_index = idx;
    execute(m_filtered[m_index]);
  }
}
