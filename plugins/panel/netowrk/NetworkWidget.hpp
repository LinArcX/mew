#pragma once
#include "panel/PanelWidget.hpp"
#include "XConnection.hpp"
#include "FontRenderer.hpp"
#include <X11/Xlib.h>
#include <string>
#include <vector>

class NetworkWidget : public PanelWidget
{
public:
  NetworkWidget(XConnection& xconn, FontRenderer& font)
    : m_xconn(xconn), m_font(font) {}
  ~NetworkWidget() override;

  const char* id() const override { return "network"; }
  int width() const override { return 110; }
  bool anchorRight() const override { return true; }
  void draw(Display* d, Window panel, int x, int baseline) override;
  bool onClick(int) override { return false; }
  bool handleLocalClick(int, int screenX) override;
  std::string tooltip() const override { return "Network interface"; }
  bool tick() override;
  bool handleEscape() override;
  Window popupWindow() const override { return m_popup; }
  void drawPopup() override;
  bool handlePopupClick(XButtonEvent* e) override;
  bool hasFocusedPopup() const override { return m_popupActive; }

private:
  void refresh();
  bool isUp(const std::string& name) const;
  void killSwitch();
  void showPopup(int screenX);
  void hidePopup();

  XConnection& m_xconn;
  FontRenderer& m_font;

  std::vector<std::string> m_ifaces;
  std::string m_selected;
  Window m_popup = None;
  bool m_popupActive = false;
  time_t m_lastRefresh = 0;

  static constexpr int kPopupW = 200;
  static constexpr int kRowH = 28;
};
