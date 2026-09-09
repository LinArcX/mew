#pragma once

#include "AppLauncher.hpp"
#include "Background.hpp"
#include "ClientManager.hpp"
#include "Config.hpp"
#include "FontRenderer.hpp"
#include "KeybindingsWindow.hpp"
#include "Panel.hpp"
#include "Sound.hpp"
#include "WindowSwitcher.hpp"
#include "XConnection.hpp"

/**
 * @brief Top-level window manager orchestrator.
 *
 * Owns X connection and composes all subsystems. run() blocks in the
 * main event loop until logout.
 */
class Mew
{
public:
  /**
   * @brief Construct subsystems (does not open Display yet).
   */
  Mew();

  /**
   * @brief Tear down subsystems and remove pidfile.
   */
  ~Mew();

  /**
   * @brief Initialize Display, become WM, and enter the event loop.
   * @return Process exit code (0 on clean logout).
   */
  int run();

  /**
   * @brief Request graceful quit from the event loop.
   */
  void requestQuit();

  /**
   * @brief Request config/keybindings reload.
   */
  void requestReconfigure();

  /**
   * @brief Singleton pointer for C-style callbacks and signal handlers.
   */
  static Mew* instance();

private:
  void grabKey(KeyCode keycode, unsigned int modifiers);
  void grabKeys();
  bool handleCustomKeybinding(XKeyEvent* pEvent);
  void runAutostart();
  void reconfigure();
  void manageExisting();
  void processEvent(XEvent& event);
  void raiseOverlays();

  static void onShowLauncher();
  static void onShowKeybindings();
  static void onQuit();
  static void onReconfigure();
  static void onRaiseOverlays();
  static void onFullscreen(bool enter);
  static int errorHandler(Display* pDisplay, XErrorEvent* pEvent);

  XConnection m_xconn;
  Config m_config;
  FontRenderer m_font;
  Background m_background;
  Sound m_sound;
  ClientManager* m_pClients = nullptr;
  WindowSwitcher* m_pSwitcher = nullptr;
  KeybindingsWindow* m_pKeybindings = nullptr;
  AppLauncher* m_pLauncher = nullptr;
  Panel* m_pPanel = nullptr;

  bool m_shouldQuit = false;
  bool m_needReconfigure = false;

  static Mew* s_pInstance;
};
