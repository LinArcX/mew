#pragma once

#include "StartMenuItem.hpp"

#include <map>
#include <string>
#include <vector>

typedef StartMenuItem* (*StartMenuItemFactory)();

class StartMenuRegistry
{
public:
  static StartMenuRegistry& instance();
  void add(const std::string& name, StartMenuItemFactory factory);
  std::vector<StartMenuItem*> create(const std::vector<std::string>& ids);

private:
  StartMenuRegistry() = default;
  std::map<std::string, StartMenuItemFactory> m_factories;
};

class StartMenuItemRegistrar
{
public:
  StartMenuItemRegistrar(const std::string& name, StartMenuItemFactory factory);
};
