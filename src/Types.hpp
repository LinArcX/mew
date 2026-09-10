#pragma once

#include <X11/Xlib.h>
#include <string>
#include <vector>

struct Client
{
  Window window = None;
  Window frame = None;

  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  int oldX = 0;
  int oldY = 0;
  int oldWidth = 0;
  int oldHeight = 0;

  bool maximized = false;
  bool minimized = false;
  bool fullscreen = false;
  bool transient = false;   // dialog / transient window
  bool noMaximize = false;  // disable maximize button
  bool csd = false;  // client-side decorations (GTK)

  // Expected UnmapNotify count to ignore (reparent/fullscreen transitions).
  int ignoreUnmap = 0;

  Time lastTitleClick = 0;
};

struct KeyBinding
{
  KeyCode keycode = 0;
  unsigned int modifiers = 0;
  std::string command;
  std::string display;
};

struct DesktopApp
{
  std::string name;
  std::string exec;
  std::string icon;   // Icon= from .desktop (name or path)
  int useCount = 0;   // launch frequency
};

enum class ResizeDirection
{
  NoEdge,
  Left,
  Right,
  Top,
  Bottom,
  TopLeft,
  TopRight,
  BottomLeft,
  BottomRight
};

namespace MewConst
{
  constexpr int titleHeight = 28;
  constexpr int borderWidth = 6;
  constexpr int resizeBorder = 6;
  constexpr int buttonWidth = 30;
  constexpr int minWidth = 120;
  constexpr int minHeight = 60;
  constexpr int panelHeight = 28;

  constexpr unsigned long colorBorder = 0x333333;
  constexpr unsigned long colorTitle = 0x444444;
  constexpr unsigned long colorButton = 0x555555;
  constexpr unsigned long colorText = 0xffffff;
  constexpr unsigned long colorSwitcherBg = 0x1e1e1e;
  constexpr unsigned long colorSwitcherBorder = 0x555555;
  constexpr unsigned long colorSwitcherHl = 0x0a64c8;
  constexpr unsigned long colorSwitcherText = 0xffffff;
}
