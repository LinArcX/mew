#pragma once

#include "FontRenderer.hpp"
#include "Types.hpp"
#include "XConnection.hpp"

#include <X11/Xlib.h>
#include <string>
#include <vector>

/**
 * @brief Rofi-style application launcher.
 *
 * Scans .desktop files, provides type-to-filter search, keyboard navigation,
 * and launches the selected Exec command.
 */
class AppLauncher
{
public:
  /**
   * @brief Construct a launcher bound to X and font.
   * @param xconn Open X connection.
   * @param font Font renderer for UI text.
   */
  AppLauncher(XConnection& xconn, FontRenderer& font);

  /**
   * @brief Destroy the launcher window if created.
   */
  ~AppLauncher();

  /**
   * @brief Scan apps if needed, map centered popup, grab keyboard.
   */
  void show();

  /**
   * @brief Unmap popup and release keyboard grab.
   */
  void hide();

  /**
   * @brief Redraw search box and filtered results.
   */
  void draw();

  /**
   * @brief Handle KeyPress while launcher is active.
   * @param pEvent X key event.
   */
  void handleKey(XKeyEvent* pEvent);

  /**
   * @brief Handle ButtonPress on the launcher (click a result).
   * @param pEvent X button event.
   */
  void handleClick(XButtonEvent* pEvent);

  /**
   * @brief Whether the launcher is mapped.
   */
  bool isActive() const { return m_active; }

  /**
   * @brief X window id, or None.
   */
  Window window() const { return m_window; }

private:
  /** @brief Parse Name= / Exec= from desktop file body. */
  std::string desktopField(const std::string& content, const std::string& key);

  /** @brief Scan one applications directory for .desktop files. */
  void scanDir(const std::string& dir);

  /** @brief Scan standard application directories and sort by name. */
  void scanApps();

  /** @brief Rebuild m_filtered from m_query. */
  void filter();

  /** @brief Run Exec of the highlighted entry and hide. */
  void launchSelected();

  XConnection& m_xconn;
  FontRenderer& m_font;

  Window m_window = None;
  bool m_active = false;
  std::string m_query;
  size_t m_index = 0;
  std::vector<DesktopApp> m_apps;
  std::vector<int> m_filtered;
  size_t m_scroll = 0; // first visible index into m_filtered

  /** @brief Ensure m_index stays in view by adjusting m_scroll. */
  void ensureVisible();

  /** @brief Fixed window height for kMaxVisible rows. */
  int windowHeight() const;

  static constexpr int kWidth = 480;
  static constexpr int kLineH = 28;
  static constexpr int kPad = 10;
  static constexpr int kMaxVisible = 12;
};
