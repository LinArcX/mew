#include "KeybindingsWindow.hpp"
#include <X11/keysym.h>
#include "Util.hpp"

#include <algorithm>
#include <sstream>

KeybindingsWindow::KeybindingsWindow(XConnection& xconn, FontRenderer& font, Config& config)
  : m_xconn(xconn)
  , m_font(font)
  , m_config(config)
{
}

KeybindingsWindow::~KeybindingsWindow()
{
  hide();
  if (m_window != None && m_xconn.display())
  {
    XDestroyWindow(m_xconn.display(), m_window);
    m_window = None;
  }
}

void KeybindingsWindow::buildDisplay()
{
  m_lines.clear();
  m_lines.push_back(Util::getConfigDirectory() + "/keybindings");
  m_lines.push_back("");

  int i = 1;
  for (const KeyBinding& b : m_config.keybindings())
  {
    std::ostringstream oss;
    oss << i << ". " << b.display << " --> " << b.command;
    m_lines.push_back(oss.str());
    ++i;
  }

  if (m_config.keybindings().empty())
  {
    m_lines.push_back("(no keybindings configured)");
  }
}

void KeybindingsWindow::applyGeometry()
{
  XMoveResizeWindow(m_xconn.display(), m_window, m_x, m_y, m_width, m_height);
  draw();
}

void KeybindingsWindow::draw()
{
  if (m_window == None || !m_active)
  {
    return;
  }

  Display* d = m_xconn.display();
  GC gc = XCreateGC(d, m_window, 0, nullptr);

  XSetForeground(d, gc, MewConst::colorBorder);
  XFillRectangle(d, m_window, gc, 0, 0, m_width, m_height);

  XSetForeground(d, gc, MewConst::colorTitle);
  XFillRectangle(
    d, m_window, gc,
    MewConst::borderWidth, MewConst::borderWidth,
    m_width - MewConst::borderWidth * 2,
    MewConst::titleHeight - MewConst::borderWidth);

  int closeX = m_width - MewConst::buttonWidth;
  XSetForeground(d, gc, MewConst::colorButton);
  XFillRectangle(
    d, m_window, gc, closeX, MewConst::borderWidth,
    MewConst::buttonWidth, MewConst::titleHeight - MewConst::borderWidth);

  XSetForeground(d, gc, MewConst::colorText);
  XDrawLine(d, m_window, gc, closeX + 9, 8, closeX + MewConst::buttonWidth - 9, MewConst::titleHeight - 9);
  XDrawLine(d, m_window, gc, closeX + MewConst::buttonWidth - 9, 8, closeX + 9, MewConst::titleHeight - 9);

  XftFont* pFont = m_font.font();
  int textHeight = pFont ? (pFont->ascent + pFont->descent) : 10;
  int titleBaseline = MewConst::borderWidth
    + (MewConst::titleHeight - MewConst::borderWidth - textHeight) / 2
    + (pFont ? pFont->ascent : 10);
  m_font.draw(d, m_xconn.screen(), m_window, MewConst::borderWidth + 8, titleBaseline, "Keybindings");

  XSetForeground(d, gc, MewConst::colorSwitcherBg);
  XFillRectangle(
    d, m_window, gc,
    MewConst::borderWidth, MewConst::titleHeight,
    m_width - MewConst::borderWidth * 2,
    m_height - MewConst::titleHeight - MewConst::borderWidth);

  int y = MewConst::titleHeight + kPadding + (pFont ? pFont->ascent : 12);
  for (const std::string& line : m_lines)
  {
    if (!line.empty())
    {
      m_font.draw(d, m_xconn.screen(), m_window, MewConst::borderWidth + kPadding, y, line);
    }
    y += kLineHeight;
    if (y > m_height - MewConst::borderWidth - 4)
    {
      break;
    }
  }

  XFreeGC(d, gc);
}

void KeybindingsWindow::moveInteractive()
{
  Display* d = m_xconn.display();
  Window dummy = None;
  int rootX = 0;
  int rootY = 0;
  int winX = 0;
  int winY = 0;
  unsigned int mask = 0;
  XQueryPointer(d, m_xconn.root(), &dummy, &dummy, &rootX, &rootY, &winX, &winY, &mask);

  int startX = m_x;
  int startY = m_y;

  XGrabPointer(
    d, m_window, False,
    ButtonMotionMask | ButtonReleaseMask,
    GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

  XEvent event;
  while (true)
  {
    XMaskEvent(d, ButtonMotionMask | ButtonReleaseMask, &event);
    if (event.type == MotionNotify)
    {
      m_x = startX + (event.xmotion.x_root - rootX);
      m_y = startY + (event.xmotion.y_root - rootY);
      applyGeometry();
    }
    if (event.type == ButtonRelease)
    {
      break;
    }
  }
  XUngrabPointer(d, CurrentTime);
}

void KeybindingsWindow::show()
{
  buildDisplay();

  int screenW = m_xconn.width();
  int screenH = m_xconn.height();
  Display* d = m_xconn.display();

  if (m_window == None)
  {
    m_width = kDefaultWidth;
    m_height = std::min(
      static_cast<int>(MewConst::titleHeight + MewConst::borderWidth + kPadding * 2
                       + m_lines.size() * kLineHeight + 12),
      screenH - 80);
    m_x = (screenW - m_width) / 2;
    m_y = (screenH - m_height) / 2;

    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = MewConst::colorBorder;
    attrs.border_pixel = MewConst::colorSwitcherBorder;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    m_window = XCreateWindow(
      d, m_xconn.root(),
      m_x, m_y, m_width, m_height, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
      &attrs);
  }
  else
  {
    m_height = std::min(
      static_cast<int>(MewConst::titleHeight + MewConst::borderWidth + kPadding * 2
                       + m_lines.size() * kLineHeight + 12),
      screenH - 80);
    XMoveResizeWindow(d, m_window, m_x, m_y, m_width, m_height);
  }

  XMapRaised(d, m_window);
  XGrabKeyboard(d, m_window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  m_active = true;
  draw();
}

void KeybindingsWindow::hide()
{
  if (m_window != None && m_active)
  {
    XUnmapWindow(m_xconn.display(), m_window);
  }
  XUngrabKeyboard(m_xconn.display(), CurrentTime);
  m_active = false;
}

void KeybindingsWindow::raiseIfActive()
{
  if (m_window != None && m_active)
  {
    XRaiseWindow(m_xconn.display(), m_window);
  }
}

void KeybindingsWindow::handleClick(XButtonEvent* pEvent)
{
  if (!pEvent)
  {
    return;
  }
  if (pEvent->y < MewConst::borderWidth || pEvent->y >= MewConst::titleHeight)
  {
    return;
  }

  int closeX = m_width - MewConst::buttonWidth;
  if (pEvent->x >= closeX)
  {
    hide();
    return;
  }

  moveInteractive();
}


void KeybindingsWindow::handleKey(XKeyEvent* pEvent)
{
  if (!pEvent || !m_active)
  {
    return;
  }
  KeySym sym = XLookupKeysym(pEvent, 0);
  if (sym == XK_Escape)
  {
    hide();
  }
}
