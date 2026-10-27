#pragma once
#include "../StartMenuItem.hpp"

/**
 * @brief Start-menu item that opens the diagnostics window.
 */
class DiagnosticsItem : public StartMenuItem
{
public:
  /** @brief Start-menu item id ("diagnostics"). */
  const char* id() const override { return "diagnostics"; }

  /** @brief Label shown in the start menu. */
  const char* label() const override { return "Diagnostics"; }

  /**
   * @brief Invoke the onShowDiagnostics callback and close the menu.
   * @return false (always closes the parent menu).
   */
  bool onActivate(StartMenuContext& ctx,
                  int parentX, int parentY, int parentW) override;
};
