#pragma once

#include "ClientManager.hpp"
#include "Config.hpp"
#include "FontRenderer.hpp"
#include "XConnection.hpp"

#include <X11/Xlib.h>
#include <ctime>
#include <string>
#include <vector>

/**
 * @brief Bottom panel with start menu, language switcher, volume, clock, and desktop toggle.
 *
 * Owns the start menu and nested PowerManager submenu. External actions
 * (launcher, keybindings, quit, reconfigure) are invoked via callbacks.
 */
class Panel
{
public:
  /**
   * @brief Construct a panel bound to X, font, and client manager.
   * @param xconn Open X connection.
   * @param font Font renderer.
   * @param clients Client manager used for show-desktop.
   */
  Panel(XConnection& xconn, FontRenderer& font, ClientManager& clients);

  /**
   * @brief Destroy panel and menu windows.
   */
  ~Panel();

  /**
   * @brief Set panel background color (from config).
   * @param color 0xRRGGBB pixel value.
   */
  void setBackgroundColor(unsigned long color);

  /**
   * @brief Set default item (text/icon) color.
   * @param color 0xRRGGBB pixel value.
   */
  void setItemColor(unsigned long color);

  /**
   * @brief Set hovered item highlight color.
   * @param color 0xRRGGBB pixel value.
   */
  void setHoverColor(unsigned long color);

  /**
   * @brief Config for volume commands from keybindings file.
   * @param pConfig Non-owning pointer.
   */
  void setConfig(const Config* pConfig) { m_pConfig = pConfig; }

  /**
   * @brief Hide or show the panel (used during true fullscreen).
   * @param visible true to map panel.
   */
  void setVisible(bool visible);

  /**
   * @brief Handle pointer motion over the panel (hover highlight + tooltip).
   * @param x Pointer x in panel coordinates.
   */
  void handleMotion(int x);

  /**
   * @brief Clear hover state when pointer leaves the panel.
   */
  void handleLeave();

  /**
   * @brief Callback when user picks Apps in the start menu.
   */
  void setOnShowLauncher(void (*fn)()) { m_onShowLauncher = fn; }

  /**
   * @brief Callback when user picks KeyBindings.
   */
  void setOnShowKeybindings(void (*fn)()) { m_onShowKeybindings = fn; }

  /**
   * @brief Callback when user picks Logout.
   */
  void setOnQuit(void (*fn)()) { m_onQuit = fn; }

  /**
   * @brief Callback when user picks Reconfigure mew.
   */
  void setOnReconfigure(void (*fn)()) { m_onReconfigure = fn; }

  /**
   * @brief Create and map the panel window.
   */
  void create();

  /**
   * @brief Redraw panel contents (clock, volume, icons).
   */
  void draw();

  /**
   * @brief Raise the panel above other windows.
   */
  void raise();

  /**
   * @brief Handle a click on the panel strip.
   * @param x Click x in panel coordinates.
   */
  void handleClick(int x);

  /**
   * @brief Handle a click on the start menu window.
   * @param y Click y in menu coordinates.
   */
  void handleStartMenuClick(int y);

  /**
   * @brief Handle a click on the power submenu.
   * @param y Click y in menu coordinates.
   */
  void handlePowerMenuClick(int y);

  /**
   * @brief Hide start and power menus.
   */
  void hideMenus();

  /**
   * @brief Open PowerManager submenu (for keybindings).
   */
  void openPowerMenu();

  /**
   * @brief Redraw start menu if active (Expose).
   */
  void drawStartMenu();

  /**
   * @brief Redraw power menu if active (Expose).
   */
  void drawPowerMenu();

  /**
   * @brief Handle a click on the network interface menu.
   * @param y Click y in menu coordinates.
   */
  void handleNetworkMenuClick(int y);

  /**
   * @brief Redraw network menu if active (Expose).
   */
  void drawNetworkMenu();

  /**
   * @brief Panel X window, or None.
   */
  Window window() const { return m_window; }

  /**
   * @brief Start menu X window, or None.
   */
  Window startMenuWindow() const { return m_startMenu; }

  /**
   * @brief Power menu X window, or None.
   */
  Window powerMenuWindow() const { return m_powerMenu; }

  /** @brief Network interface picker window, or None. */
  Window networkMenuWindow() const { return m_netMenu; }

  /** @brief Whether network menu is mapped. */
  bool isNetworkMenuActive() const { return m_netMenuActive; }

  /**
   * @brief Whether start menu is mapped.
   */
  bool isStartMenuActive() const { return m_startMenuActive; }

  /**
   * @brief Whether power menu is mapped.
   */
  bool isPowerMenuActive() const { return m_powerMenuActive; }

  /**
   * @brief Panel height in pixels.
   */
  int height() const { return MewConst::panelHeight; }

  /**
   * @brief Last clock update time (for periodic redraw).
   */
  time_t lastTime() const { return m_lastTime; }

private:
  void updateVolume();
  void toggleMute();
  void showStartMenu();
  void hidePowerMenu();
  void showPowerMenu();
  void drawMenuWindow(Window win, const std::vector<std::string>& items, int width);
  void doReboot();
  void doPoweroff();
  void toggleDesktop();
  int hitTest(int x) const;
  void showTooltip(int x, const char* text);
  void hideTooltip();
  void refreshLayout();
  void cycleLayout();
  void refreshNetwork();
  void showNetworkMenu();
  void hideNetworkMenu();
  void toggleKillSwitch();
  bool isInterfaceUp(const std::string& name) const;
  void runAudioCommand(const char* keyName);

  XConnection& m_xconn;
  FontRenderer& m_font;
  ClientManager& m_clients;

  Window m_window = None;
  unsigned long m_bgColor = 0x222222;
  unsigned long m_itemColor = 0xffffff;
  unsigned long m_hoverColor = 0x0a64c8;
  time_t m_lastTime = 0;
  bool m_desktopShowing = false;
  int m_volumePercent = -1;
  bool m_volumeMuted = false;
  int m_hoverZone = -1; // 0=start 1=lang 2=volume 3=desktop -1=none
  Window m_tooltip = None;
  std::string m_layoutName = "??";
  int m_layoutGroup = 0;
  int m_layoutCount = 1;
  const Config* m_pConfig = nullptr;

  std::vector<std::string> m_netIfaces;
  std::string m_selectedIface;
  Window m_netMenu = None;
  bool m_netMenuActive = false;
  static constexpr int kNetMenuW = 200;
  static constexpr int kNetMenuItemH = 28;

  Window m_startMenu = None;
  bool m_startMenuActive = false;
  Window m_powerMenu = None;
  bool m_powerMenuActive = false;

  void (*m_onShowLauncher)() = nullptr;
  void (*m_onShowKeybindings)() = nullptr;
  void (*m_onQuit)() = nullptr;
  void (*m_onReconfigure)() = nullptr;

  static constexpr int kMenuItemH = 32;
  static constexpr int kStartMenuW = 180;
  static constexpr int kPowerMenuW = 180;
};
