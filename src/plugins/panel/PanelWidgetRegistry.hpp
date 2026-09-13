#pragma once

#include "PanelWidget.hpp"
#include "../XConnection.hpp"
#include "../FontRenderer.hpp"

#include <map>
#include <string>
#include <vector>

typedef PanelWidget* (*PanelWidgetFactory)(XConnection&, FontRenderer&);

/**
 * @brief Process-wide registry of optional panel widgets.
 *
 * Widgets self-register via a file-scope PanelWidgetRegistrar in their .cpp.
 */
class PanelWidgetRegistry
{
public:
  static PanelWidgetRegistry& instance();

  void add(const std::string& name, PanelWidgetFactory factory);

  /** @brief Instantiate widgets for the given ids, in order. Skips unknown. */
  std::vector<PanelWidget*> create(
    const std::vector<std::string>& ids,
    XConnection& xconn,
    FontRenderer& font);

private:
  PanelWidgetRegistry() = default;
  std::map<std::string, PanelWidgetFactory> m_factories;
};

/**
 * @brief Helper to register a widget at file scope.
 */
class PanelWidgetRegistrar
{
public:
  PanelWidgetRegistrar(const std::string& name, PanelWidgetFactory factory);
};
