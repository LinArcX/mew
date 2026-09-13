#pragma once
#include "../StartMenuItem.hpp"

class PowerItem : public StartMenuItem
{
public:
  ~PowerItem() override;

  const char* id() const override { return "power"; }
  const char* label() const override { return "PowerManager  >"; }

  bool onActivate(StartMenuContext& ctx,
                  int parentX, int parentY, int parentW) override;

  Window submenuWindow() const override { return m_submenu; }
  void drawSubmenu(Display* d, int screen) override;
  bool handleSubmenuClick(int y) override;
  void hideSubmenu() override;

private:
  void openSubmenu(Display* d, int x, int y);
  void doReboot();
  void doPoweroff();

  Window m_submenu = None;
  StartMenuContext m_ctx{};

  static constexpr int kRowH = 32;
  static constexpr int kWidth = 180;
  static constexpr int kRows = 4;
};
