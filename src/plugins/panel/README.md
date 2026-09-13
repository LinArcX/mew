# Panel plugins

Each panel widget should implement `PanelWidget` (see PanelWidget.hpp).

Current widgets still live in `Panel.cpp` and will be migrated one-by-one:

- StartButton
- LanguageSwitcher
- Volume
- NetworkManager
- KillSwitch
- Clock
- DesktopToggle

To add a widget:
1. Create `src/panel/MyWidget.hpp` + `.cpp` implementing `PanelWidget`
2. Construct it from `Panel` and append to the widget list
3. Add the `.cpp` to the Makefile `MODULAR_SRC` list
