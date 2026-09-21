#include "PanelPluginRegistry.hpp"

PanelPluginRegistry& PanelPluginRegistry::instance()
{
  static PanelPluginRegistry reg;
  return reg;
}

void PanelPluginRegistry::add(const std::string& name, PanelPluginFactory factory)
{
  if (name.empty() || !factory)
  {
    return;
  }
  m_factories[name] = factory;
}

std::vector<PanelPlugin*> PanelPluginRegistry::create(
  const std::vector<std::string>& ids,
  XConnection& xconn,
  FontRenderer& font)
{
  std::vector<PanelPlugin*> out;
  for (const std::string& id : ids)
  {
    auto it = m_factories.find(id);
    if (it == m_factories.end())
    {
      continue;
    }
    PanelPlugin* pPlugin = it->second(xconn, font);
    if (pPlugin)
    {
      out.push_back(pPlugin);
    }
  }
  return out;
}

PanelPlugin* PanelPluginRegistry::createOne(
  const std::string& id, XConnection& xconn, FontRenderer& font)
{
  auto it = m_factories.find(id);
  if (it == m_factories.end())
  {
    return nullptr;
  }
  return it->second(xconn, font);
}

PanelPluginRegistrar::PanelPluginRegistrar(const std::string& name, PanelPluginFactory factory)
{
  PanelPluginRegistry::instance().add(name, factory);
}
