#pragma once

#include "FontRenderer.hpp"
#include "Types.hpp"
#include "XConnection.hpp"
#include "plugins/panel/PanelPlugin.hpp"

#include <vector>

#include <X11/Xlib.h>
#include <string>
#include <vector>

/**
 * @brief Floating window that displays mew's system diagnostics.
 *
 * Shows memory usage, uptime, and the status of out-of-process plugins.
 */
class DiagnosticsWindow
{
public:
  /**
   * @brief Construct an unbound diagnostics viewer.
   * @param xconn Open X connection.
   * @param font Font renderer for title and body text.
   */
  DiagnosticsWindow(XConnection& xconn, FontRenderer& font);

  /**
   * @brief Destroy the X window if created.
   */
  ~DiagnosticsWindow();

  /**
   * @brief Build the display list, map the window centered, and draw.
   */
  void show(const std::vector<PanelPlugin*>& plugins);
  void tick();

  /**
   * @brief Unmap the window.
   */
  void hide();

  /**
   * @brief Raise the window if it is currently active.
   */
  void raiseIfActive();

  /**
   * @brief Redraw titlebar and diagnostic lines.
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
  /** @brief Rebuild m_lines from system state. */
  void buildDisplay();

  /** @brief Apply m_x/m_y/m_width/m_height via XMoveResizeWindow and redraw. */
  void applyGeometry();

  /** @brief Interactive drag of the window by the titlebar. */
  void moveInteractive();

  XConnection& m_xconn;
  FontRenderer& m_font;

  Window m_window = None;
  bool m_active = false;
  int m_x = 0;
  int m_y = 0;
  int m_width = 600;
  int m_height = 300;
  std::vector<std::string> m_lines;
  std::vector<PanelPlugin*> m_pPlugins;

  static constexpr int kLineHeight = 20;
  static constexpr int kPadding = 12;
  static constexpr int kDefaultWidth = 600;
};
