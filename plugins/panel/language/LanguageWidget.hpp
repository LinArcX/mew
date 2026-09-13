#pragma once
#include "panel/PanelWidget.hpp"
#include "XConnection.hpp"
#include "FontRenderer.hpp"
#include <string>

class LanguageWidget : public PanelWidget
{
public:
  LanguageWidget(XConnection& xconn, FontRenderer& font)
    : m_xconn(xconn), m_font(font) {}

  const char* id() const override { return "language"; }
  int width() const override;
  bool anchorRight() const override { return true; }
  void draw(Display* d, Window panel, int x, int baseline) override;
  bool onClick(int) override { return false; }
  bool handleLocalClick(int, int) override;
  std::string tooltip() const override { return "Keyboard layout (click to cycle)"; }

private:
  void refresh();
  XConnection& m_xconn;
  FontRenderer& m_font;
  std::string m_name = "??";
  int m_group = 0;
  int m_count = 1;
};
