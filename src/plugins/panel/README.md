# Panel plugins

Each panel plugin should implement `PanelPlugin` (see PanelPlugin.hpp).

Current plugins still live in `Panel.cpp` and will be migrated one-by-one:

- StartButton
- LanguageSwitcher
- Volume
- NetworkManager
- KillSwitch
- Clock
- DesktopToggle

# default plugins
- clock
- desktop

To add a plugin:
1. Create `src/panel/MyPlugin.hpp` + `.cpp` implementing `PanelPlugin`
2. Construct it from `Panel` and append to the plugin list
3. Add the `.cpp` to the Makefile `MODULAR_SRC` list
