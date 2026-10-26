#pragma once
#include "../StartMenuItem.hpp"

/**
 * @brief Start-menu item with a nested power-manager submenu.
 *
 * The submenu has four rows: Logout, Reconfigure mew, Reboot, Poweroff.
 * Clicks on Logout/Reconfigure dispatch the corresponding callbacks from
 * the start-menu context; Reboot/Poweroff try logind via dbus first, then
 * the classic /sbin binaries, then the raw reboot(2) syscall.
 */
class PowerItem : public StartMenuItem
{
public:
  /** @brief Destroy the submenu window. */
  ~PowerItem() override;

  /** @brief Start-menu item id ("power"). */
  const char* id() const override { return "power"; }

  /** @brief Label shown in the start menu (includes ">" hint). */
  const char* label() const override { return "PowerManager  >"; }

  /**
   * @brief Toggle the submenu open/closed.
   * @param ctx     Start-menu context cached for later callbacks.
   * @param parentX Parent menu x used to anchor the submenu.
   * @param parentY Parent menu y used to anchor the submenu.
   * @param parentW Parent menu width used to anchor the submenu.
   * @return true (always keeps the parent menu open).
   */
  bool onActivate(StartMenuContext& ctx,
                  int parentX, int parentY, int parentW) override;

  /** @brief Return the submenu window, or None. */
  Window submenuWindow() const override { return m_submenu; }

  /**
   * @brief Draw the submenu rows.
   * @param d      X display.
   * @param screen Screen number.
   */
  void drawSubmenu(Display* d, int screen) override;

  /**
   * @brief Dispatch a click on a submenu row.
   * @param y Click y in submenu coordinates.
   * @return true (always consumed).
   */
  bool handleSubmenuClick(int y) override;

  /** @brief Unmap the submenu. */
  void hideSubmenu() override;

private:
  /**
   * @brief Create/reposition and map the submenu.
   * @param d X display.
   * @param x Submenu x in root coordinates.
   * @param y Submenu y in root coordinates.
   */
  void openSubmenu(Display* d, int x, int y);

  /** @brief Reboot via logind dbus, then binaries, then reboot(2). */
  void doReboot();

  /** @brief Power off via logind dbus, then binaries, then reboot(2). */
  void doPoweroff();

  Window m_submenu = None;     ///< Submenu X window, or None.
  StartMenuContext m_ctx{};    ///< Cached context for later callbacks.

  static constexpr int kRowH = 32;   ///< Row height in pixels.
  static constexpr int kWidth = 180; ///< Submenu width in pixels.
  static constexpr int kRows = 4;    ///< Number of submenu rows.
};
