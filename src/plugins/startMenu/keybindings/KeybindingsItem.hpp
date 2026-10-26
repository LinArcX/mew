#pragma once
#include "../StartMenuItem.hpp"

/**
 * @brief Start-menu item that opens the keybindings window.
 */
class KeybindingsItem : public StartMenuItem
{
public:
  /** @brief Start-menu item id ("keybindings"). */
  const char* id() const override { return "keybindings"; }

  /** @brief Label shown in the start menu. */
  const char* label() const override { return "KeyBindings"; }

  /**
   * @brief Invoke the onShowKeybindings callback and close the menu.
   * @return false (always closes the parent menu).
   */
  bool onActivate(StartMenuContext& ctx,
                  int parentX, int parentY, int parentW) override;
};
