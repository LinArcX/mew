#include "WindowSwitcher.hpp"

#include <X11/keysym.h>
#include <algorithm>
#include <cstdio>

WindowSwitcher::WindowSwitcher(XConnection& xconn, FontRenderer& font, ClientManager& clients)
  : m_xconn(xconn)
  , m_font(font)
  , m_clients(clients)
{
}

WindowSwitcher::~WindowSwitcher()
{
  hide();
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

  auto keyPressed = [&](KeySym sym) -> bool {
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
  for (size_t i = 0; i < m_list.size(); ++i)
  {
    int y = kPad + static_cast<int>(i) * kLineH;
    if (i == m_index)
    {
      XSetForeground(d, gc, MewConst::colorSwitcherHl);
      XFillRectangle(d, m_window, gc, 4, y, kWidth - 8, kLineH);
    }

    std::string title = titleFor(m_list[i]);
    if (title.size() > 48)
    {
      title = title.substr(0, 45) + "...";
    }

    int baseline = y + (kLineH + (pFont ? pFont->ascent : 10)) / 2 - 2;
    m_font.draw(d, m_xconn.screen(), m_window, kPad + 6, baseline, title);
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

void WindowSwitcher::cycle(bool reverse)
{
  m_list.clear();
  for (Client* pClient : m_clients.clients())
  {
    m_list.push_back(pClient);
  }

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
    m_clients.focus(m_list[m_index]);
  }
  hide();
}
