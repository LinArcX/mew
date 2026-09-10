#include "ClientManager.hpp"
#include <X11/Xutil.h>

#include <X11/Xatom.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

ClientManager::ClientManager(XConnection& xconn, FontRenderer& font)
  : m_xconn(xconn)
  , m_font(font)
{
}

int ClientManager::usableHeight() const
{
  return m_xconn.height() - m_panelHeight;
}

std::string ClientManager::windowTitle(Window window)
{
  Display* d = m_xconn.display();
  char* name = nullptr;
  if (XFetchName(d, window, &name) && name)
  {
    std::string title(name);
    XFree(name);
    return title.empty() ? "Untitled" : title;
  }

  XTextProperty prop;
  if (XGetWMName(d, window, &prop) && prop.value)
  {
    std::string title(reinterpret_cast<char*>(prop.value));
    XFree(prop.value);
    return title.empty() ? "Untitled" : title;
  }
  return "Untitled";
}

Client* ClientManager::findClient(Window window)
{
  for (Client* pClient : m_clients)
  {
    if (pClient->window == window || pClient->frame == window)
    {
      return pClient;
    }
  }
  return nullptr;
}

Client* ClientManager::focusedClient()
{
  Display* d = m_xconn.display();
  Window focused = None;
  int revert = 0;
  XGetInputFocus(d, &focused, &revert);
  if (focused == None || focused == PointerRoot)
  {
    return nullptr;
  }

  Client* pClient = findClient(focused);
  if (pClient)
  {
    return pClient;
  }

  // GTK/CSD apps set focus on a child widget, not the top-level window.
  // Walk up the parent chain until we hit a managed window or frame.
  Window current = focused;
  for (int depth = 0; depth < 16; ++depth)
  {
    Window rootReturn = None;
    Window parentReturn = None;
    Window* children = nullptr;
    unsigned int nChildren = 0;
    if (!XQueryTree(d, current, &rootReturn, &parentReturn, &children, &nChildren))
    {
      return nullptr;
    }
    if (children)
    {
      XFree(children);
    }
    if (parentReturn == None || parentReturn == rootReturn)
    {
      return nullptr;
    }
    pClient = findClient(parentReturn);
    if (pClient)
    {
      return pClient;
    }
    current = parentReturn;
  }
  return nullptr;
  //Window focused = None;
  //int revert = 0;
  //XGetInputFocus(m_xconn.display(), &focused, &revert);
  //return findClient(focused);
}

void ClientManager::drawFrame(Client* pClient)
{
  if (!pClient || pClient->fullscreen || pClient->csd)
  {
    return;
  }

  Display* d = m_xconn.display();
  GC gc = XCreateGC(d, pClient->frame, 0, nullptr);

  XSetForeground(d, gc, MewConst::colorBorder);
  XFillRectangle(
    d, pClient->frame, gc, 0, 0,
    pClient->width + MewConst::borderWidth * 2,
    pClient->height + MewConst::titleHeight + MewConst::borderWidth);

  XSetForeground(d, gc, MewConst::colorTitle);
  XFillRectangle(
    d, pClient->frame, gc,
    MewConst::borderWidth, MewConst::borderWidth,
    pClient->width, MewConst::titleHeight - MewConst::borderWidth);

  int closeX = MewConst::borderWidth + pClient->width - MewConst::buttonWidth;
  int maxX = closeX - MewConst::buttonWidth;
  int minX = maxX - MewConst::buttonWidth;
  int buttonY = MewConst::borderWidth;

  XSetForeground(d, gc, MewConst::colorButton);
  XFillRectangle(d, pClient->frame, gc, minX, buttonY, MewConst::buttonWidth, MewConst::titleHeight - MewConst::borderWidth);
  XFillRectangle(d, pClient->frame, gc, maxX, buttonY, MewConst::buttonWidth, MewConst::titleHeight - MewConst::borderWidth);
  XFillRectangle(d, pClient->frame, gc, closeX, buttonY, MewConst::buttonWidth, MewConst::titleHeight - MewConst::borderWidth);

  XSetForeground(d, gc, MewConst::colorText);
  XDrawLine(d, pClient->frame, gc, minX + 9, MewConst::titleHeight / 2 + 4, minX + MewConst::buttonWidth - 9, MewConst::titleHeight / 2 + 4);

  if (pClient->maximized)
  {
    XDrawRectangle(d, pClient->frame, gc, maxX + 9, 8, 10, 9);
    XDrawRectangle(d, pClient->frame, gc, maxX + 12, 11, 10, 9);
  }
  else
  {
    XDrawRectangle(d, pClient->frame, gc, maxX + 9, 8, 11, 10);
  }

  XDrawLine(d, pClient->frame, gc, closeX + 9, 8, closeX + MewConst::buttonWidth - 9, MewConst::titleHeight - 9);
  XDrawLine(d, pClient->frame, gc, closeX + MewConst::buttonWidth - 9, 8, closeX + 9, MewConst::titleHeight - 9);

  std::string title = windowTitle(pClient->window);
  XftFont* pFont = m_font.font();
  int textHeight = pFont ? (pFont->ascent + pFont->descent) : 10;
  int baseline = MewConst::borderWidth
    + (MewConst::titleHeight - MewConst::borderWidth - textHeight) / 2
    + (pFont ? pFont->ascent : 10);
  m_font.draw(d, m_xconn.screen(), pClient->frame, MewConst::borderWidth + 8, baseline, title);

  XFreeGC(d, gc);
}

void ClientManager::resize(Client* pClient)
{
  if (!pClient || pClient->fullscreen)
  {
    return;
  }

  int bx = pClient->csd ? 0 : MewConst::borderWidth;
  int by = pClient->csd ? 0 : MewConst::titleHeight;
  Display* d = m_xconn.display();
  XMoveResizeWindow(d, pClient->window,  bx, by, pClient->width, pClient->height);

  //MewConst::borderWidth, MewConst::titleHeight, pClient->width, pClient->height);
  XMoveResizeWindow(
    d, pClient->frame, pClient->x, pClient->y,
    pClient->width + bx * 2,
    pClient->height + by + bx);
  if (!pClient->csd)
  {
    drawFrame(pClient);
  }
  //drawFrame(pClient);
}

void ClientManager::focus(Client* pClient)
{
  if (!pClient)
  {
    return;
  }

  Display* d = m_xconn.display();

  if (pClient->minimized)
  {
    pClient->minimized = false;
    XMapWindow(d, pClient->frame);
  }

  if (pClient->fullscreen)
  {
    // Frame is unmapped; raise and focus the client window on root.
    XRaiseWindow(d, pClient->window);
    XSetInputFocus(d, pClient->window, RevertToPointerRoot, CurrentTime);
    if (m_raiseOverlay)
    {
      m_raiseOverlay();
    }
    return;
  }

  XRaiseWindow(d, pClient->frame);
  if (m_raiseOverlay)
  {
    m_raiseOverlay();
  }

  // Prefer explicit focus; also send WM_TAKE_FOCUS for clients that need it (e.g. neovim)
  XSetInputFocus(d, pClient->window, RevertToPointerRoot, CurrentTime);

  Atom* protocols = nullptr;
  int count = 0;
  if (XGetWMProtocols(d, pClient->window, &protocols, &count))
  {
    Atom takeFocus = XInternAtom(d, "WM_TAKE_FOCUS", False);
    for (int i = 0; i < count; ++i)
    {
      if (protocols[i] == takeFocus)
      {
        XEvent ev{};
        ev.xclient.type = ClientMessage;
        ev.xclient.window = pClient->window;
        ev.xclient.message_type = m_xconn.atomProtocols();
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = static_cast<long>(takeFocus);
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(d, pClient->window, False, NoEventMask, &ev);
        break;
      }
    }
    if (protocols)
    {
      XFree(protocols);
    }
  }

  drawFrame(pClient);
}

void ClientManager::focusNext()
{
  if (m_clients.empty())
  {
    return;
  }

  Client* pCurrent = focusedClient();
  size_t start = 0;
  if (pCurrent)
  {
    auto it = std::find(m_clients.begin(), m_clients.end(), pCurrent);
    if (it != m_clients.end())
    {
      start = static_cast<size_t>((it - m_clients.begin() + 1) % static_cast<long>(m_clients.size()));
    }
  }

  for (size_t i = 0; i < m_clients.size(); ++i)
  {
    size_t index = (start + i) % m_clients.size();
    Client* pClient = m_clients[index];
    if (!pClient->minimized)
    {
      focus(pClient);
      return;
    }
  }
}

void ClientManager::close(Client* pClient)
{
  if (!pClient)
  {
    return;
  }

  Display* d = m_xconn.display();
  Window window = pClient->window;
  Atom* protocols = nullptr;
  int count = 0;
  bool supportsDelete = false;

  if (XGetWMProtocols(d, window, &protocols, &count))
  {
    for (int i = 0; i < count; ++i)
    {
      if (protocols[i] == m_xconn.atomDeleteWindow())
      {
        supportsDelete = true;
        break;
      }
    }
    if (protocols)
    {
      XFree(protocols);
    }
  }

  if (supportsDelete)
  {
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = m_xconn.atomProtocols();
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(m_xconn.atomDeleteWindow());
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(d, window, False, NoEventMask, &event);
    XFlush(d);
  }
  else
  {
    XKillClient(d, window);
  }
}

void ClientManager::minimize(Client* pClient)
{
  if (!pClient)
  {
    return;
  }
  if (pClient->fullscreen)
  {
    setFullscreen(pClient, false);
  }
  pClient->minimized = true;
  XUnmapWindow(m_xconn.display(), pClient->frame);
  focusNext();
}

void ClientManager::maximize(Client* pClient)
{
  if (!pClient || pClient->noMaximize)
  {
    return;
  }

  // Fullscreen owns the geometry; leave it first so decorations return.
  if (pClient->fullscreen)
  {
    setFullscreen(pClient, false);
    return;
  }

  if (!pClient->maximized)
  {
    int bx = pClient->csd ? 0 : MewConst::borderWidth;
    int by = pClient->csd ? 0 : MewConst::titleHeight;
    pClient->oldX = pClient->x;
    pClient->oldY = pClient->y;
    pClient->oldWidth = pClient->width;
    pClient->oldHeight = pClient->height;
    pClient->x = 0;
    pClient->y = 0;
    pClient->width = m_xconn.width() - bx * 2;
    pClient->height = usableHeight() - by - bx;
    pClient->maximized = true;

    //pClient->oldX = pClient->x;
    //pClient->oldY = pClient->y;
    //pClient->oldWidth = pClient->width;
    //pClient->oldHeight = pClient->height;
    //pClient->x = 0;
    //pClient->y = 0;
    //pClient->width = m_xconn.width() - MewConst::borderWidth * 2;
    //pClient->height = usableHeight() - MewConst::titleHeight - MewConst::borderWidth;
    //pClient->maximized = true;
  }
  else
  {
    pClient->x = pClient->oldX;
    pClient->y = pClient->oldY;
    pClient->width = pClient->oldWidth;
    pClient->height = pClient->oldHeight;
    pClient->maximized = false;
  }

  resize(pClient);
  focus(pClient);
}

void ClientManager::setFullscreen(Client* pClient, bool enable)
{
  if (!pClient)
  {
    return;
  }

  Display* d = m_xconn.display();
  if (enable && !pClient->fullscreen)
  {
    if (!pClient->maximized)
    {
      pClient->oldX = pClient->x;
      pClient->oldY = pClient->y;
      pClient->oldWidth = pClient->width;
      pClient->oldHeight = pClient->height;
    }
    pClient->fullscreen = true;
    pClient->maximized = true;

    // Reparent to root generates UnmapNotify; ignore expected ones.
    pClient->ignoreUnmap += 2;

    // True fullscreen: client covers entire screen; frame and panel hidden
    XReparentWindow(d, pClient->window, m_xconn.root(), 0, 0);
    XMoveResizeWindow(d, pClient->window, 0, 0, m_xconn.width(), m_xconn.height());
    XMapWindow(d, pClient->window);
    XUnmapWindow(d, pClient->frame);
    XRaiseWindow(d, pClient->window);
    XSetInputFocus(d, pClient->window, RevertToPointerRoot, CurrentTime);

    XEvent ce{};
    ce.xconfigure.type = ConfigureNotify;
    ce.xconfigure.event = pClient->window;
    ce.xconfigure.window = pClient->window;
    ce.xconfigure.x = 0;
    ce.xconfigure.y = 0;
    ce.xconfigure.width = m_xconn.width();
    ce.xconfigure.height = m_xconn.height();
    ce.xconfigure.border_width = 0;
    ce.xconfigure.above = None;
    ce.xconfigure.override_redirect = False;
    XSendEvent(d, pClient->window, False, StructureNotifyMask, &ce);

    // EWMH state
    Atom state = m_xconn.atomNetWmState();
    Atom fs = m_xconn.atomNetWmStateFullscreen();
    XChangeProperty(d, pClient->window, state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&fs), 1);

    if (m_onFullscreen)
    {
      m_onFullscreen(true);
    }
  }
  else if (!enable && pClient->fullscreen)
  {
    pClient->fullscreen = false;
    pClient->maximized = false;
    pClient->x = pClient->oldX;
    pClient->y = pClient->oldY;
    pClient->width = pClient->oldWidth;
    pClient->height = pClient->oldHeight;

    // Reparent back can emit UnmapNotify; ignore expected ones.
    pClient->ignoreUnmap += 2;

    int bx = pClient->csd ? 0 : MewConst::borderWidth;
    int by = pClient->csd ? 0 : MewConst::titleHeight;
    XReparentWindow(d, pClient->window, pClient->frame, bx, by);
    XMapWindow(d, pClient->window);
    XMapRaised(d, pClient->frame);
    XMoveResizeWindow(
      d, pClient->window, bx, by,
      pClient->width, pClient->height);
    XMoveResizeWindow(
      d, pClient->frame, pClient->x, pClient->y,
      pClient->width + bx * 2,
      pClient->height + by + bx);
    if (!pClient->csd)
    {
      drawFrame(pClient);
    }

    //XReparentWindow(
    //  d, pClient->window, pClient->frame,
    //  MewConst::borderWidth, MewConst::titleHeight);
    //// Reparent may leave client unmapped; force both mapped + raised.
    //XMapWindow(d, pClient->window);
    //XMapRaised(d, pClient->frame);
    //XMoveResizeWindow(
    //  d, pClient->window,
    //  MewConst::borderWidth, MewConst::titleHeight,
    //  pClient->width, pClient->height);
    //XMoveResizeWindow(
    //  d, pClient->frame, pClient->x, pClient->y,
    //  pClient->width + MewConst::borderWidth * 2,
    //  pClient->height + MewConst::titleHeight + MewConst::borderWidth);
    //drawFrame(pClient);

    Atom state = m_xconn.atomNetWmState();
    XDeleteProperty(d, pClient->window, state);

    if (m_onFullscreen)
    {
      m_onFullscreen(false);
    }
    focus(pClient);
  }
}

void ClientManager::snap(Client* pClient, const std::string& edge)
{
  if (!pClient)
  {
    return;
  }

  if (pClient->fullscreen)
  {
    setFullscreen(pClient, false);
  }
  int bx = pClient->csd ? 0 : MewConst::borderWidth;
  int by = pClient->csd ? 0 : MewConst::titleHeight;
  int bx2 = bx * 2;
  int byb = by + bx;


  int screenW = m_xconn.width();
  int screenH = usableHeight();

  if (!pClient->maximized)
  {
    pClient->oldX = pClient->x;
    pClient->oldY = pClient->y;
    pClient->oldWidth = pClient->width;
    pClient->oldHeight = pClient->height;
  }
  pClient->maximized = false;

  if (edge == "left")
  {
    pClient->x = 0;
    pClient->y = 0;
    pClient->width = screenW / 2 - bx2;
    pClient->height = screenH - byb;
  }
  else if (edge == "right")
  {
    pClient->x = screenW / 2;
    pClient->y = 0;
    pClient->width = screenW / 2 - bx2;
    pClient->height = screenH - byb;
  }
  else if (edge == "top")
  {
    pClient->x = 0;
    pClient->y = 0;
    pClient->width = screenW - bx2;
    pClient->height = screenH / 2 - byb;
  }
  else if (edge == "bottom")
  {
    pClient->x = 0;
    pClient->y = screenH / 2;
    pClient->width = screenW - bx2;
    pClient->height = screenH / 2 - byb;
  }
  //if (edge == "left")
  //{
  //  pClient->x = 0;
  //  pClient->y = 0;
  //  pClient->width = screenW / 2 - MewConst::borderWidth * 2;
  //  pClient->height = screenH - MewConst::titleHeight - MewConst::borderWidth;
  //}
  //else if (edge == "right")
  //{
  //  pClient->x = screenW / 2;
  //  pClient->y = 0;
  //  pClient->width = screenW / 2 - MewConst::borderWidth * 2;
  //  pClient->height = screenH - MewConst::titleHeight - MewConst::borderWidth;
  //}
  //else if (edge == "top")
  //{
  //  pClient->x = 0;
  //  pClient->y = 0;
  //  pClient->width = screenW - MewConst::borderWidth * 2;
  //  pClient->height = screenH / 2 - MewConst::titleHeight - MewConst::borderWidth;
  //}
  //else if (edge == "bottom")
  //{
  //  pClient->x = 0;
  //  pClient->y = screenH / 2;
  //  pClient->width = screenW - MewConst::borderWidth * 2;
  //  pClient->height = screenH / 2 - MewConst::titleHeight - MewConst::borderWidth;
  //}
  else
  {
    return;
  }

  if (pClient->width < MewConst::minWidth)
  {
    pClient->width = MewConst::minWidth;
  }
  if (pClient->height < MewConst::minHeight)
  {
    pClient->height = MewConst::minHeight;
  }

  resize(pClient);
  focus(pClient);
}

void ClientManager::center(Client* pClient)
{
  if (!pClient)
  {
    return;
  }

  if (pClient->fullscreen)
  {
    setFullscreen(pClient, false);
  }
  if (pClient->maximized)
  {
    maximize(pClient);
  }

  int screenW = m_xconn.width();
  int screenH = usableHeight();
  pClient->width = (screenW * 2) / 3;
  pClient->height = (screenH * 2) / 3;

  int bx = pClient->csd ? 0 : MewConst::borderWidth;
  int by = pClient->csd ? 0 : MewConst::titleHeight;
  int frameW = pClient->width + bx * 2;
  int frameH = pClient->height + by + bx;

  //int frameW = pClient->width + MewConst::borderWidth * 2;
  //int frameH = pClient->height + MewConst::titleHeight + MewConst::borderWidth;
  pClient->x = (screenW - frameW) / 2;
  pClient->y = (screenH - frameH) / 2;
  if (pClient->x < 0)
  {
    pClient->x = 0;
  }
  if (pClient->y < 0)
  {
    pClient->y = 0;
  }
  resize(pClient);
  focus(pClient);
}

ResizeDirection ClientManager::resizeDirection(Client* pClient, int x, int y)
{
  int frameWidth = pClient->width + MewConst::borderWidth * 2;
  int frameHeight = pClient->height + MewConst::titleHeight + MewConst::borderWidth;

  bool left = x <= MewConst::resizeBorder;
  bool right = x >= frameWidth - MewConst::resizeBorder;
  bool top = y <= MewConst::resizeBorder;
  bool bottom = y >= frameHeight - MewConst::resizeBorder;

  if (left && top)
  {
    return ResizeDirection::TopLeft;
  }
  if (right && top)
  {
    return ResizeDirection::TopRight;
  }
  if (left && bottom)
  {
    return ResizeDirection::BottomLeft;
  }
  if (right && bottom)
  {
    return ResizeDirection::BottomRight;
  }
  if (left)
  {
    return ResizeDirection::Left;
  }
  if (right)
  {
    return ResizeDirection::Right;
  }
  if (top)
  {
    return ResizeDirection::Top;
  }
  if (bottom)
  {
    return ResizeDirection::Bottom;
  }
  return ResizeDirection::NoEdge;
}

Cursor ClientManager::cursorFor(ResizeDirection direction)
{
  switch (direction)
  {
    case ResizeDirection::Left:
    case ResizeDirection::Right:
      return m_xconn.cursorResizeH();
    case ResizeDirection::Top:
    case ResizeDirection::Bottom:
      return m_xconn.cursorResizeV();
    case ResizeDirection::TopLeft:
      return m_xconn.cursorResizeTl();
    case ResizeDirection::TopRight:
      return m_xconn.cursorResizeTr();
    case ResizeDirection::BottomLeft:
      return m_xconn.cursorResizeBl();
    case ResizeDirection::BottomRight:
      return m_xconn.cursorResizeBr();
    default:
      return m_xconn.cursorDefault();
  }
}

void ClientManager::resizeInteractive(Client* pClient, ResizeDirection direction)
{
  if (!pClient || pClient->maximized || pClient->fullscreen)
  {
    return;
  }

  Display* d = m_xconn.display();
  int startX = pClient->x;
  int startY = pClient->y;
  int startWidth = pClient->width;
  int startHeight = pClient->height;

  Window dummy = None;
  int rootX = 0;
  int rootY = 0;
  int winX = 0;
  int winY = 0;
  unsigned int mask = 0;
  XQueryPointer(d, m_xconn.root(), &dummy, &dummy, &rootX, &rootY, &winX, &winY, &mask);

  XGrabPointer(
    d, pClient->frame, False,
    ButtonMotionMask | ButtonReleaseMask,
    GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

  XEvent event;
  while (true)
  {
    XMaskEvent(d, ButtonMotionMask | ButtonReleaseMask, &event);
    if (event.type == MotionNotify)
    {
      int dx = event.xmotion.x_root - rootX;
      int dy = event.xmotion.y_root - rootY;
      int newX = startX;
      int newY = startY;
      int newWidth = startWidth;
      int newHeight = startHeight;

      if (direction == ResizeDirection::Left ||
          direction == ResizeDirection::TopLeft ||
          direction == ResizeDirection::BottomLeft)
      {
        newX = startX + dx;
        newWidth = startWidth - dx;
      }
      if (direction == ResizeDirection::Right ||
          direction == ResizeDirection::TopRight ||
          direction == ResizeDirection::BottomRight)
      {
        newWidth = startWidth + dx;
      }
      if (direction == ResizeDirection::Top ||
          direction == ResizeDirection::TopLeft ||
          direction == ResizeDirection::TopRight)
      {
        newY = startY + dy;
        newHeight = startHeight - dy;
      }
      if (direction == ResizeDirection::Bottom ||
          direction == ResizeDirection::BottomLeft ||
          direction == ResizeDirection::BottomRight)
      {
        newHeight = startHeight + dy;
      }

      if (newWidth < MewConst::minWidth)
      {
        if (direction == ResizeDirection::Left ||
            direction == ResizeDirection::TopLeft ||
            direction == ResizeDirection::BottomLeft)
        {
          newX -= MewConst::minWidth - newWidth;
        }
        newWidth = MewConst::minWidth;
      }
      if (newHeight < MewConst::minHeight)
      {
        if (direction == ResizeDirection::Top ||
            direction == ResizeDirection::TopLeft ||
            direction == ResizeDirection::TopRight)
        {
          newY -= MewConst::minHeight - newHeight;
        }
        newHeight = MewConst::minHeight;
      }

      pClient->x = newX;
      pClient->y = newY;
      pClient->width = newWidth;
      pClient->height = newHeight;
      resize(pClient);
    }
    if (event.type == ButtonRelease)
    {
      break;
    }
  }
  XUngrabPointer(d, CurrentTime);
}

void ClientManager::move(Client* pClient)
{
  if (!pClient || pClient->maximized || pClient->fullscreen)
  {
    return;
  }

  Display* d = m_xconn.display();
  Window dummy = None;
  int rootX = 0;
  int rootY = 0;
  int winX = 0;
  int winY = 0;
  unsigned int mask = 0;
  XQueryPointer(d, m_xconn.root(), &dummy, &dummy, &rootX, &rootY, &winX, &winY, &mask);

  int startX = pClient->x;
  int startY = pClient->y;

  XGrabPointer(
    d, pClient->frame, False,
    ButtonMotionMask | ButtonReleaseMask,
    GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

  XEvent event;
  while (true)
  {
    XMaskEvent(d, ButtonMotionMask | ButtonReleaseMask, &event);
    if (event.type == MotionNotify)
    {
      pClient->x = startX + (event.xmotion.x_root - rootX);
      pClient->y = startY + (event.xmotion.y_root - rootY);
      resize(pClient);
    }
    if (event.type == ButtonRelease)
    {
      break;
    }
  }
  XUngrabPointer(d, CurrentTime);
}

void ClientManager::manage(Window window)
{
  if (findClient(window))
  {
    return;
  }

  Display* d = m_xconn.display();
  XWindowAttributes attr;
  if (!XGetWindowAttributes(d, window, &attr))
  {
    return;
  }
  if (attr.override_redirect)
  {
    return;
  }

  // Detect client-side decorations (GTK, etc.) early so all geometry below uses it.
  bool hasCSD = false;
  {
    Atom gtkExtents = XInternAtom(d, "_GTK_FRAME_EXTENTS", False);
    Atom actualType;
    int actualFormat;
    unsigned long nitems;
    unsigned long bytesAfter;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(d, window, gtkExtents, 0, 4, False, XA_CARDINAL,
        &actualType, &actualFormat, &nitems, &bytesAfter, &data) == Success && data)
    {
      hasCSD = true;
      XFree(data);
    }
  }
  int bx = hasCSD ? 0 : MewConst::borderWidth;
  int by = hasCSD ? 0 : MewConst::titleHeight;

  //// Detect client-side decorations (GTK, etc.)
  //bool hasCSD = false;
  //{
  //  Atom gtkExtents = XInternAtom(d, "_GTK_FRAME_EXTENTS", False);
  //  Atom actualType;
  //  int actualFormat;
  //  unsigned long nitems;
  //  unsigned long bytesAfter;
  //  unsigned char* data = nullptr;
  //  if (XGetWindowProperty(d, window, gtkExtents, 0, 4, False, XA_CARDINAL,
  //      &actualType, &actualFormat, &nitems, &bytesAfter, &data) == Success && data)
  //  {
  //    hasCSD = true;
  //    XFree(data);
  //  }
  //}

  int screenW = m_xconn.width();
  int screenH = usableHeight();

  // Detect dialogs / transient windows (e.g. nemo extract progress)
  Window transientFor = None;
  bool isTransient = (XGetTransientForHint(d, window, &transientFor) && transientFor != None);

  // Respect application size; only enforce a small floor (not 2/3 screen)
  int w = attr.width;
  int h = attr.height;
  if (w < MewConst::minWidth)
  {
    w = MewConst::minWidth;
  }
  if (h < MewConst::minHeight)
  {
    h = MewConst::minHeight;
  }

  // Clamp to usable screen
  if (w > screenW - bx * 2)
  {
    w = screenW - bx * 2;
  }
  if (h > screenH - by - bx)
  {
    h = screenH - by - bx;
  }

  int frameW = w + bx * 2;
  int frameH = h + by + bx;
  int x = (screenW - frameW) / 2;
  int y = (screenH - frameH) / 2;
  if (x < 0)
  {
    x = 0;
  }
  if (y < 0)
  {
    y = 0;
  }

  Client* pClient = new Client{};
  pClient->window = window;
  pClient->csd = hasCSD;
  pClient->frame = XCreateSimpleWindow(
    d, m_xconn.root(), x, y, frameW, frameH, 0,
    hasCSD ? 0 : MewConst::colorBorder,
    hasCSD ? 0 : MewConst::colorTitle);
    //MewConst::colorBorder, MewConst::colorTitle);
  pClient->x = x;
  pClient->y = y;
  pClient->width = w;
  pClient->height = h;
  pClient->oldX = x;
  pClient->oldY = y;
  pClient->oldWidth = w;
  pClient->oldHeight = h;
  pClient->transient = isTransient;
  pClient->noMaximize = hasCSD ? true : isTransient;
    //hasCSD ? true : isTransient;//isTransient;

  // Per-app geometry from config (matched by WM_CLASS)
  if (m_pConfig)
  {
    XClassHint hint{};
    if (XGetClassHint(d, window, &hint))
    {
      std::string inst = hint.res_name ? hint.res_name : "";
      std::string cls = hint.res_class ? hint.res_class : "";
      if (hint.res_name)
      {
        XFree(hint.res_name);
      }
      if (hint.res_class)
      {
        XFree(hint.res_class);
      }
      const Config::AppGeometry* geo = m_pConfig->appGeometry(inst);
      if (!geo)
      {
        geo = m_pConfig->appGeometry(cls);
      }
      if (geo)
      {
        if (geo->hasW)
        {
          w = geo->width;
        }
        if (geo->hasH)
        {
          h = geo->height;
        }
        if (w < MewConst::minWidth)
        {
          w = MewConst::minWidth;
        }
        if (h < MewConst::minHeight)
        {
          h = MewConst::minHeight;
        }
        frameW = w + MewConst::borderWidth * 2;
        frameH = h + MewConst::titleHeight + MewConst::borderWidth;
        if (geo->hasX)
        {
          x = geo->x;
        }
        else
        {
          x = (screenW - frameW) / 2;
        }
        if (geo->hasY)
        {
          y = geo->y;
        }
        else
        {
          y = (screenH - frameH) / 2;
        }
        pClient->x = x;
        pClient->y = y;
        pClient->width = w;
        pClient->height = h;
        pClient->oldX = x;
        pClient->oldY = y;
        pClient->oldWidth = w;
        pClient->oldHeight = h;
        XMoveResizeWindow(d, pClient->frame, x, y, frameW, frameH);
        if (geo->maximized)
        {
          pClient->noMaximize = false;
        }
      }
    }
  }

  XSelectInput(d, pClient->frame, ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
  XAddToSaveSet(d, window);
  XSelectInput(d, window, StructureNotifyMask | PropertyChangeMask);

  int insetX = hasCSD ? 0 : MewConst::borderWidth;
  int insetY = hasCSD ? 0 : MewConst::titleHeight;
  XReparentWindow(d, window, pClient->frame, insetX, insetY);
      //MewConst::borderWidth, MewConst::titleHeight);
  XMapWindow(d, pClient->frame);
  XMapWindow(d, window);

  if (m_raiseOverlay)
  {
    m_raiseOverlay();
  }

  m_clients.push_back(pClient);
  resize(pClient);
  focus(pClient);

  if (m_pConfig)
  {
    XClassHint hint{};
    if (XGetClassHint(d, window, &hint))
    {
      std::string inst = hint.res_name ? hint.res_name : "";
      std::string cls = hint.res_class ? hint.res_class : "";
      if (hint.res_name)
      {
        XFree(hint.res_name);
      }
      if (hint.res_class)
      {
        XFree(hint.res_class);
      }
      const Config::AppGeometry* geo = m_pConfig->appGeometry(inst);
      if (!geo)
      {
        geo = m_pConfig->appGeometry(cls);
      }
      if (geo && geo->maximized)
      {
        maximize(pClient);
      }
    }
  }
}

void ClientManager::unmanage(Client* pClient)
{
  if (!pClient)
  {
    return;
  }

  Display* d = m_xconn.display();
  XReparentWindow(d, pClient->window, m_xconn.root(), pClient->x, pClient->y);
  XRemoveFromSaveSet(d, pClient->window);
  XDestroyWindow(d, pClient->frame);
  m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), pClient), m_clients.end());
  delete pClient;

  // Restore keyboard focus to another window (fixes neovim hollow cursor)
  if (!m_clients.empty())
  {
    focus(m_clients.back());
  }
}

void ClientManager::handleButtonPress(XButtonEvent* pEvent)
{
  if (!pEvent)
  {
    return;
  }

  Client* pClient = findClient(pEvent->window);
  if (!pClient)
  {
    return;
  }

  focus(pClient);
  if (pClient->csd)
  {
    return;  // CSD app handles its own titlebar
  }

  ResizeDirection direction = resizeDirection(pClient, pEvent->x, pEvent->y);

  if (pEvent->button == Button1 && direction != ResizeDirection::NoEdge)
  {
    resizeInteractive(pClient, direction);
    return;
  }

  if (pEvent->y >= MewConst::resizeBorder && pEvent->y < MewConst::titleHeight)
  {
    int frameWidth = pClient->width + MewConst::borderWidth * 2;
    int closeX = frameWidth - MewConst::buttonWidth - MewConst::borderWidth;
    int maxX = closeX - MewConst::buttonWidth;
    int minX = maxX - MewConst::buttonWidth;

    if (pEvent->button == Button1)
    {
      if (pEvent->x >= minX && pEvent->x < maxX)
      {
        minimize(pClient);
        return;
      }
      if (pEvent->x >= maxX && pEvent->x < closeX)
      {
        if (!pClient->noMaximize)
        {
          maximize(pClient);
        }
        return;
      }
      if (pEvent->x >= closeX)
      {
        close(pClient);
        return;
      }
    }

    if (pEvent->button == Button1 && pEvent->x < minX)
    {
      Time now = pEvent->time;
      if (pClient->lastTitleClick != 0 && now - pClient->lastTitleClick < 400)
      {
        pClient->lastTitleClick = 0;
        maximize(pClient);
        return;
      }
      pClient->lastTitleClick = now;
      move(pClient);
    }
  }
}

void ClientManager::handleMotion(XMotionEvent* pEvent)
{
  if (!pEvent)
  {
    return;
  }
  Client* pClient = findClient(pEvent->window);
  if (!pClient || pClient->csd)
  {
    return;
  }
  ResizeDirection direction = resizeDirection(pClient, pEvent->x, pEvent->y);
  XDefineCursor(m_xconn.display(), pClient->frame, cursorFor(direction));
}
