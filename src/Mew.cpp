#include "Mew.hpp"
#include "Util.hpp"
#include "panel/MusicPlayerWidget.hpp"

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <csignal>
#include <unistd.h>

Mew* Mew::s_pInstance = nullptr;

Mew::Mew()
{
  s_pInstance = this;
}

Mew::~Mew()
{
  delete m_pPanel;
  m_pPanel = nullptr;
  delete m_pLauncher;
  m_pLauncher = nullptr;
  delete m_pKeybindings;
  m_pKeybindings = nullptr;
  delete m_pPower;
  m_pPower = nullptr;
  delete m_pSwitcher;
  m_pSwitcher = nullptr;
  delete m_pClients;
  m_pClients = nullptr;

  if (m_config.useEmbeddedLogoutSound())
  {
    m_sound.playEmbeddedLogout();
  }
  else
  {
    m_sound.play(m_config.logoutSound());
  }

  Util::removePidfile();
  s_pInstance = nullptr;
}

Mew* Mew::instance()
{
  return s_pInstance;
}

void Mew::requestQuit()
{
  m_shouldQuit = true;
}

void Mew::requestReconfigure()
{
  m_needReconfigure = true;
}

void Mew::onShowLauncher()
{
  if (s_pInstance && s_pInstance->m_pLauncher)
  {
    s_pInstance->m_pLauncher->show();
  }
}

void Mew::onShowKeybindings()
{
  if (s_pInstance && s_pInstance->m_pKeybindings)
  {
    s_pInstance->m_pKeybindings->show();
  }
}

void Mew::onQuit()
{
  if (s_pInstance)
  {
    s_pInstance->requestQuit();
  }
}

void Mew::onReconfigure()
{
  if (s_pInstance)
  {
    s_pInstance->requestReconfigure();
  }
}

void Mew::onRaiseOverlays()
{
  if (!s_pInstance)
  {
    return;
  }
  s_pInstance->raiseOverlays();
}

void Mew::onFullscreen(bool enter)
{
  if (!s_pInstance || !s_pInstance->m_pPanel)
  {
    return;
  }
  s_pInstance->m_pPanel->setVisible(!enter);
  if (!enter)
  {
    s_pInstance->m_pPanel->raise();
  }
}

int Mew::errorHandler(Display*, XErrorEvent*)
{
  return 0;
}

void Mew::raiseOverlays()
{
  if (m_pKeybindings)
  {
    m_pKeybindings->raiseIfActive();
  }
  if (m_pPanel)
  {
    m_pPanel->raise();
  }
}

void Mew::grabKey(KeyCode keycode, unsigned int modifiers)
{
  unsigned int lockMasks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
  for (unsigned int lock : lockMasks)
  {
    XGrabKey(
      m_xconn.display(), keycode, modifiers | lock, m_xconn.root(),
      True, GrabModeAsync, GrabModeAsync);
  }
}

void Mew::grabKeys()
{
  Display* d = m_xconn.display();
  grabKey(XKeysymToKeycode(d, XK_Tab), Mod1Mask);
  grabKey(XKeysymToKeycode(d, XK_Tab), Mod1Mask | ShiftMask);
  grabKey(XKeysymToKeycode(d, XK_F4), Mod1Mask);
  grabKey(XKeysymToKeycode(d, XK_F1), Mod1Mask);
  grabKey(XKeysymToKeycode(d, XK_q), Mod1Mask | ShiftMask);

  for (const KeyBinding& binding : m_config.keybindings())
  {
    grabKey(binding.keycode, binding.modifiers);
  }
}

bool Mew::handleCustomKeybinding(XKeyEvent* pEvent)
{
  if (!pEvent || !m_pClients)
  {
    return false;
  }

  unsigned int state = pEvent->state & ~(LockMask | Mod2Mask);
  Client* pClient = m_pClients->focusedClient();

  for (const KeyBinding& binding : m_config.keybindings())
  {
    if (pEvent->keycode != binding.keycode || state != binding.modifiers)
    {
      continue;
    }

    const std::string& cmd = binding.command;
    if (cmd == "snap-left")
    {
      m_pClients->snap(pClient, "left");
      return true;
    }
    if (cmd == "snap-right")
    {
      m_pClients->snap(pClient, "right");
      return true;
    }
    if (cmd == "snap-top")
    {
      m_pClients->snap(pClient, "top");
      return true;
    }
    if (cmd == "snap-bottom")
    {
      m_pClients->snap(pClient, "bottom");
      return true;
    }
    if (cmd == "maximize")
    {
      m_pClients->maximize(pClient);
      return true;
    }
    if (cmd == "minimize")
    {
      m_pClients->minimize(pClient);
      return true;
    }
    if (cmd == "fullscreen")
    {
      if (pClient)
      {
        m_pClients->setFullscreen(pClient, !pClient->fullscreen);
      }
      return true;
    }
    if (cmd == "center")
    {
      m_pClients->center(pClient);
      return true;
    }
    if (cmd == "apps" || cmd == "open-apps")
    {
      if (m_pLauncher)
      {
        m_pLauncher->show();
      }
      return true;
    }
    if (cmd == "keybindings" || cmd == "open-keybindings")
    {
      if (m_pKeybindings)
      {
        m_pKeybindings->show();
      }
      return true;
    }
    if (cmd == "logout" || cmd == "quit")
    {
      m_shouldQuit = true;
      return true;
    }
    if (cmd == "reconfigure")
    {
      m_needReconfigure = true;
      return true;
    }
    if (cmd == "reboot")
    {
      if (m_pPanel)
      {
        // reuse panel power actions via simulated index would be awkward;
        // call system path same as panel
      }
      std::system("dbus-send --system --print-reply "
                  "--dest=org.freedesktop.login1 /org/freedesktop/login1 "
                  "org.freedesktop.login1.Manager.Reboot boolean:false "
                  ">/dev/null 2>&1");
      return true;
    }
    if (cmd == "poweroff")
    {
      std::system("dbus-send --system --print-reply "
                  "--dest=org.freedesktop.login1 /org/freedesktop/login1 "
                  "org.freedesktop.login1.Manager.PowerOff boolean:false "
                  ">/dev/null 2>&1");
      return true;
    }
    if (cmd == "power-manager" || cmd == "open-power")
    {
      if (m_pPower)
      {
        m_pPower->show();
      }
      return true;
    }

    std::string command = cmd + " >/dev/null 2>&1 &";
    printf("mew: running: %s\n", cmd.c_str());
    std::system(command.c_str());
    // If volume keybinding, refresh panel volume display
    if (m_pPanel
        && (cmd.find("amixer") != std::string::npos
            || cmd.find("pactl") != std::string::npos
            || cmd.find("wpctl") != std::string::npos))
    {
      struct timespec ts = {0, 80 * 1000 * 1000};
      nanosleep(&ts, nullptr);
      m_pPanel->draw();
    }
    return true;
  }
  return false;
}

void Mew::runAutostart()
{
  std::string path = Util::getConfigDirectory() + "/autostart";
  std::ifstream file(path);
  if (!file.is_open())
  {
    fprintf(stderr, "mew: no autostart file: %s\n", path.c_str());
    return;
  }

  std::string line;
  while (std::getline(file, line))
  {
    line = Util::trim(line);
    if (line.empty() || line[0] == '#')
    {
      continue;
    }
    printf("mew: autostart: %s\n", line.c_str());
    std::string command = line + " >/dev/null 2>&1 &";
    std::system(command.c_str());
  }
}

void Mew::reconfigure()
{
  printf("mew: reconfiguring...\n");
  XUngrabKey(m_xconn.display(), AnyKey, AnyModifier, m_xconn.root());
  m_config.load();
  m_config.loadKeybindings(m_xconn.display());
  m_background.apply(m_xconn, m_config);
  if (m_pPanel)
  {
    m_pPanel->setBackgroundColor(m_config.panelColor());
    m_pPanel->setItemColor(m_config.panelItemColor());
    m_pPanel->setHoverColor(m_config.panelHoverColor());
    m_pPanel->draw();
  }
  if (m_pClients)
  {
    for (Client* pClient : m_pClients->clients())
    {
      m_pClients->drawFrame(pClient);
    }
  }
  grabKeys();
}

void Mew::manageExisting()
{
  Window rootReturn = None;
  Window parentReturn = None;
  Window* children = nullptr;
  unsigned int childCount = 0;

  if (!XQueryTree(m_xconn.display(), m_xconn.root(), &rootReturn, &parentReturn, &children, &childCount))
  {
    return;
  }

  for (unsigned int i = 0; i < childCount; ++i)
  {
    XWindowAttributes attr;
    if (!XGetWindowAttributes(m_xconn.display(), children[i], &attr))
    {
      continue;
    }
    if (attr.map_state == IsViewable && !attr.override_redirect)
    {
      m_pClients->manage(children[i]);
    }
  }

  if (children)
  {
    XFree(children);
  }
}

void Mew::processEvent(XEvent& event)
{
  Display* d = m_xconn.display();

  switch (event.type)
  {
    case PropertyNotify:
    {
      Atom netName = m_xconn.atomNetWmName();
      if (event.xproperty.atom == XA_WM_NAME || event.xproperty.atom == netName)
      {
        Client* pClient = m_pClients->findClient(event.xproperty.window);
        if (pClient)
        {
          m_pClients->drawFrame(pClient);
        }
      }
      break;
    }

    case ClientMessage:
    {
      XClientMessageEvent* cm = &event.xclient;
      if (cm->message_type == m_xconn.atomNetWmMoveResize())
      {
        Client* pClient = m_pClients->findClient(cm->window);
        if (pClient && cm->data.l[2] == 8)  // _NET_WM_MOVERESIZE_MOVE
        {
          m_pClients->move(pClient);
        }
        break;
      }

      if (cm->message_type == m_xconn.atomNetWmState())
      {
        Client* pClient = m_pClients->findClient(cm->window);
        if (!pClient)
        {
          break;
        }
        long action = cm->data.l[0];
        Atom a1 = static_cast<Atom>(cm->data.l[1]);
        Atom a2 = static_cast<Atom>(cm->data.l[2]);
        bool wantFs = (a1 == m_xconn.atomNetWmStateFullscreen() || a2 == m_xconn.atomNetWmStateFullscreen());
        bool wantMax = (a1 == m_xconn.atomNetWmStateMaxVert() || a1 == m_xconn.atomNetWmStateMaxHorz()
                        || a2 == m_xconn.atomNetWmStateMaxVert() || a2 == m_xconn.atomNetWmStateMaxHorz());

        if (wantFs)
        {
          if (action == 1 || (action == 2 && !pClient->fullscreen))
          {
            m_pClients->setFullscreen(pClient, true);
          }
          else if (action == 0 || (action == 2 && pClient->fullscreen))
          {
            m_pClients->setFullscreen(pClient, false);
          }
        }
        else if (wantMax)
        {
          if (action == 1 || (action == 2 && !pClient->maximized))
          {
            if (!pClient->maximized)
            {
              m_pClients->maximize(pClient);
            }
          }
          else if (action == 0 || (action == 2 && pClient->maximized))
          {
            if (pClient->maximized)
            {
              m_pClients->maximize(pClient);
            }
          }
        }
      }
      break;
    }

    case MapRequest:
      m_pClients->manage(event.xmaprequest.window);
      break;

    case ConfigureRequest:
    {
      Client* pClient = m_pClients->findClient(event.xconfigurerequest.window);
      if (!pClient)
      {
        XWindowChanges changes{};
        changes.x = event.xconfigurerequest.x;
        changes.y = event.xconfigurerequest.y;
        changes.width = event.xconfigurerequest.width;
        changes.height = event.xconfigurerequest.height;
        changes.border_width = event.xconfigurerequest.border_width;
        changes.sibling = event.xconfigurerequest.above;
        changes.stack_mode = event.xconfigurerequest.detail;
        XConfigureWindow(d, event.xconfigurerequest.window, event.xconfigurerequest.value_mask, &changes);
        break;
      }
      // Fullscreen / maximized geometry is owned by the WM.
      if (pClient->fullscreen || pClient->maximized)
      {
        break;
      }
      if (event.xconfigurerequest.value_mask & CWX)
      {
        pClient->x = event.xconfigurerequest.x;
      }
      if (event.xconfigurerequest.value_mask & CWY)
      {
        pClient->y = event.xconfigurerequest.y;
      }
      if (event.xconfigurerequest.value_mask & CWWidth)
      {
        pClient->width = std::max(MewConst::minWidth, event.xconfigurerequest.width);
      }
      if (event.xconfigurerequest.value_mask & CWHeight)
      {
        pClient->height = std::max(MewConst::minHeight, event.xconfigurerequest.height);
      }
      m_pClients->resize(pClient);
      break;
    }

    case ButtonPress:
    {
      Window w = event.xbutton.window;
      if (m_pPanel)
      {
        for (PanelWidget* pW : m_pPanel->widgets())
        {
          if (pW->popupWindow() == w)
          {
            if (!pW->handlePopupClick(&event.xbutton))
            {
              pW->handleEscape();
            }
            break;
          }
        }
      }

      for (PanelWidget* pW : m_pPanel->widgets())
      {
        if (pW->popupWindow() != None && pW->popupWindow() != w)
        {
          pW->handleEscape();
        }
      }

      if (m_pKeybindings && m_pKeybindings->isActive() && w == m_pKeybindings->window())
      {
        m_pKeybindings->handleClick(&event.xbutton);
        break;
      }
      if (m_pPanel && w == m_pPanel->window())
      {
        m_pPanel->handleClick(event.xbutton.x);
        break;
      }
      if (m_pPanel && m_pPanel->isStartMenuActive() && w == m_pPanel->startMenuWindow())
      {
        m_pPanel->handleStartMenuClick(event.xbutton.y);
        break;
      }
      if (m_pPanel && m_pPanel->isPowerMenuActive() && w == m_pPanel->powerMenuWindow())
      {
        m_pPanel->handlePowerMenuClick(event.xbutton.y);
        break;
      }
      if (m_pPanel && m_pPanel->isNetworkMenuActive() && w == m_pPanel->networkMenuWindow())
      {
        m_pPanel->handleNetworkMenuClick(event.xbutton.y);
        break;
      }
      if (m_pLauncher && m_pLauncher->isActive() && w == m_pLauncher->window())
      {
        m_pLauncher->handleClick(&event.xbutton);
        break;
      }
      if (m_pPower && m_pPower->isActive() && w == m_pPower->window())
      {
        m_pPower->handleClick(&event.xbutton);
        break;
      }
      if (m_pPanel)
      {
        m_pPanel->hideMenus();
      }
      if (m_pLauncher && m_pLauncher->isActive())
      {
        m_pLauncher->hide();
      }
      if (m_pPower && m_pPower->isActive())
      {
        m_pPower->hide();
      }
      m_pClients->handleButtonPress(&event.xbutton);
      break;
    }

    case MotionNotify:
     if (m_pPanel)
      {
        for (PanelWidget* pW : m_pPanel->widgets())
        {
          if (pW->popupWindow() == event.xmotion.window)
          {
            pW->handlePopupMotion(&event.xmotion);
            break;
          }
        }
      }
      if (m_pPanel && event.xmotion.window == m_pPanel->window())
      {
        m_pPanel->handleMotion(event.xmotion.x);
        break;
      }
      m_pClients->handleMotion(&event.xmotion);
      break;

    case ButtonRelease:
    {
      if (m_pPanel)
      {
        for (PanelWidget* pW : m_pPanel->widgets())
        {
          if (pW->popupWindow() == event.xbutton.window)
          {
            MusicPlayerWidget* pm = dynamic_cast<MusicPlayerWidget*>(pW);
            if (pm)
            {
              pm->commitSeek();
            }
            break;
          }
        }
      }
      break;
    }
    case LeaveNotify:
      if (m_pPanel && event.xcrossing.window == m_pPanel->window())
      {
        m_pPanel->handleLeave();
      }
      break;

    case Expose:
    {
      Window w = event.xexpose.window;
      if (m_pSwitcher && w == m_pSwitcher->window())
      {
        m_pSwitcher->draw();
        break;
      }
      if (m_pKeybindings && w == m_pKeybindings->window())
      {
        m_pKeybindings->draw();
        break;
      }
      if (m_pPanel && w == m_pPanel->window())
      {
        m_pPanel->draw();
        break;
      }
      if (m_pPanel)
      {
        for (PanelWidget* pW : m_pPanel->widgets())
        {
          if (w == pW->popupWindow())
          {
            pW->drawPopup();
            break;
          }
        }
      }
      if (m_pLauncher && w == m_pLauncher->window())
      {
        m_pLauncher->draw();
        break;
      }
      if (m_pPower && w == m_pPower->window())
      {
        m_pPower->draw();
        break;
      }
      if (m_pPanel && w == m_pPanel->startMenuWindow())
      {
        m_pPanel->drawStartMenu();
        break;
      }
      if (m_pPanel && w == m_pPanel->powerMenuWindow())
      {
        m_pPanel->drawPowerMenu();
        break;
      }
      if (m_pPanel && w == m_pPanel->networkMenuWindow())
      {
        m_pPanel->drawNetworkMenu();
        break;
      }
      Client* pClient = m_pClients->findClient(w);
      if (pClient)
      {
        m_pClients->drawFrame(pClient);
      }
      break;
    }

    case DestroyNotify:
    {
      Client* pClient = m_pClients->findClient(event.xdestroywindow.window);
      if (pClient)
      {
        XDestroyWindow(d, pClient->frame);
        auto& list = m_pClients->clients();
        list.erase(std::remove(list.begin(), list.end(), pClient), list.end());
        delete pClient;
        if (!list.empty())
        {
          m_pClients->focus(list.back());
        }
      }
      break;
    }

    case UnmapNotify:
    {
      Client* pClient = m_pClients->findClient(event.xunmap.window);
      if (pClient && event.xunmap.window == pClient->window)
      {
        // Reparent/fullscreen can emit UnmapNotify; ignore those.
        if (pClient->ignoreUnmap > 0)
        {
          pClient->ignoreUnmap--;
          break;
        }
        m_pClients->unmanage(pClient);
      }
      break;
    }

    case KeyPress:
    {
      XKeyEvent* key = &event.xkey;
      if (m_pPanel)
      {
        bool handled = false;
        for (PanelWidget* pW : m_pPanel->widgets())
        {
          if (pW->hasFocusedPopup() && pW->handlePopupKey(key))
          {
            handled = true;
            break;
          }
        }
        if (handled) break;
      }

      KeySym keysymEarly = XLookupKeysym(key, 0);
      if (keysymEarly == XK_Escape)
      {
        if (m_pPanel && m_pPanel->handleEscape())
        {
          break;
        }
        if (m_pLauncher && m_pLauncher->isActive())
        {
          m_pLauncher->hide();
          break;
        }
        if (m_pKeybindings && m_pKeybindings->isActive())
        {
          m_pKeybindings->hide();
          break;
        }
        if (m_pPower && m_pPower->isActive())
        {
          m_pPower->hide();
          break;
        }
        if (m_pPanel && (m_pPanel->isStartMenuActive() || m_pPanel->isPowerMenuActive() || m_pPanel->isNetworkMenuActive()))
        {
          m_pPanel->hideMenus();
          break;
        }
      }
      if (m_pLauncher && m_pLauncher->isActive())
      {
        m_pLauncher->handleKey(key);
        break;
      }
      if (m_pKeybindings && m_pKeybindings->isActive())
      {
        m_pKeybindings->handleKey(key);
        break;
      }
      if (handleCustomKeybinding(key))
      {
        break;
      }
      if (m_pPower && m_pPower->isActive())
      {
        m_pPower->handleKey(key);
        break;
      }

      unsigned int state = key->state & ~(LockMask | Mod2Mask);
      KeySym keysym = XLookupKeysym(key, 0);

      if (keysym == XK_Tab && (state == Mod1Mask || state == (Mod1Mask | ShiftMask)))
      {
        bool reverse = (state & ShiftMask) != 0;
        m_pSwitcher->cycle(reverse);
        break;
      }
      if (state == Mod1Mask && keysym == XK_F4)
      {
        m_pClients->close(m_pClients->focusedClient());
        break;
      }
      if (state == (Mod1Mask | ShiftMask) && keysym == XK_q)
      {
        m_shouldQuit = true;
        break;
      }
      if (state == Mod1Mask && keysym == XK_F1)
      {
        m_pKeybindings->show();
        break;
      }
      break;
    }

    case KeyRelease:
    {
      KeySym keysym = XLookupKeysym(&event.xkey, 0);
      if ((keysym == XK_Alt_L || keysym == XK_Alt_R) && m_pSwitcher && m_pSwitcher->isActive())
      {
        m_pSwitcher->commit();
      }
      break;
    }

    default:
      break;
  }
}

int Mew::run()
{
  if (!m_xconn.open())
  {
    return -1;
  }

  XSetErrorHandler(errorHandler);

  Util::createConfigDirectory();
  m_config.load();
  m_config.loadKeybindings(m_xconn.display());
  m_font.load(m_xconn.display(), m_xconn.screen(), m_config.titleFontSize());
  m_xconn.loadCursors(m_config.mouseTheme().c_str(), m_config.mouseSize());
  m_background.apply(m_xconn, m_config);
  if (m_config.useEmbeddedSound())
  {
    m_sound.playEmbeddedLogin();
  }
  else
  {
    m_sound.play(m_config.loginSound());
  }

  m_pClients = new ClientManager(m_xconn, m_font);
  m_pClients->setPanelHeight(MewConst::panelHeight);
  m_pClients->setConfig(&m_config);
  m_pClients->setRaiseOverlay(onRaiseOverlays);
  m_pClients->setOnFullscreen(onFullscreen);

  m_pSwitcher = new WindowSwitcher(m_xconn, m_font, *m_pClients);
  m_pKeybindings = new KeybindingsWindow(m_xconn, m_font, m_config);
  m_pPower = new PowerWindow(m_xconn, m_font);
  m_pPower->setOnReconfigure(onReconfigure);
  m_pPower->setOnQuit(onQuit);
  m_pLauncher = new AppLauncher(m_xconn, m_font);
  m_pPanel = new Panel(m_xconn, m_font, *m_pClients);
  m_pPanel->setBackgroundColor(m_config.panelColor());
  m_pPanel->setItemColor(m_config.panelItemColor());
  m_pPanel->setHoverColor(m_config.panelHoverColor());
  m_pPanel->setConfig(&m_config);
  m_pPanel->setOnShowLauncher(onShowLauncher);
  m_pPanel->setOnShowKeybindings(onShowKeybindings);
  m_pPanel->setOnQuit(onQuit);
  m_pPanel->setOnReconfigure(onReconfigure);

  XSelectInput(
    m_xconn.display(),
    m_xconn.root(),
    SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | KeyReleaseMask | PropertyChangeMask);

  grabKeys();
  m_pPanel->create();
  Util::writePidfile();
  manageExisting();
  runAutostart();
  XSync(m_xconn.display(), False);

  while (!m_shouldQuit)
  {
    if (m_needReconfigure)
    {
      m_needReconfigure = false;
      reconfigure();
    }

    if (m_pSwitcher && m_pSwitcher->isActive() && !m_pSwitcher->isAltHeld())
    {
      m_pSwitcher->commit();
    }

    time_t now = time(nullptr);
    if (m_pPanel && now != m_pPanel->lastTime())
    {
      m_pPanel->draw();
    }
    if (m_pPanel)
    {
      m_pPanel->tick();
    }

    if (XPending(m_xconn.display()) == 0)
    {
      struct timespec ts = {0, 50 * 1000 * 1000};
      nanosleep(&ts, nullptr);
      continue;
    }

    XEvent event;
    XNextEvent(m_xconn.display(), &event);
    processEvent(event);
  }

  return 0;
}
