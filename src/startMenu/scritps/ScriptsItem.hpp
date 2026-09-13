#pragma once
#include "StartMenuItem.hpp"

#include <vector>
#include <string>

class ScriptsItem : public StartMenuItem
{
public:
  ~ScriptsItem() override;

  const char* id() const override { return "scripts"; }
  const char* label() const override { return "Scripts  >"; }

  bool onActivate(StartMenuContext& ctx,
                  int parentX, int parentY, int parentW) override;

  Window submenuWindow() const override { return m_submenu; }
  void drawSubmenu(Display* d, int screen) override;
  bool handleSubmenuClick(int y) override;
  void hideSubmenu() override;

private:
  void loadScripts();
  void openSubmenu(Display* d, int x, int y);

  Window m_submenu = None;
  StartMenuContext m_ctx{};
  std::vector<std::string> m_scripts;

  static constexpr int kRowH = 28;
  static constexpr int kWidth = 220;
};
