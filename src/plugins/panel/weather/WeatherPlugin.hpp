#pragma once

#include "../PanelPlugin.hpp"
#include "PluginProcess.hpp"
#include "EmbeddedFont.hpp"
#include "XConnection.hpp"
#include "FontRenderer.hpp"
#include "Config.hpp"

#include <ctime>
#include <string>
#include <vector>

/**
 * @brief One row in the 10-day forecast popup.
 */
struct ForecastDay
{
  std::string date;   ///< Human-readable date, e.g. "Sep 13".
  std::string icon;   ///< Weather Icons PUA glyph for the code.
  std::string max;    ///< Maximum temperature (Celsius).
  std::string min;    ///< Minimum temperature (Celsius).
};

/**
 * @brief Panel plugin showing current weather and a 10-day forecast.
 *
 * Data comes from wttr.in (current conditions + lat/lon) and Open-Meteo
 * (10-day daily forecast). Both are fetched via curl in a forked child.
 * Clicking the widget opens the forecast popup; row colors are keyed
 * to the daily maximum temperature.
 */
class WeatherPlugin : public PanelPlugin
{
public:
  /**
   * @brief Construct a weather widget.
   * @param xconn Open X connection.
   * @param font  Font renderer for the text label.
   */
  WeatherPlugin(XConnection& xconn, FontRenderer& font);

  /** @brief Destroy the popup window if created. */
  ~WeatherPlugin() override;

  /** @brief Plugin id used in config ("weather"). */
  const char* id() const override { return "weather"; }

  /** @brief Fixed pixel width sized for icon + location + temperature. */
  int width() const override { return kWidth; }

  /**
   * @brief Draw the icon and location/temperature text.
   * @param display  X display.
   * @param panel    Panel pixmap/window.
   * @param x        Left edge of the plugin.
   * @param baseline Text baseline y.
   */
  void draw(Display* display, Window panel, int x, int baseline) override;

  /**
   * @brief Fetch current data if stale, then open the forecast popup.
   * @param screenX Absolute screen x of the plugin.
   * @return true if the popup was opened.
   */
  bool onClick(int screenX) override;

  /** @brief Tooltip text (location: description, temp). */
  std::string tooltip() const override;

  /** @brief Refresh data when the cache window has elapsed. */
  bool tick() override;

  /** @brief Apply location override and colors from config. */
  void configure(const Config& config) override;

  /** @brief Hover hook (currently unused). */
  void onHover(int screenX) override;

  /** @brief Unhover hook (currently unused). */
  void onUnhover() override;

  /** @brief Close the forecast popup on ESC. */
  bool handleEscape() override;

  /** @brief Optional out-of-process backend for the plugin. */
  PluginProcess* process() const override { return const_cast<PluginProcess*>(&m_process); }

  /** @brief Return the forecast popup window, or None. */
  Window popupWindow() const override { return m_popup; }

  /** @brief Redraw the forecast popup. */
  void drawPopup() override;

private:
  enum class FetchState { Idle, Current, Forecast };

  /** @brief Start background fetch for current conditions. */
  void startFetch();

  /** @brief Parse current conditions from JSON. */
  bool parseCurrent(const std::string& json);

  /**
   * @brief Start background fetch for the 10-day forecast.
   * @param lat Latitude string.
   * @param lon Longitude string.
   */
  void startFetchForecast(const std::string& lat, const std::string& lon);

  /** @brief Parse the forecast from JSON. */
  bool parseForecast(const std::string& json);

  /**
   * @brief Pick a Weather Icons glyph from a text description.
   * @param desc wttr.in description ("Sunny", "Patchy rain", ...).
   */
  static const char* iconForDesc(const std::string& desc);

  /**
   * @brief Pick a Weather Icons glyph from a WMO weather code.
   * @param code Open-Meteo weather_code.
   */
  static const char* iconForCode(int code);

  /**
   * @brief Map the forecast popup anchored to the plugin.
   * @param screenX Absolute screen x of the plugin.
   */
  void showPopup(int screenX);

  /** @brief Unmap the popup and release the ESC grab. */
  void hidePopup();

  XConnection& m_xconn;      ///< Open X connection.
  FontRenderer& m_font;      ///< Font renderer for the text label.
  time_t m_lastFetch = 0;    ///< Timestamp of the last successful fetch.
  bool m_valid = false;      ///< True once a successful fetch has happened.

  std::string m_temp;             ///< Current temperature (Celsius, string).
  std::string m_desc;             ///< Current weather description.
  std::string m_location;         ///< Resolved area name.
  std::string m_icon;             ///< Current icon glyph.
  std::string m_locationOverride; ///< User-specified location override.

  std::vector<ForecastDay> m_forecast;  ///< Up to 10 daily forecast rows.
  EmbeddedFont m_iconFont;              ///< Embedded Weather Icons font.

  PluginProcess m_process;
  FetchState m_fetchState = FetchState::Idle;
  std::string m_lat;
  std::string m_lon;
  unsigned long m_iconColor = 0xffffff; ///< Icon color (config).
  unsigned long m_textColor = 0xffffff; ///< Text color (config).

  Window m_popup = None;      ///< Forecast popup window.
  int m_popupW = 0;           ///< Popup width in pixels.
  int m_popupH = 0;           ///< Popup height in pixels.
  bool m_popupActive = false; ///< True while the popup is mapped.
  bool m_escGrabbed = false;  ///< True while ESC is grabbed on root.

  static constexpr int kRefreshSeconds = 30 * 60;  ///< Data cache lifetime.
  static constexpr int kWidth = 140;                ///< Panel width.
  static constexpr int kPopupWidth = 280;           ///< Forecast popup width.
  static constexpr int kPopupRowH = 28;             ///< Forecast row height.
  static constexpr int kPopupPad = 8;               ///< Forecast inner pad.
};
