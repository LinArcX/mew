#pragma once
#include "../StartMenuItem.hpp"

/**
 * @brief Start-menu item that opens the application launcher.
 *
 * Activates the AppLauncher popup via the context callback and closes
 * the parent start menu.
 */
class AppsItem : public StartMenuItem
{
public:
  /** @brief Start-menu item id ("apps"). */
  const char* id() const override { return "apps"; }

  /** @brief Label shown in the start menu. */
  const char* label() const override { return "Apps"; }

  /**
   * @brief Invoke the onShowLauncher callback and close the menu.
   * @param ctx     Start-menu context (unused fields tolerated).
   * @param parentX Parent menu x (unused).
   * @param parentY Parent menu y (unused).
   * @param parentW Parent menu width (unused).
   * @return false (always closes the parent menu).
   */
  bool onActivate(StartMenuContext& ctx,
                  int parentX, int parentY, int parentW) override;
};
