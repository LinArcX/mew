#pragma once

#include "FontRenderer.hpp"
#include "XConnection.hpp"

#include <X11/Xlib.h>
#include <string>
#include <vector>

/**
 * @brief Centered power-action popup with a search input at top.
 * Similar to AppLauncher but with a fixed list of actions.
 */
class PowerWindow
{
public:
  PowerWindow(XConnection& xconn, FontRenderer& font);
  ~PowerWindow();

  void show();
  void hide();
  void draw();
  void handleKey(XKeyEvent* pEvent);
  void handleClick(XButtonEvent* pEvent);

  bool isActive() const { return m_active; }
  Window window() const { return m_window; }

  void setOnReconfigure(void (*fn)()) { m_onReconfigure = fn; }
  void setOnQuit(void (*fn)()) { m_onQuit = fn; }

private:
  void filter();
  void ensureVisible();
  void execute(int itemIndex);
  int windowHeight() const;

  XConnection& m_xconn;
  FontRenderer& m_font;

  Window m_window = None;
  bool m_active = false;
  std::string m_query;
  size_t m_index = 0;
  size_t m_scroll = 0;
  std::vector<int> m_filtered;

  void (*m_onReconfigure)() = nullptr;
  void (*m_onQuit)() = nullptr;

  static constexpr int kWidth = 360;
  static constexpr int kLineH = 28;
  static constexpr int kPad = 10;
  static constexpr int kMaxVisible = 6;
};
