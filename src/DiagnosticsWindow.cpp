#include "DiagnosticsWindow.hpp"
#include "Util.hpp"

#include <X11/keysym.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unistd.h>

DiagnosticsWindow::DiagnosticsWindow(XConnection& xconn, FontRenderer& font)
  : m_xconn(xconn)
  , m_font(font)
{
}

DiagnosticsWindow::~DiagnosticsWindow()
{
  hide();
  if (m_window != None && m_xconn.display())
  {
    XDestroyWindow(m_xconn.display(), m_window);
    m_window = None;
  }
}

void DiagnosticsWindow::buildDisplay()
{
  m_lines.clear();
  m_lines.push_back("System Diagnostics");
  m_lines.push_back("");

  std::string pidfile = Util::getPidfile();
  m_lines.push_back("Pidfile: " + pidfile);

  std::ifstream statm("/proc/self/statm");
  if (statm)
  {
    long size = 0;
    long resident = 0;
    statm >> size >> resident;
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize > 0)
    {
      m_lines.push_back("Memory Usage: " + std::to_string((resident * pageSize) / (1024 * 1024)) + " MB");
    }
  }

  m_lines.push_back("");
  m_lines.push_back("Plugins:");
  m_lines.push_back("  All plugins are currently running in-process.");
  m_lines.push_back("  Next step: Migrate to out-of-process architecture.");

  m_lines.push_back("");
  m_lines.push_back("Press ESC to close.");

  m_height = std::min(
    static_cast<int>(MewConst::titleHeight + MewConst::borderWidth + kPadding * 2
                     + m_lines.size() * kLineHeight + 12),
    m_xconn.height() - 80);
}

void DiagnosticsWindow::applyGeometry()
{
  XMoveResizeWindow(m_xconn.display(), m_window, m_x, m_y, m_width, m_height);
  draw();
}

void DiagnosticsWindow::draw()
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
  m_font.draw(d, m_xconn.screen(), m_window, MewConst::borderWidth + 8, titleBaseline, "Diagnostics");

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

void DiagnosticsWindow::moveInteractive()
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

void DiagnosticsWindow::show()
{
  buildDisplay();

  int screenW = m_xconn.width();
  int screenH = m_xconn.height();
  Display* d = m_xconn.display();

  if (m_window == None)
  {
    m_width = kDefaultWidth;
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
    XMoveResizeWindow(d, m_window, m_x, m_y, m_width, m_height);
  }

  XMapRaised(d, m_window);
  XGrabKeyboard(d, m_window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  m_active = true;
  draw();
}

void DiagnosticsWindow::hide()
{
  if (m_window != None && m_active)
  {
    XUnmapWindow(m_xconn.display(), m_window);
  }
  XUngrabKeyboard(m_xconn.display(), CurrentTime);
  m_active = false;
}

void DiagnosticsWindow::raiseIfActive()
{
  if (m_window != None && m_active)
  {
    XRaiseWindow(m_xconn.display(), m_window);
  }
}

void DiagnosticsWindow::handleClick(XButtonEvent* pEvent)
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

void DiagnosticsWindow::handleKey(XKeyEvent* pEvent)
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
