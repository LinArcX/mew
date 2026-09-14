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

  m_atomNetWmMoveResize = XInternAtom(m_pDisplay, "_NET_WM_MOVERESIZE", False);
  m_atomDeleteWindow = XInternAtom(m_pDisplay, "WM_DELETE_WINDOW", False);
  m_atomProtocols = XInternAtom(m_pDisplay, "WM_PROTOCOLS", False);
  m_atomNetWmName = XInternAtom(m_pDisplay, "_NET_WM_NAME", False);
  m_atomNetWmState = XInternAtom(m_pDisplay, "_NET_WM_STATE", False);
  m_atomNetWmStateFullscreen = XInternAtom(m_pDisplay, "_NET_WM_STATE_FULLSCREEN", False);
  m_atomNetWmStateMaxVert = XInternAtom(m_pDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);
  m_atomNetWmStateMaxHorz = XInternAtom(m_pDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
  m_atomNetSupported = XInternAtom(m_pDisplay, "_NET_SUPPORTED", False);
  m_atomNetSupportingWmCheck = XInternAtom(m_pDisplay, "_NET_SUPPORTING_WM_CHECK", False);
  m_atomNetClientList = XInternAtom(m_pDisplay, "_NET_CLIENT_LIST", False);
  m_atomNetClientListStacking = XInternAtom(m_pDisplay, "_NET_CLIENT_LIST_STACKING", False);
  m_atomNetActiveWindow = XInternAtom(m_pDisplay, "_NET_ACTIVE_WINDOW", False);
  m_atomNetNumberOfDesktops = XInternAtom(m_pDisplay, "_NET_NUMBER_OF_DESKTOPS", False);
  m_atomNetCurrentDesktop = XInternAtom(m_pDisplay, "_NET_CURRENT_DESKTOP", False);
  m_atomNetDesktopViewport = XInternAtom(m_pDisplay, "_NET_DESKTOP_VIEWPORT", False);
  m_atomNetDesktopGeometry = XInternAtom(m_pDisplay, "_NET_DESKTOP_GEOMETRY", False);

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
    m_atomNetWmMoveResize,
    m_atomNetSupported,
    m_atomNetSupportingWmCheck,
    m_atomNetWmName,
    m_atomNetWmState,
    m_atomNetWmStateFullscreen,
    m_atomNetWmStateMaxVert,
    m_atomNetWmStateMaxHorz,
    m_atomNetClientList,
    m_atomNetClientListStacking,
    m_atomNetActiveWindow,
    m_atomNetNumberOfDesktops,
    m_atomNetCurrentDesktop,
    m_atomNetDesktopViewport,
    m_atomNetDesktopGeometry,
  };
  XChangeProperty(
    m_pDisplay, m_root, m_atomNetSupported, XA_ATOM, 32, PropModeReplace,
    reinterpret_cast<unsigned char*>(supported),
    static_cast<int>(sizeof(supported) / sizeof(supported[0])));

  // Single desktop (required by many EWMH clients / wmctrl)
  long one = 1;
  long zero = 0;
  XChangeProperty(
    m_pDisplay, m_root, m_atomNetNumberOfDesktops, XA_CARDINAL, 32, PropModeReplace,
    reinterpret_cast<unsigned char*>(&one), 1);
  XChangeProperty(
    m_pDisplay, m_root, m_atomNetCurrentDesktop, XA_CARDINAL, 32, PropModeReplace,
    reinterpret_cast<unsigned char*>(&zero), 1);

  long viewport[2] = {0, 0};
  XChangeProperty(
    m_pDisplay, m_root, m_atomNetDesktopViewport, XA_CARDINAL, 32, PropModeReplace,
    reinterpret_cast<unsigned char*>(viewport), 2);

  long geometry[2] = {DisplayWidth(m_pDisplay, m_screen), DisplayHeight(m_pDisplay, m_screen)};
  XChangeProperty(
    m_pDisplay, m_root, m_atomNetDesktopGeometry, XA_CARDINAL, 32, PropModeReplace,
    reinterpret_cast<unsigned char*>(geometry), 2);

  // Empty client list until first manage()
  XChangeProperty(
    m_pDisplay, m_root, m_atomNetClientList, XA_WINDOW, 32, PropModeReplace,
    nullptr, 0);
  XChangeProperty(
    m_pDisplay, m_root, m_atomNetClientListStacking, XA_WINDOW, 32, PropModeReplace,
    nullptr, 0);

  Window none = None;
  XChangeProperty(
    m_pDisplay, m_root, m_atomNetActiveWindow, XA_WINDOW, 32, PropModeReplace,
    reinterpret_cast<unsigned char*>(&none), 1);
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
