#pragma once

#include "ClientManager.hpp"
#include "Config.hpp"
#include "FontRenderer.hpp"
#include "XConnection.hpp"
#include "panel/PanelWidgetRegistry.hpp"
#include "startMenu/StartMenuRegistry.hpp"

#include <X11/Xlib.h>
#include <string>
#include <vector>

/**
 * @brief Bottom panel with start menu and plugin widgets.
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
   * @brief Hide start and power menus.
   */
  void hideMenus();

  /**
   * @brief Redraw start menu if active (Expose).
   */
  void drawStartMenu();

  /**
   * @brief Panel X window, or None.
   */
  Window window() const { return m_window; }

  /**
   * @brief Start menu X window, or None.
   */
  Window startMenuWindow() const { return m_startMenu; }

  /**
   * @brief Whether start menu is mapped.
   */
  bool isStartMenuActive() const { return m_startMenuActive; }

  void tick();

  bool handleEscape();

  bool isItemSubmenuWindow(Window w) const;
  bool handleItemSubmenuClick(Window w, int y);
  void drawItemSubmenu(Window w);
  void hideItemSubmenus();

  std::vector<PanelWidget*>& widgets() { return m_widgets; }

private:
  void showStartMenu();
  void doReboot();
  void doPoweroff();
  void showTooltip(int x, const char* text);
  void hideTooltip();

  /** @brief Load assets/mew.png, downscale to kStartIconSize, store RGBA. */
  void loadStartIcon();

  StartMenuContext makeStartMenuContext();

  static constexpr int kStartIconSize = 26;
  std::vector<unsigned char> m_startIconRgba;  // 20x20x4, empty if unavailable

  XConnection& m_xconn;
  FontRenderer& m_font;
  ClientManager& m_clients;

  Window m_window = None;
  unsigned long m_bgColor = 0x222222;
  unsigned long m_itemColor = 0xffffff;
  unsigned long m_hoverColor = 0x0a64c8;
  int m_hoverZone = -1; // 0=start -1=none
  Window m_tooltip = None;
  const Config* m_pConfig = nullptr;

  Window m_startMenu = None;
  bool m_startMenuActive = false;

  Pixmap m_backBuffer = None;
  int m_backBufferW = 0;
  std::vector<PanelWidget*> m_widgets;
  PanelWidget* m_pHoverWidget = nullptr;

  std::vector<StartMenuItem*> m_startItems;
  int m_startMenuY = 0;

  void (*m_onShowLauncher)() = nullptr;
  void (*m_onShowKeybindings)() = nullptr;
  void (*m_onQuit)() = nullptr;
  void (*m_onReconfigure)() = nullptr;

  static constexpr int kStartMenuW = 180;
};
