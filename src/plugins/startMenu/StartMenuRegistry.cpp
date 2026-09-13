#include "StartMenuRegistry.hpp"

StartMenuRegistry& StartMenuRegistry::instance()
{
  static StartMenuRegistry reg;
  return reg;
}

void StartMenuRegistry::add(const std::string& name, StartMenuItemFactory factory)
{
  if (name.empty() || !factory)
  {
    return;
  }
  m_factories[name] = factory;
}

std::vector<StartMenuItem*> StartMenuRegistry::create(const std::vector<std::string>& ids)
{
  std::vector<StartMenuItem*> out;
  for (const std::string& id : ids)
  {
    auto it = m_factories.find(id);
    if (it == m_factories.end())
    {
      continue;
    }
    StartMenuItem* pItem = it->second();
    if (pItem)
    {
      out.push_back(pItem);
    }
  }
  return out;
}

StartMenuItemRegistrar::StartMenuItemRegistrar(const std::string& name, StartMenuItemFactory factory)
{
  StartMenuRegistry::instance().add(name, factory);
}
