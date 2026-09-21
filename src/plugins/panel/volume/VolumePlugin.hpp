#pragma once
#include "panel/PanelPlugin.hpp"
#include "XConnection.hpp"
#include "FontRenderer.hpp"
#include "Config.hpp"
#include <X11/Xlib.h>
#include <string>

class VolumePlugin : public PanelPlugin
{
public:
  VolumePlugin(XConnection& xconn, FontRenderer& font)
    : m_xconn(xconn), m_font(font) {}
  ~VolumePlugin() override;

  const char* id() const override { return "volume"; }
  int width() const override { return 35; }
  bool anchorRight() const override { return true; }
  void draw(Display* d, Window panel, int x, int baseline) override;
  bool onClick(int) override { return false; }
  bool handleLocalClick(int localX, int screenX) override;
  std::string tooltip() const override;
  void configure(const Config& config) override { m_pConfig = &config; }
  bool tick() override;
  bool handleEscape() override;
  Window popupWindow() const override { return m_popup; }
  void drawPopup() override;
  bool handlePopupClick(XButtonEvent* pEvent) override;
  bool handlePopupMotion(XMotionEvent* pEvent) override;
  bool hasFocusedPopup() const override { return m_popupActive; }

private:
  void refresh();
  void setPercent(int pct);
  void toggleMute();
  void showPopup(int screenX);
  void hidePopup();
  void resolveControl(std::string& device, std::string& control) const;

  XConnection& m_xconn;
  FontRenderer& m_font;
  const Config* m_pConfig = nullptr;

  int m_percent = 0;
  bool m_muted = false;
  bool m_valid = false;

  Window m_popup = None;
  bool m_popupActive = false;
  bool m_dragging = false;
  time_t m_lastRefresh = 0;

  static constexpr int kPopupW = 60;
  static constexpr int kPopupH = 230;
};
