#pragma once
#include "panel/PanelPlugin.hpp"
#include "XConnection.hpp"
#include "FontRenderer.hpp"
#include <string>

/**
 * @brief Panel plugin that shows the active XKB keyboard layout.
 *
 * Clicking the label cycles to the next configured layout. The label
 * shows the layout's two-letter code (uppercase).
 */
class LanguagePlugin : public PanelPlugin
{
public:
  /**
   * @brief Construct a keyboard-layout indicator.
   * @param xconn Open X connection.
   * @param font  Font renderer for the label.
   */
  LanguagePlugin(XConnection& xconn, FontRenderer& font)
    : m_xconn(xconn), m_font(font) {}

  /** @brief Plugin id used in config ("language"). */
  const char* id() const override { return "language"; }

  /** @brief Pixel width sized to fit the current layout label. */
  int width() const override;

  /** @brief Anchor to the right group of the panel. */
  bool anchorRight() const override { return true; }

  /**
   * @brief Refresh the current layout and draw the label.
   * @param d        X display.
   * @param panel    Panel pixmap/window.
   * @param x        Left edge of the plugin.
   * @param baseline Text baseline y.
   */
  void draw(Display* d, Window panel, int x, int baseline) override;

  /** @brief Screen-coordinate click handler is unused. */
  bool onClick(int) override { return false; }

  /**
   * @brief Cycle XKB group to the next configured layout.
   * @return true (always consumes the click).
   */
  bool handleLocalClick(int, int) override;

  /** @brief Tooltip text. */
  std::string tooltip() const override { return "Keyboard layout (click to cycle)"; }

private:
  /** @brief Re-read the active XKB group and layout code list. */
  void refresh();

  XConnection& m_xconn;      ///< Open X connection.
  FontRenderer& m_font;      ///< Font renderer.
  std::string m_name = "??"; ///< Uppercased layout code (e.g. "US").
  int m_group = 0;           ///< Current XKB group index.
  int m_count = 1;           ///< Total number of configured layouts.
};
