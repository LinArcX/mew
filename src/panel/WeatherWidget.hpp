#pragma once

#include "PanelWidget.hpp"
#include "../XConnection.hpp"
#include "../FontRenderer.hpp"

#include <ctime>
#include <string>

/**
 * @brief Panel widget showing current weather for the auto-detected location.
 *
 * Fetches from wttr.in in a forked child, parses JSON manually, refreshes
 * every 30 minutes. Uses Unicode weather symbols as icons.
 */
class WeatherWidget : public PanelWidget
{
public:
  WeatherWidget(XConnection& xconn, FontRenderer& font);
  ~WeatherWidget();

  const char* id() const override { return "weather"; }
  int width() const override { return kWidth; }
  void draw(Display* display, Window panel, int x, int baseline) override;
  bool onClick() override;
  std::string tooltip() const override;

  /** @brief Fetch if stale. Called by Panel each tick. */
  void tick() override;

  void configure(const Config& config) override;
private:
  bool fetch();
  static const char* iconFor(const std::string& desc);

  XConnection& m_xconn;
  FontRenderer& m_font;
  time_t m_lastFetch = 0;
  bool m_valid = false;

  std::string m_temp;
  std::string m_desc;
  std::string m_location;
  std::string m_icon;
  std::string m_locationOverride;

  XftFont* m_pIconFont = nullptr;
  void drawIcon(Display* display, Window panel, int x, int baseline, const std::string& icon);

  static constexpr int kRefreshSeconds = 30 * 60;
  static constexpr int kWidth = 140;
};
