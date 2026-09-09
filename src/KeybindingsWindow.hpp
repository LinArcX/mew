#pragma once

#include "Config.hpp"
#include "FontRenderer.hpp"
#include "Types.hpp"
#include "XConnection.hpp"

#include <X11/Xlib.h>
#include <string>
#include <vector>

/**
 * @brief Floating window that lists configured keybindings.
 *
 * Supports show/hide, redraw, titlebar drag, and close-button click.
 */
class KeybindingsWindow
{
public:
  /**
   * @brief Construct an unbound keybindings viewer.
   * @param xconn Open X connection.
   * @param font Font renderer for title and body text.
   * @param config Config providing the keybinding list.
   */
  KeybindingsWindow(XConnection& xconn, FontRenderer& font, Config& config);

  /**
   * @brief Destroy the X window if created.
   */
  ~KeybindingsWindow();

  /**
   * @brief Build the display list, map the window centered, and draw.
   */
  void show();

  /**
   * @brief Unmap the window.
   */
  void hide();

  /**
   * @brief Raise the window if it is currently active.
   */
  void raiseIfActive();

  /**
   * @brief Redraw titlebar and keybinding lines.
   */
  void draw();

  /**
   * @brief Handle a button press on this window (close or drag).
   * @param pEvent X button event.
   */
  void handleClick(XButtonEvent* pEvent);

  /**
   * @brief Handle key while window is active (Escape closes).
   * @param pEvent X key event.
   */
  void handleKey(XKeyEvent* pEvent);

  /**
   * @brief Whether the window is mapped and active.
   */
  bool isActive() const { return m_active; }

  /**
   * @brief X window id, or None.
   */
  Window window() const { return m_window; }

private:
  /** @brief Rebuild m_lines from config keybindings. */
  void buildDisplay();

  /** @brief Apply m_x/m_y/m_width/m_height via XMoveResizeWindow and redraw. */
  void applyGeometry();

  /** @brief Interactive drag of the window by the titlebar. */
  void moveInteractive();

  XConnection& m_xconn;
  FontRenderer& m_font;
  Config& m_config;

  Window m_window = None;
  bool m_active = false;
  int m_x = 0;
  int m_y = 0;
  int m_width = 560;
  int m_height = 200;
  std::vector<std::string> m_lines;

  static constexpr int kLineHeight = 20;
  static constexpr int kPadding = 12;
  static constexpr int kDefaultWidth = 560;
};
