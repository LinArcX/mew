#pragma once

#include "FontRenderer.hpp"
#include "XConnection.hpp"
#include "Config.hpp"

#include <X11/Xlib.h>
#include <string>

/**
 * @brief Context handed to a start menu item on activation.
 */
struct StartMenuContext
{
  XConnection* pXconn = nullptr;
  FontRenderer* pFont = nullptr;
  const Config* pConfig = nullptr;

  void (*onShowLauncher)() = nullptr;
  void (*onShowKeybindings)() = nullptr;
  void (*onQuit)() = nullptr;
  void (*onReconfigure)() = nullptr;
};

/**
 * @brief One entry in the start menu.
 *
 * Items are leaves (do something) or parents (open a submenu). Parent items
 * own their own submenu window so hierarchies can nest without Panel having
 * to know about the tree.
 */
class StartMenuItem
{
public:
  virtual ~StartMenuItem() = default;

  /** Unique id used in config. */
  virtual const char* id() const = 0;

  /** Label shown in the parent menu. */
  virtual const char* label() const = 0;

  /** Row height in pixels. */
  virtual int height() const { return 32; }

  /**
   * Called when the item is clicked.
   * @param parentX,parentY,parentW Geometry of the parent menu window.
   * @return true to keep the parent menu open, false to close it.
   */
  virtual bool onActivate(StartMenuContext& ctx,
                          int parentX, int parentY, int parentW) = 0;

  /** Submenu window id (parent items), or None. */
  virtual Window submenuWindow() const { return None; }

  /** Redraw submenu (Expose). */
  virtual void drawSubmenu(Display* d, int screen) { (void)d; (void)screen; }

  /** Handle a click inside the submenu. Return true if consumed. */
  virtual bool handleSubmenuClick(int y) { (void)y; return false; }

  /** Close submenu if open. */
  virtual void hideSubmenu() {}
};
