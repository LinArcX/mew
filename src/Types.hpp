#pragma once

#include <X11/Xlib.h>
#include <string>
#include <vector>

/**
 * @brief Bookkeeping for one managed client window.
 *
 * Stores the frame id, current geometry, cached "old" geometry for
 * maximize/restore, state flags, and misc bookkeeping such as the
 * expected UnmapNotify count and last title click time.
 */
struct Client
{
  Window window = None;      ///< Client top-level X window.
  Window frame = None;       ///< WM-owned frame wrapping the client.

  int x = 0;                 ///< Frame x in root coordinates.
  int y = 0;                 ///< Frame y in root coordinates.
  int width = 0;             ///< Client width in pixels.
  int height = 0;            ///< Client height in pixels.

  int oldX = 0;              ///< Saved x for maximize/restore.
  int oldY = 0;              ///< Saved y for maximize/restore.
  int oldWidth = 0;          ///< Saved width for maximize/restore.
  int oldHeight = 0;         ///< Saved height for maximize/restore.

  bool maximized = false;    ///< True while maximized.
  bool minimized = false;    ///< True while minimized (unmapped).
  bool fullscreen = false;   ///< True while fullscreen.
  bool transient = false;    ///< True for dialog / transient windows.
  bool noMaximize = false;   ///< Disable the maximize button.
  bool csd = false;          ///< Client provides its own decorations (GTK).

  int ignoreUnmap = 0;       ///< UnmapNotify count to ignore (reparent/fullscreen).

  Time lastTitleClick = 0;   ///< Timestamp of previous titlebar click.
};

/**
 * @brief One parsed keybinding entry from ~/.config/mew/keybindings.
 */
struct KeyBinding
{
  KeyCode keycode = 0;          ///< X keycode resolved from the keysym.
  unsigned int modifiers = 0;   ///< Modifier mask (Mod4, Control, Shift, ...).
  std::string command;          ///< Action name or shell command.
  std::string display;          ///< Human-readable form for the keybindings window.
};

/**
 * @brief One application entry parsed from a .desktop file.
 */
struct DesktopApp
{
  std::string name;         ///< Display name (Name=).
  std::string exec;         ///< Exec command line, %-substitutions stripped.
  std::string icon;         ///< Icon= field (name or absolute path).
  int useCount = 0;         ///< Launch frequency, drives the sort order.
};

/**
 * @brief Which edge or corner of a frame the pointer is over.
 */
enum class ResizeDirection
{
  NoEdge,      ///< Not on any edge.
  Left,        ///< Left edge.
  Right,       ///< Right edge.
  Top,         ///< Top edge.
  Bottom,      ///< Bottom edge.
  TopLeft,     ///< Top-left corner.
  TopRight,    ///< Top-right corner.
  BottomLeft,  ///< Bottom-left corner.
  BottomRight  ///< Bottom-right corner.
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
