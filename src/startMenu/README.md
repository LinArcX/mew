# Start Menu Modules

Each subdirectory is a start menu item. Drop a folder in `src/startMenu/`
with at least one `.cpp` and register it via `StartMenuItemRegistrar`.

## Layout
src/startMenu/
├── StartMenuItem.hpp
├── StartMenuRegistry.hpp/.cpp
├── apps/AppsItem.cpp
├── keybindings/KeybindingsItem.cpp
├── power/PowerItem.cpp
└── scripts/ScriptsItem.cpp

## Enabling items

In `~/.config/mew/config`:

    start_menu_items = apps,keybindings,power,scripts

Items appear in the given order. Unknown ids are skipped.

Default (when the key is absent): `apps,keybindings,power`.

## Adding a new item

1. `mkdir src/startMenu/myitem`
2. Create `MyItem.hpp` / `MyItem.cpp` implementing `StartMenuItem`.
3. Register at file scope:

```cpp
static StartMenuItem* createMyItem() { return new MyItem(); }
static StartMenuItemRegistrar s_myItem("myitem", createMyItem);

4. Add myitem to start_menu_items.
