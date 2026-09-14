# AGENTS.md

## Project overview

`mew` is a minimal, fast window manager for X. Plain C++23 with raw Xlib and no desktop-environment dependencies. The core principle is small, readable, hackable code — keep it that way.

## Exclusive file policy

You MUST strictly exclude every file and directory listed in the project's `.gitignore`. This applies to any file, directory, or glob pattern present in `.gitignore`.

RULES:

- NEVER read, write, edit, or reference any file or directory matched by `.gitignore` — including during searches, globbing, or grepping.
- NEVER suggest changes to files matched by `.gitignore`.
- NEVER pass `.gitignore`-matched files as context to any model or tool.
- The `.gitignore` file may itself be read and edited as needed, but the entries it lists remain off-limits.

## Build & verify

- After finishing any change, always build the project with `./scripts/build.sh` and fix every error until the build succeeds.
  - `./scripts/build.sh`          → debug build
  - `./scripts/build.sh --release` → release build
- NEVER run `mew` after building. mew is a window manager: it takes over the X session and is disruptive to launch.
- Do not hand-edit the generated `src/*_data.h` headers (fonts, images, audio) — they are produced automatically by the build from files under `assets/`.

## Code conventions

- Write safe, minimal, readable code. Simplicity is the highest priority.
- Plain C++23. Avoid external libraries; if you must use one, pick the most minimal and safe option.
- Use meaningful names for functions and variables. Remove dead code and commented-out code.
- Follow the loaded `cplusplus` and `general-programming` skills for style (2-space indent, camelCase, PascalCase types, no macros, no exceptions, etc.).

## Plugins

- Plugins live in `src/plugins/panel/*/` and `src/plugins/startMenu/*/`.
- Each plugin directory has a `compiler_flags.txt` file that controls its flags, enable/disable, extra sources, and `xxd` assets. The build system (`scripts/build.sh`) picks plugins up automatically.
- When adding or changing a plugin, keep it inside that infrastructure instead of touching the core build.

## Hands-off areas

- NEVER modify `~/.config/mew/` (files: `autostart`, `keybindings`, `config`) — those are the user's personal runtime configuration.

## Language

- Always respond in English. Never write in Chinese.