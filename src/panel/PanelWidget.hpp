#pragma once

#include <X11/Xlib.h>
#include <string>
#include "../Config.hpp"

/**
 * @brief Interface for a panel plugin widget.
 *
 * Add a new class under src/panel/ implementing this interface, then
 * register it from Panel. Future work can auto-register via a simple
 * static registry.
 */
class PanelWidget
{
public:
  virtual ~PanelWidget() = default;

  /** @brief Short id used in config / logs (e.g. "volume"). */
  virtual const char* id() const = 0;

  /** @brief Preferred width in pixels. */
  virtual int width() const = 0;

  /**
   * @brief Draw the widget into the panel.
   * @param display X display.
   * @param panel Panel window.
   * @param x Left edge of widget.
   * @param baseline Text baseline y.
   */
  virtual void draw(Display* display, Window panel, int x, int baseline) = 0;

  /**
   * @brief Handle a click inside the widget.
   * @return true if handled.
   */
  virtual bool onClick() = 0;

  /**
   * @brief Tooltip text when hovered, or empty.
   */
  virtual std::string tooltip() const { return {}; }

  /** @brief Pointer entered this widget; screenX is widget's left edge. */
  virtual void onHover(int screenX) { (void)screenX; }

  /** @brief Pointer left this widget. */
  virtual void onUnhover() {}

  /** @brief Return true if ESC was consumed (e.g. popup hidden). */
  virtual bool handleEscape() { return false; }

  /** @brief Popup window id, or None. */
  virtual Window popupWindow() const { return None; }

  /** @brief Redraw popup contents (Expose). */
  virtual void drawPopup() {}
  /** @brief Optional per-event-loop-tick hook. */
  virtual void tick() {}

  virtual void configure(const Config& config) { (void)config; }
};
