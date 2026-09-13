#include "AppsItem.hpp"
#include "../StartMenuRegistry.hpp"

bool AppsItem::onActivate(StartMenuContext& ctx,
                          int parentX, int parentY, int parentW)
{
  (void)parentX; (void)parentY; (void)parentW;
  if (ctx.onShowLauncher)
  {
    ctx.onShowLauncher();
  }
  return false;   // close the parent menu
}

static StartMenuItem* createApps() { return new AppsItem(); }
static StartMenuItemRegistrar s_apps("apps", createApps);
