#pragma once

#include "PanelPlugin.hpp"
#include "../XConnection.hpp"
#include "../FontRenderer.hpp"

#include <map>
#include <string>
#include <vector>

typedef PanelPlugin* (*PanelPluginFactory)(XConnection&, FontRenderer&);

/**
 * @brief Process-wide registry of optional panel plugins.
 *
 * Plugins self-register via a file-scope PanelPluginRegistrar in their .cpp.
 */
class PanelPluginRegistry
{
public:
  static PanelPluginRegistry& instance();

  void add(const std::string& name, PanelPluginFactory factory);

  /** @brief Instantiate plugins for the given ids, in order. Skips unknown. */
  std::vector<PanelPlugin*> create(
    const std::vector<std::string>& ids,
    XConnection& xconn,
    FontRenderer& font);
  
  PanelPlugin* createOne(const std::string& id, XConnection& xconn, FontRenderer& font);

private:
  PanelPluginRegistry() = default;
  std::map<std::string, PanelPluginFactory> m_factories;
};

/**
 * @brief Helper to register a plugin at file scope.
 */
class PanelPluginRegistrar
{
public:
  PanelPluginRegistrar(const std::string& name, PanelPluginFactory factory);
};
