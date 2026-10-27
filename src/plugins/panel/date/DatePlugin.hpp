#pragma once
#include "../PanelPlugin.hpp"
#include "../XConnection.hpp"
#include "../FontRenderer.hpp"
#include "Config.hpp"
#include <ctime>
#include <string>

/**
 * @brief Panel plugin that displays the current date.
 *
 * The width is computed from the font metrics of a worst-case sample
 * string so that layout does not shift as digits change. Redraws only
 * when the date string changes (once a day).
 */
class DatePlugin : public PanelPlugin
{
public:
  /**
   * @brief Construct a date viewer bound to the given X connection and font.
   * @param xconn Open X connection.
   * @param font  Font renderer used for the date text.
   */
  DatePlugin(XConnection& xconn, FontRenderer& font)
    : m_xconn(xconn), m_font(font) {}

  /** @brief Plugin id used in config ("date"). */
  const char* id() const override { return "date"; }

  /** @brief Pixel width sized to fit a worst-case date sample. */
  int width() const override;

  /** @brief Anchor to the right group of the panel. */
  bool anchorRight() const override { return true; }

  /**
   * @brief Draw the date string.
   * @param d        X display.
   * @param panel    Panel pixmap/window.
   * @param x        Left edge of the plugin.
   * @param baseline Text baseline y.
   */
  void draw(Display* d, Window panel, int x, int baseline) override;

  /** @brief Clicks on the date are ignored. */
  bool onClick(int) override { return false; }

  /** @brief Local clicks are ignored. */
  bool handleLocalClick(int, int) override { return false; }

  /** @brief Tooltip text shown on hover. */
  std::string tooltip() const override { return "Date"; }

  /** @brief Request redraw when the calendar day changes. */
  bool tick() override;

private:
  XConnection& m_xconn;     ///< Open X connection.
  FontRenderer& m_font;     ///< Font renderer.
  std::string m_lastDate;   ///< Last drawn date string, throttles redraws.
};
