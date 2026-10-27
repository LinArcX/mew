#pragma once
#include "panel/PanelPlugin.hpp"
#include "XConnection.hpp"
#include "FontRenderer.hpp"
#include "PluginProcess.hpp"
#include <X11/Xlib.h>
#include <string>
#include <vector>

/**
 * @brief Panel plugin showing the active network interface and a
 *        kill-switch.
 *
 * Displays the selected interface with an up/down indicator. Left-click
 * on the indicator toggles the interface up/down (kill-switch). Left-click
 * on the label opens a popup listing every non-loopback interface so the
 * user can pick the "primary" one.
 */
class NetworkPlugin : public PanelPlugin
{
public:
  /**
   * @brief Construct a network plugin.
   * @param xconn Open X connection.
   * @param font  Font renderer for the label.
   */
  NetworkPlugin(XConnection& xconn, FontRenderer& font)
    : m_xconn(xconn), m_font(font) {}

  /** @brief Destroy the popup window if created. */
  ~NetworkPlugin() override;

  /** @brief Plugin id used in config ("network"). */
  const char* id() const override { return "network"; }

  /** @brief Pixel width sized to fit the interface label. */
  int width() const override;

  /** @brief Anchor to the right group of the panel. */
  bool anchorRight() const override { return true; }

  /**
   * @brief Draw the up/down indicator and interface label.
   * @param d        X display.
   * @param panel    Panel pixmap/window.
   * @param x        Left edge of the plugin.
   * @param baseline Text baseline y.
   */
  void draw(Display* d, Window panel, int x, int baseline) override;

  /** @brief Screen-coordinate click handler is unused. */
  bool onClick(int) override { return false; }

  /**
   * @brief Dispatch a click: indicator = kill-switch, label = picker popup.
   * @param localX  Local x inside the plugin.
   * @param screenX Absolute screen x.
   * @return true (always consumes the click).
   */
  bool handleLocalClick(int, int screenX) override;

  /** @brief Tooltip text. */
  std::string tooltip() const override { return "Network interface"; }

  /** @brief Re-scan interfaces every few seconds. */
  bool tick() override;

  /** @brief Close the picker popup on ESC. */
  bool handleEscape() override;

  /** @brief Return the picker popup window, or None. */
  Window popupWindow() const override { return m_popup; }

  /** @brief Redraw the picker popup. */
  void drawPopup() override;

  /**
   * @brief Select an interface from the popup.
   * @param e Button event positioned over a row.
   * @return true if consumed.
   */
  bool handlePopupClick(XButtonEvent* e) override;

  /** @brief True while the picker popup has keyboard focus. */
  bool hasFocusedPopup() const override { return m_popupActive; }

  /** @brief Optional out-of-process backend for the plugin. */
  PluginProcess* process() const override { return const_cast<PluginProcess*>(&m_process); }

private:
  /** @brief Re-enumerate /sys/class/net and pick a default interface. */
  void refresh();

  /**
   * @brief True if /sys/class/net/<name>/operstate is "up".
   * @param name Interface name.
   */
  bool isUp(const std::string& name) const;

  /** @brief Toggle the selected interface up/down (needs privilege escalation). */
  void killSwitch();

  /**
   * @brief Map the picker popup anchored to the plugin.
   * @param screenX Absolute screen x of the plugin.
   */
  void showPopup(int screenX);

  /** @brief Unmap the picker popup and release the grab. */
  void hidePopup();

  XConnection& m_xconn;                ///< Open X connection.
  FontRenderer& m_font;                ///< Font renderer.

  std::vector<std::string> m_ifaces;   ///< Enumerated interfaces (sans lo).
  std::string m_selected;              ///< Currently selected interface.
  PluginProcess m_process;
  Window m_popup = None;               ///< Picker popup window.
  bool m_popupActive = false;          ///< True while the picker is mapped.
  time_t m_lastRefresh = 0;            ///< Last interface rescan time.

  static constexpr int kPopupW = 200;  ///< Picker popup width.
  static constexpr int kRowH = 28;     ///< Picker popup row height.
};
