#include "XConnection.hpp"

#include <cstdio>
#include <cstdlib>

XConnection::XConnection()
{
}

XConnection::~XConnection()
{
  close();
}

bool XConnection::open()
{
  m_pDisplay = XOpenDisplay(nullptr);
  if (!m_pDisplay)
  {
    fprintf(stderr, "mew: cannot open display\n");
    return false;
  }

  m_screen = DefaultScreen(m_pDisplay);
  m_root = RootWindow(m_pDisplay, m_screen);

  m_atomDeleteWindow = XInternAtom(m_pDisplay, "WM_DELETE_WINDOW", False);
  m_atomProtocols = XInternAtom(m_pDisplay, "WM_PROTOCOLS", False);
  m_atomNetWmName = XInternAtom(m_pDisplay, "_NET_WM_NAME", False);
  m_atomNetWmState = XInternAtom(m_pDisplay, "_NET_WM_STATE", False);
  m_atomNetWmStateFullscreen = XInternAtom(m_pDisplay, "_NET_WM_STATE_FULLSCREEN", False);
  m_atomNetWmStateMaxVert = XInternAtom(m_pDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);
  m_atomNetWmStateMaxHorz = XInternAtom(m_pDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);

  return true;
}

void XConnection::close()
{
  if (m_pDisplay)
  {
    XCloseDisplay(m_pDisplay);
    m_pDisplay = nullptr;
  }
}

Cursor XConnection::loadCursor(const char* themeName, unsigned int fallbackShape)
{
  Cursor c = XcursorLibraryLoadCursor(m_pDisplay, themeName);
  if (c == None)
  {
    c = XCreateFontCursor(m_pDisplay, fallbackShape);
  }
  return c;
}

void XConnection::loadCursors(const char* themeName, int size)
{
  if (themeName && themeName[0] != '\0')
  {
    setenv("XCURSOR_THEME", themeName, 0);
  }
  char sizeBuf[16];
  snprintf(sizeBuf, sizeof(sizeBuf), "%d", size);
  setenv("XCURSOR_SIZE", sizeBuf, 0);

  m_cursorDefault = loadCursor("left_ptr", XC_left_ptr);
  m_cursorResizeH = loadCursor("sb_h_double_arrow", XC_sb_h_double_arrow);
  m_cursorResizeV = loadCursor("sb_v_double_arrow", XC_sb_v_double_arrow);
  m_cursorResizeTl = loadCursor("top_left_corner", XC_top_left_corner);
  m_cursorResizeTr = loadCursor("top_right_corner", XC_top_right_corner);
  m_cursorResizeBl = loadCursor("bottom_left_corner", XC_bottom_left_corner);
  m_cursorResizeBr = loadCursor("bottom_right_corner", XC_bottom_right_corner);

  XDefineCursor(m_pDisplay, m_root, m_cursorDefault);
}

int XConnection::width() const
{
  return DisplayWidth(m_pDisplay, m_screen);
}

int XConnection::height() const
{
  return DisplayHeight(m_pDisplay, m_screen);
}
