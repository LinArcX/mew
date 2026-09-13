#pragma once
#include "../StartMenuItem.hpp"

class KeybindingsItem : public StartMenuItem
{
public:
  const char* id() const override { return "keybindings"; }
  const char* label() const override { return "KeyBindings"; }
  bool onActivate(StartMenuContext& ctx,
                  int parentX, int parentY, int parentW) override;
};
