#pragma once
#include "panel/PanelWidget.hpp"
#include "XConnection.hpp"
#include "FontRenderer.hpp"
#include "ClientManager.hpp"

class DesktopWidget : public PanelWidget
{
public:
  DesktopWidget(XConnection& xconn, FontRenderer& font, ClientManager& clients)
    : m_xconn(xconn), m_font(font), m_clients(clients) {}

  const char* id() const override { return "desktop"; }
  int width() const override;
  bool anchorRight() const override { return true; }
  void draw(Display* d, Window panel, int x, int baseline) override;
  bool onClick(int) override { return false; }
  bool handleLocalClick(int, int) override;
  std::string tooltip() const override { return "Show desktop"; }

private:
  XConnection& m_xconn;
  FontRenderer& m_font;
  ClientManager& m_clients;
  bool m_showing = false;
};
