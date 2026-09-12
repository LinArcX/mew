#pragma once

#include "PanelWidget.hpp"
#include "../EmbeddedFont.hpp"
#include "../XConnection.hpp"
#include "../FontRenderer.hpp"
#include "../Config.hpp"

#include <ctime>
#include <string>
#include <vector>

struct ForecastDay
{
  std::string date;
  std::string icon;
  std::string max;
  std::string min;
};

class WeatherWidget : public PanelWidget
{
public:
  WeatherWidget(XConnection& xconn, FontRenderer& font);
  ~WeatherWidget() override;

  const char* id() const override { return "weather"; }
  int width() const override { return kWidth; }
  void draw(Display* display, Window panel, int x, int baseline) override;
  bool onClick() override;
  std::string tooltip() const override;
  void tick() override;
  void configure(const Config& config) override;

  void onHover(int screenX) override;
  void onUnhover() override;
  bool handleEscape() override;
  Window popupWindow() const override { return m_popup; }
  void drawPopup() override;

private:
  bool fetch();
  bool fetchForecast(const std::string& lat, const std::string& lon);
  static const char* iconForDesc(const std::string& desc);
  static const char* iconForCode(int code);
  void showPopup(int screenX);
  void hidePopup();

  XConnection& m_xconn;
  FontRenderer& m_font;
  time_t m_lastFetch = 0;
  bool m_valid = false;

  std::string m_temp;
  std::string m_desc;
  std::string m_location;
  std::string m_icon;
  std::string m_locationOverride;

  std::vector<ForecastDay> m_forecast;
  EmbeddedFont m_iconFont;

  Window m_popup = None;
  int m_popupW = 0;
  int m_popupH = 0;
  bool m_popupActive = false;
  bool m_escGrabbed = false;

  static constexpr int kRefreshSeconds = 30 * 60;
  static constexpr int kWidth = 140;
  static constexpr int kPopupWidth = 240;
  static constexpr int kPopupRowH = 22;
  static constexpr int kPopupPad = 8;
};
