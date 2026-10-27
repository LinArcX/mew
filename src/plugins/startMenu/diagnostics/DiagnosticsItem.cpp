#include "DiagnosticsItem.hpp"
#include "../StartMenuRegistry.hpp"

bool DiagnosticsItem::onActivate(StartMenuContext& ctx,
                                 int parentX, int parentY, int parentW)
{
  (void)parentX; (void)parentY; (void)parentW;
  if (ctx.onShowDiagnostics)
  {
    ctx.onShowDiagnostics();
  }
  return false;
}

static StartMenuItem* createDiagnostics() { return new DiagnosticsItem(); }
static StartMenuItemRegistrar s_diag("diagnostics", createDiagnostics);
