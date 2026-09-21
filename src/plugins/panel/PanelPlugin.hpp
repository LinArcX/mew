#pragma once

#include <X11/Xlib.h>
#include <string>
#include "../Config.hpp"

enum class PanelPosition { Left, Center, Right };

/**
 * @brief Interface for a panel plugin.
 *
 * Add a new class under src/panel/ implementing this interface, then
 * register it from Panel. Future work can auto-register via a simple
 * static registry.
 */
class PanelPlugin
{
public:
  virtual ~PanelPlugin() = default;

  /** @brief Short id used in config / logs (e.g. "volume"). */
  virtual const char* id() const = 0;

  /** @brief Preferred width in pixels. */
  virtual int width() const = 0;

  /**
   * @brief Draw the plugin into the panel.
   * @param display X display.
   * @param panel Panel window.
   * @param x Left edge of plugin.
   * @param baseline Text baseline y.
   */
  virtual void draw(Display* display, Window panel, int x, int baseline) = 0;

  /**
   * @brief Handle a click inside the plugin.
   * @return true if handled.
   */
  virtual bool onClick(int screenX) = 0;

  /**
   * @brief Tooltip text when hovered, or empty.
   */
  virtual std::string tooltip() const { return {}; }

  /** @brief Pointer entered this plugin; screenX is plugin's left edge. */
  virtual void onHover(int screenX) { (void)screenX; }

  /** @brief Pointer left this plugin. */
  virtual void onUnhover() {}

  /** @brief Return true if ESC was consumed (e.g. popup hidden). */
  virtual bool handleEscape() { return false; }

  /** @brief Popup window id, or None. */
  virtual Window popupWindow() const { return None; }

  /** @brief Redraw popup contents (Expose). */
  virtual void drawPopup() {}

  /** @brief Route a KeyPress to a focused popup. Return true if consumed. */
  virtual bool handlePopupKey(XKeyEvent* pEvent) { (void)pEvent; return false; }

  /** @brief Route a ButtonPress on the popup. Return true if consumed. */
  virtual bool handlePopupClick(XButtonEvent* pEvent) { (void)pEvent; return false; }

  virtual bool handlePopupMotion(XMotionEvent* pEvent) { (void)pEvent; return false; }

  /** @brief True if this plugin's popup currently has keyboard focus. */
  virtual bool hasFocusedPopup() const { return false; }

   /** @brief Called each event-loop tick. Return true if a redraw is wanted. */
  virtual bool tick() { return false; }

  virtual void configure(const Config& config) { (void)config; }

  /** @brief Plugin-local click dispatch. Return true if consumed. */
  virtual bool handleLocalClick(int localX, int screenX)
  {
    (void)localX; (void)screenX;
    return false;
  }

  /** @brief Called on ButtonRelease inside this plugin's popup. */
  virtual void handlePopupRelease(XButtonEvent* pEvent) { (void)pEvent; }

  /** @brief True to anchor the plugin to the right edge of the panel. */
  virtual bool anchorRight() const { return false; }

  PanelPosition position() const { return m_position; }

  void setPosition(PanelPosition p) { m_position = p; }

private:

  PanelPosition m_position = PanelPosition::Left;
};
