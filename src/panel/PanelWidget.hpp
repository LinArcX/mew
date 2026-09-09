#pragma once

#include <X11/Xlib.h>
#include <string>

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
};
