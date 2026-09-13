#pragma once
#include "../../../src/plugins/startMenu/StartMenuItem.hpp"

class AppsItem : public StartMenuItem
{
public:
  const char* id() const override { return "apps"; }
  const char* label() const override { return "Apps"; }
  bool onActivate(StartMenuContext& ctx,
                  int parentX, int parentY, int parentW) override;
};
