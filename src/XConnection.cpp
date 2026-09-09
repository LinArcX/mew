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
  m_atomNetSupported = XInternAtom(m_pDisplay, "_NET_SUPPORTED", False);
  m_atomNetSupportingWmCheck = XInternAtom(m_pDisplay, "_NET_SUPPORTING_WM_CHECK", False);

  setupEwmh();
  return true;
}

void XConnection::setupEwmh()
{
  if (!m_pDisplay)
  {
    return;
  }

  // Child window required by EWMH for _NET_SUPPORTING_WM_CHECK
  m_ewmhWmCheck = XCreateSimpleWindow(m_pDisplay, m_root, 0, 0, 1, 1, 0, 0, 0);

  Atom utf8 = XInternAtom(m_pDisplay, "UTF8_STRING", False);
  const char* wmName = "mew";
  XChangeProperty(
    m_pDisplay, m_ewmhWmCheck, m_atomNetWmName, utf8, 8, PropModeReplace,
    reinterpret_cast<const unsigned char*>(wmName), 3);
  XChangeProperty(
    m_pDisplay, m_ewmhWmCheck, m_atomNetSupportingWmCheck, XA_WINDOW, 32,
    PropModeReplace, reinterpret_cast<unsigned char*>(&m_ewmhWmCheck), 1);
  XChangeProperty(
    m_pDisplay, m_root, m_atomNetSupportingWmCheck, XA_WINDOW, 32,
    PropModeReplace, reinterpret_cast<unsigned char*>(&m_ewmhWmCheck), 1);

  Atom supported[] = {
    m_atomNetSupported,
    m_atomNetSupportingWmCheck,
    m_atomNetWmName,
    m_atomNetWmState,
    m_atomNetWmStateFullscreen,
    m_atomNetWmStateMaxVert,
    m_atomNetWmStateMaxHorz,
  };
  XChangeProperty(
    m_pDisplay, m_root, m_atomNetSupported, XA_ATOM, 32, PropModeReplace,
    reinterpret_cast<unsigned char*>(supported),
    static_cast<int>(sizeof(supported) / sizeof(supported[0])));
}

void XConnection::close()
{
  if (m_pDisplay)
  {
    if (m_ewmhWmCheck != None)
    {
      XDestroyWindow(m_pDisplay, m_ewmhWmCheck);
      m_ewmhWmCheck = None;
    }
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
