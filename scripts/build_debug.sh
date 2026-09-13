#!/bin/bash
set -e
clear

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo ">>> creating build/debug directory"
mkdir -p build/debug

# --- Base flags (unchanged from your original) ---
CXXFLAGS_BASE="-std=c++23 -g -pg -O0 -DDEBUG --coverage -Isrc -Isrc/panel"
PKGS="x11 xft fontconfig freetype2 xcursor"
LD_FLAGS="-lasound"

##-Wall -Wextra -Werror \
###-Wformat=2 -Wunused-function -Wpedantic -Wno-unused-parameter \
##-Wredundant-decls -Wmissing-include-dirs -Wlogical-op \
##-Wshadow -Wwrite-strings -Wunused-result \
## -ldl -pthread -lmagic -lm \

# --- Core asset embedding (unchanged) ---
echo ">>> generating core asset data"
xxd -i -n hurmit_ttf   ./assets/fonts/Hermit/HurmitNerdFont-Regular.otf    > src/hurmit_font_data.h
xxd -i -n weather_ttf  ./assets/fonts/weathericons-regular-webfont.ttf     > src/weather_font_data.h
xxd -i -n symbols_ttf  ./assets/fonts/SymbolsNerdFontMono-Regular.ttf      > src/symbols_font_data.h
xxd -i -n logo_png     ./assets/images/logo.jpg                            > src/logo_data.h
xxd -i -n logoFull_png ./assets/images/logoFull.jpg                        > src/logoFull_data.h
xxd -i -n login_wav    ./assets/audio/login.wav                            > src/login_wav_data.h
xxd -i -n logout_wav   ./assets/audio/logout.wav                           > src/logout_wav_data.h

# --- Collect sources ---
SRC=()
for f in src/*.cpp;       do [ -f "$f" ] && SRC+=("$f"); done
for f in src/panel/*.cpp; do [ -f "$f" ] && SRC+=("$f"); done
# Plugin subdirectories are added only if they have a compiler_flags.txt.

# --- Discover plugin flags ---
echo ">>> scanning plugins"
shopt -s nullglob
for plugin_dir in src/panel/*/; do
  plugin_name="$(basename "$plugin_dir")"
  flags_file="${plugin_dir}compiler_flags.txt"

  if [ ! -f "$flags_file" ]; then
    echo "  skipping: $plugin_name (no compiler_flags.txt)"
    continue
  fi

  echo "  plugin: $plugin_name"

  while IFS= read -r raw_line || [ -n "$raw_line" ]; do
    line="${raw_line%%#*}"
    line="${line%$'\r'}"
    line="$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    [ -z "$line" ] && continue

    key="${line%%=*}"
    value="${line#*=}"
    value="$(echo "$value" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"

    case "$key" in
      LD_FLAGS)
        [ -n "$value" ] && LD_FLAGS="$LD_FLAGS $value"
        ;;

      PKGCONF_FLAG)
        if [ -n "$value" ]; then
          # Accept comma or space separated module names.
          pkgs="$(echo "$value" | tr ',' ' ')"
          PKGS="$PKGS $pkgs"
        fi
        ;;

      SRC)
        # Accept comma or space separated paths. Only .cpp files are kept.
        items="$(echo "$value" | tr ',' ' ')"
        for s in $items; do
          case "$s" in
            *.cpp)
              path="${plugin_dir}${s}"
              [ -f "$path" ] && SRC+=("$path")
              ;;
          esac
        done
        ;;

      XXD)
        asset="${value%%:*}"
        rest="${value#*:}"
        header="$rest"
        if [ -z "$asset" ] || [ -z "$header" ] || [ "$asset" = "$header" ]; then
          echo "    warning: invalid XXD entry: $value" >&2
          continue
        fi
        asset_path="${plugin_dir}${asset}"
        out_path="${plugin_dir}${header}"
        if [ ! -f "$asset_path" ]; then
          echo "    warning: asset not found: $asset_path" >&2
          continue
        fi
        mkdir -p "$(dirname "$out_path")"
        echo "    xxd: $asset_path -> $out_path"
        ( cd "$(dirname "$asset_path")" && xxd -i "$(basename "$asset_path")" ) > "$out_path"
        ;;

      *)
        echo "    warning: unknown key '${key}' in ${flags_file}" >&2
        ;;
    esac
  done < "$flags_file"
done
shopt -u nullglob

# --- Deduplicate sources (a plugin may list its own top-level .cpp in SRC) ---
declare -A SEEN
UNIQ_SRC=()
for s in "${SRC[@]}"; do
  if [ -z "${SEEN[$s]:-}" ]; then
    SEEN[$s]=1
    UNIQ_SRC+=("$s")
  fi
done

echo ">>> deleting old .gcda/.gcno files in build/debug directory"
find . -name "*.gcda" -delete
find . -name "*.gcno" -delete

echo ">>> compiling (debug mode)"
echo "    sources  : ${#UNIQ_SRC[@]}"
echo "    packages : $PKGS"
echo "    ld_flags : $LD_FLAGS"
echo

bear -- g++ $CXXFLAGS_BASE "${UNIQ_SRC[@]}" \
  $(pkg-config --cflags --libs $PKGS) \
  $LD_FLAGS \
  -o build/debug/mew

echo
echo ">>> done: build/debug/mew"
