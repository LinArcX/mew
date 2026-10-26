#pragma once
#include "panel/PanelPlugin.hpp"
#include "XConnection.hpp"
#include "FontRenderer.hpp"
#include "ClientManager.hpp"

/**
 * @brief Panel plugin that toggles the "show desktop" state.
 *
 * The first click minimizes every managed client so the root window is
 * visible; the second click restores the previously minimized clients
 * and focuses the top of the stacking list.
 */
class DesktopPlugin : public PanelPlugin
{
public:
  /**
   * @brief Construct a show-desktop toggle.
   * @param xconn   Open X connection.
   * @param font    Font renderer for the icon.
   * @param clients Client manager used for minimize/restore.
   */
  DesktopPlugin(XConnection& xconn, FontRenderer& font, ClientManager& clients)
    : m_xconn(xconn), m_font(font), m_clients(clients) {}

  /** @brief Plugin id used in config ("desktop"). */
  const char* id() const override { return "desktop"; }

  /** @brief Pixel width sized to fit the icon glyph. */
  int width() const override;

  /** @brief Anchor to the right group of the panel. */
  bool anchorRight() const override { return true; }

  /**
   * @brief Draw the show-desktop glyph.
   * @param d        X display.
   * @param panel    Panel pixmap/window.
   * @param x        Left edge of the plugin.
   * @param baseline Text baseline y.
   */
  void draw(Display* d, Window panel, int x, int baseline) override;

  /** @brief Screen-coordinate click handler is unused. */
  bool onClick(int) override { return false; }

  /**
   * @brief Toggle between minimized and restored client states.
   * @return true (always consumes the click).
   */
  bool handleLocalClick(int, int) override;

  /** @brief Tooltip text. */
  std::string tooltip() const override { return "Show desktop"; }

private:
  XConnection& m_xconn;      ///< Open X connection.
  FontRenderer& m_font;      ///< Font renderer.
  ClientManager& m_clients;  ///< Owns the managed client list.
  bool m_showing = false;    ///< True while the "desktop" state is active.
};
