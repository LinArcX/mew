#pragma once
#include "panel/PanelPlugin.hpp"
#include "XConnection.hpp"
#include "FontRenderer.hpp"
#include "Config.hpp"
#include "PluginProcess.hpp"
#include <X11/Xlib.h>
#include <string>

/**
 * @brief Panel plugin showing the ALSA volume and a slider popup.
 *
 * The control name ("Master") and device are inferred from the first
 * amixer entry in the user's keybindings file. Clicking the label opens
 * a vertical slider with a mute button.
 */
class VolumePlugin : public PanelPlugin
{
public:
  /**
   * @brief Construct a volume widget.
   * @param xconn Open X connection.
   * @param font  Font renderer for the label.
   */
  VolumePlugin(XConnection& xconn, FontRenderer& font)
    : m_xconn(xconn), m_font(font) {}

  /** @brief Destroy the popup window if created. */
  ~VolumePlugin() override;

  /** @brief Plugin id used in config ("volume"). */
  const char* id() const override { return "volume"; }

  /** @brief Pixel width sized to fit the current volume string. */
  int width() const override;

  /** @brief Anchor to the right group of the panel. */
  bool anchorRight() const override { return true; }

  /**
   * @brief Draw the volume percentage or mute glyph.
   * @param d        X display.
   * @param panel    Panel pixmap/window.
   * @param x        Left edge of the plugin.
   * @param baseline Text baseline y.
   */
  void draw(Display* d, Window panel, int x, int baseline) override;

  /** @brief Screen-coordinate click handler is unused. */
  bool onClick(int) override { return false; }

  /**
   * @brief Open or close the slider popup.
   * @param localX  Local x (unused).
   * @param screenX Absolute screen x.
   * @return true (always consumes the click).
   */
  bool handleLocalClick(int localX, int screenX) override;

  /** @brief Tooltip text. */
  std::string tooltip() const override;

  /**
   * @brief Stash the config pointer for control-name resolution.
   * @param config Loaded configuration.
   */
  void configure(const Config& config) override { m_pConfig = &config; }

  /** @brief Poll amixer every few seconds for the current level. */
  bool tick() override;

  /** @brief Close the popup on ESC. */
  bool handleEscape() override;

  /** @brief Return the slider popup window, or None. */
  Window popupWindow() const override { return m_popup; }

  /** @brief Redraw the slider popup. */
  void drawPopup() override;

  /**
   * @brief Handle mute button and slider clicks.
   * @param pEvent Button event.
   * @return true if consumed.
   */
  bool handlePopupClick(XButtonEvent* pEvent) override;

  /**
   * @brief Drag the slider while the mouse button is held.
   * @param pEvent Motion event.
   * @return true if consumed.
   */
  bool handlePopupMotion(XMotionEvent* pEvent) override;

  /** @brief True while the slider popup has keyboard focus. */
  bool hasFocusedPopup() const override { return m_popupActive; }

  /** @brief Optional out-of-process backend for the plugin. */
  PluginProcess* process() const override { return const_cast<PluginProcess*>(&m_process); }

private:
  /** @brief Parse the amixer output into m_percent and m_muted. */
  void parseOutput(const std::string& out);

  /** @brief Query amixer for the current level and mute state. */
  void refresh();

  /**
   * @brief Set the volume through amixer.
   * @param pct Percent 0..100.
   */
  void setPercent(int pct);

  /** @brief Toggle mute through amixer. */
  void toggleMute();

  /**
   * @brief Map the slider popup anchored to the plugin.
   * @param screenX Absolute screen x of the plugin.
   */
  void showPopup(int screenX);

  /** @brief Unmap the slider popup and release the grab. */
  void hidePopup();

  /**
   * @brief Derive the amixer -D device and control name from keybindings.
   * @param device  Output: -D device (may be empty).
   * @param control Output: control name (defaults to "Master").
   */
  void resolveControl(std::string& device, std::string& control) const;

  XConnection& m_xconn;      ///< Open X connection.
  FontRenderer& m_font;      ///< Font renderer.
  const Config* m_pConfig = nullptr;  ///< Config (not owned), used for control resolution.

  int m_percent = 0;         ///< Current volume level, 0..100.
  bool m_muted = false;      ///< True when amixer reports mute.
  bool m_valid = false;      ///< True once a successful amixer read happened.

  Window m_popup = None;     ///< Slider popup window.
  bool m_popupActive = false;///< True while the popup is mapped.
  bool m_dragging = false;   ///< True while the user drags the slider.
  time_t m_lastRefresh = 0;  ///< Timestamp of the last amixer query.
  PluginProcess m_process;
  bool m_queryPending = false;

  static constexpr int kPopupW = 60;   ///< Slider popup width.
  static constexpr int kPopupH = 230;  ///< Slider popup height.
};
