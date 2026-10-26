# Panel plugins

Each panel plugin implements `PanelPlugin` (see `PanelPlugin.hpp`) and
self-registers via a file-scope `PanelPluginRegistrar` in its `.cpp`.

## Layout

```
src/plugins/panel/
├── PanelPlugin.hpp              # Interface
├── PanelPluginRegistry.hpp/.cpp # Name -> factory registry
├── <name>/
│   ├── <Name>Plugin.hpp
│   ├── <Name>Plugin.cpp
│   └── compiler_flags.txt
```

## Enabling plugins

In `~/.config/mew/config`:

    panel_plugins = clock,desktop,volume

Each entry is a plugin id, optionally followed by `@left`, `@center` or
`@right` to override the default group:

    panel_plugins = clock@right,network@right,music@center

Unknown ids are silently skipped. If `panel_plugins` is absent from the
config, no panel plugins are shown (only the start button).

## compiler_flags.txt

One `key=value` per line; `#` starts a comment. Recognized keys:

| Key | Meaning |
|---|---|
| `SRC` | Additional `.cpp` files in the plugin directory to compile. |
| `PKGCONF_FLAG` | Extra pkg-config packages, comma-separated. |
| `LD_FLAGS` | Extra linker flags. |
| `XXD` | `asset:header.h` pairs to xxd-embed. |
| `FILES_TO_CLEAN` | Files to delete during `build.sh --clean`. |
| `ENABLE` | Set to `false` to skip the plugin entirely. |

`scripts/build.sh` scans `src/plugins/panel/*/` automatically; no Makefile
list is needed.

## Registered plugins

| Id | Class | Description |
|---|---|---|
| `clock` | `ClockPlugin` | Date + time. |
| `desktop` | `DesktopPlugin` | Show-desktop toggle. |
| `language` | `LanguagePlugin` | Keyboard layout indicator / cycler. |
| `music` | `MusicPlayerPlugin` | mpv-backed music player. |
| `network` | `NetworkPlugin` | Network interface picker + kill-switch. |
| `pong` | `PongPlugin` | Mini Pong game. |
| `volume` | `VolumePlugin` | ALSA volume + mute. |
| `weather` | `WeatherPlugin` | wttr.in current conditions + 10-day popup. |

## Adding a new plugin

1. `mkdir src/plugins/panel/myplugin`
2. Create `MyPlugin.hpp` / `MyPlugin.cpp` implementing `PanelPlugin`.
3. Register at file scope:

   ```cpp
   static PanelPlugin* createMyPlugin(XConnection& x, FontRenderer& f)
   {
     return new MyPlugin(x, f);
   }
   static PanelPluginRegistrar s_myPlugin("myplugin", createMyPlugin);
   ```

4. Add `src/plugins/panel/myplugin/compiler_flags.txt` with at least
   `SRC=MyPlugin.cpp`.
5. Add `myplugin` to `panel_plugins` in `~/.config/mew/config`.
