#pragma once
#include "../StartMenuItem.hpp"

#include <vector>
#include <string>

/**
 * @brief Start-menu item with a nested submenu of user scripts.
 *
 * On activation, scans ~/.config/mew/scripts/ for *.sh files and lists
 * them in a submenu. Clicking an entry launches it (fork + exec via
 * std::system with output redirected).
 */
class ScriptsItem : public StartMenuItem
{
public:
  /** @brief Destroy the submenu window. */
  ~ScriptsItem() override;

  /** @brief Start-menu item id ("scripts"). */
  const char* id() const override { return "scripts"; }

  /** @brief Label shown in the start menu (includes ">" hint). */
  const char* label() const override { return "Scripts  >"; }

  /**
   * @brief Rescan scripts and toggle the submenu open/closed.
   * @return true if a submenu was shown or hidden; false if no scripts
   *         exist (parent menu then closes normally).
   */
  bool onActivate(StartMenuContext& ctx,
                  int parentX, int parentY, int parentW) override;

  /** @brief Return the submenu window, or None. */
  Window submenuWindow() const override { return m_submenu; }

  /**
   * @brief Draw the script rows (filename without .sh).
   * @param d      X display.
   * @param screen Screen number.
   */
  void drawSubmenu(Display* d, int screen) override;

  /**
   * @brief Launch the selected script.
   * @param y Click y in submenu coordinates.
   * @return true (always consumed).
   */
  bool handleSubmenuClick(int y) override;

  /** @brief Unmap the submenu. */
  void hideSubmenu() override;

private:
  /** @brief Populate m_scripts from ~/.config/mew/scripts/*.sh. */
  void loadScripts();

  /**
   * @brief Create/reposition and map the submenu.
   * @param d X display.
   * @param x Submenu x in root coordinates.
   * @param y Submenu y in root coordinates.
   */
  void openSubmenu(Display* d, int x, int y);

  Window m_submenu = None;             ///< Submenu X window, or None.
  StartMenuContext m_ctx{};            ///< Cached context for later callbacks.
  std::vector<std::string> m_scripts;  ///< Absolute paths of discovered *.sh files.

  static constexpr int kRowH = 28;     ///< Row height in pixels.
  static constexpr int kWidth = 220;   ///< Submenu width in pixels.
};
