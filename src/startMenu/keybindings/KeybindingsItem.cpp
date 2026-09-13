#include "KeybindingsItem.hpp"
#include "../StartMenuRegistry.hpp"

bool KeybindingsItem::onActivate(StartMenuContext& ctx,
                                 int parentX, int parentY, int parentW)
{
  (void)parentX; (void)parentY; (void)parentW;
  if (ctx.onShowKeybindings)
  {
    ctx.onShowKeybindings();
  }
  return false;
}

static StartMenuItem* createKeybindings() { return new KeybindingsItem(); }
static StartMenuItemRegistrar s_kb("keybindings", createKeybindings);
