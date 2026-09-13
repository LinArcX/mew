#pragma once
#include "../PanelWidget.hpp"
#include "../XConnection.hpp"
#include "../FontRenderer.hpp"
#include "Config.hpp"
#include <ctime>
#include <string>

class ClockWidget : public PanelWidget
{
public:
  ClockWidget(XConnection& xconn, FontRenderer& font)
    : m_xconn(xconn), m_font(font) {}

  const char* id() const override { return "clock"; }
  int width() const override;
  bool anchorRight() const override { return true; }
  void draw(Display* d, Window panel, int x, int baseline) override;
  bool onClick(int) override { return false; }
  bool handleLocalClick(int, int) override { return false; }
  std::string tooltip() const override;
  bool tick() override;

private:
  XConnection& m_xconn;
  FontRenderer& m_font;
  time_t m_lastDraw = 0;
};
