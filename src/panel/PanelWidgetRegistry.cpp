#include "PanelWidgetRegistry.hpp"

PanelWidgetRegistry& PanelWidgetRegistry::instance()
{
  static PanelWidgetRegistry reg;
  return reg;
}

void PanelWidgetRegistry::add(const std::string& name, PanelWidgetFactory factory)
{
  if (name.empty() || !factory)
  {
    return;
  }
  m_factories[name] = factory;
}

std::vector<PanelWidget*> PanelWidgetRegistry::create(
  const std::vector<std::string>& ids,
  XConnection& xconn,
  FontRenderer& font)
{
  std::vector<PanelWidget*> out;
  for (const std::string& id : ids)
  {
    auto it = m_factories.find(id);
    if (it == m_factories.end())
    {
      continue;
    }
    PanelWidget* pWidget = it->second(xconn, font);
    if (pWidget)
    {
      out.push_back(pWidget);
    }
  }
  return out;
}

PanelWidgetRegistrar::PanelWidgetRegistrar(const std::string& name, PanelWidgetFactory factory)
{
  PanelWidgetRegistry::instance().add(name, factory);
}
