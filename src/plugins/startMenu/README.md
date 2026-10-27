# Start Menu Modules

Each subdirectory is a start menu item. Drop a folder in
`src/plugins/startMenu/` with at least one `.cpp` and register it via
`StartMenuItemRegistrar`.

## Layout

```
src/plugins/startMenu/
├── StartMenuItem.hpp
├── StartMenuRegistry.hpp/.cpp
├── apps/AppsItem.cpp
├── diagnostics/DiagnosticsItem.cpp
├── keybindings/KeybindingsItem.cpp
├── power/PowerItem.cpp
└── scripts/ScriptsItem.cpp
```

## Enabling items

In `~/.config/mew/config`:

    start_menu_items = apps,keybindings,power,scripts

Items appear in the given order. Unknown ids are skipped.

If the key is absent, the default is `apps,keybindings,power`.

## Adding a new item

1. `mkdir src/plugins/startMenu/myitem`
2. Create `MyItem.hpp` / `MyItem.cpp` implementing `StartMenuItem`.
3. Register at file scope:

   ```cpp
   static StartMenuItem* createMyItem() { return new MyItem(); }
   static StartMenuItemRegistrar s_myItem("myitem", createMyItem);
   ```

4. Add a `compiler_flags.txt` with `SRC=MyItem.cpp`.
5. Add `myitem` to `start_menu_items`.
