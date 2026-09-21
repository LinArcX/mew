#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# ---------- colors ----------
if [ -t 1 ]; then
  C_RESET=$'\033[0m'
  C_DIM=$'\033[2m'
  C_BOLD=$'\033[1m'
  C_CYAN=$'\033[36m'
  C_YELLOW=$'\033[33m'
  C_GREEN=$'\033[32m'
  C_ORANGE=$'\033[38;5;208m'
  C_LGREEN=$'\033[38;5;120m'
  C_RED=$'\033[31m'
else
  C_RESET=""; C_DIM=""; C_BOLD=""
  C_CYAN=""; C_YELLOW=""; C_GREEN=""
  C_ORANGE=""; C_LGREEN=""; C_RED=""
fi

# ---------- arguments ----------
MODE="debug"
MODE_SET=0
INSTALL=0
for arg in "$@"; do
  case "$arg" in
    --debug)   MODE="debug";   MODE_SET=1 ;;
    --release) MODE="release"; MODE_SET=1 ;;
    --clean)   MODE="clean" ;;
    --install) INSTALL=1 ;;
    -h|--help)
      echo "Usage: scripts/build.sh [--debug|--release|--clean] [--install]"
      exit 0
      ;;
    *) echo "build.sh: unknown option: $arg" >&2; exit 1 ;;
  esac
done

if [ "$MODE" = "clean" ] && [ "$INSTALL" = "1" ]; then
  echo "build.sh: --clean and --install are mutually exclusive" >&2
  exit 1
fi

if [ "$INSTALL" = "1" ] && [ "$MODE_SET" = "0" ]; then
  MODE="release"
fi

# ---------- core generated headers ----------
CORE_GENERATED=(
  src/hurmit_font_data.h
  src/symbols_font_data.h
  src/logo_data.h
  src/logoFull_data.h
  src/login_wav_data.h
  src/logout_wav_data.h
)

# ---------- plugin infrastructure (always compiled) ----------
PLUGIN_INFRA_DIRS=(
  src/plugins/panel
  src/plugins/startMenu
)

# ---------- plugin modules (only those under src/plugins/*/ ) ----------
shopt -s nullglob
MODULE_DIRS=(
  src/plugins/panel/*/
  src/plugins/startMenu/*/
)
shopt -u nullglob

REAL_MODULES=()
for d in "${MODULE_DIRS[@]}"; do
  [ -d "$d" ] && REAL_MODULES+=("$d")
done

declare -A P_LD P_PKGS P_CLEAN P_XXD P_EXTRA_SRC P_ENABLE

parse_plugin() {
  local dir="$1"
  local file="${dir}compiler_flags.txt"
  local raw line key value
  while IFS= read -r raw || [ -n "$raw" ]; do
    line="${raw%%#*}"
    line="${line%$'\r'}"
    line="$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    [ -z "$line" ] && continue
    key="${line%%=*}"
    value="${line#*=}"
    value="$(echo "$value" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    case "$key" in
      ENABLE)
        [ -n "$value" ] && P_ENABLE[$dir]="$value"
        ;;
      LD_FLAGS)
        [ -n "$value" ] && P_LD[$dir]="${P_LD[$dir]:-} $value"
        ;;
      PKGCONF_FLAG)
        [ -n "$value" ] && P_PKGS[$dir]="${P_PKGS[$dir]:-} $(echo "$value" | tr ',' ' ')"
        ;;
      FILES_TO_CLEAN)
        [ -n "$value" ] && P_CLEAN[$dir]="${P_CLEAN[$dir]:-} $(echo "$value" | tr ',' ' ')"
        ;;
      XXD)
        [ -n "$value" ] && P_XXD[$dir]="${P_XXD[$dir]:-} $value"
        ;;
      SRC)
        for s in $(echo "$value" | tr ',' ' '); do
          case "$s" in
            *.cpp) P_EXTRA_SRC[$dir]="${P_EXTRA_SRC[$dir]:-} $s" ;;
          esac
        done
        ;;
      *)
        echo "  ${C_RED}warning:${C_RESET} unknown key '$key' in ${file}" >&2
        ;;
    esac
  done < "$file"
}

# =========================================================
# CLEAN
# =========================================================
if [ "$MODE" = "clean" ]; then
  echo "${C_BOLD}${C_CYAN}>>> clean mode${C_RESET}"

  for d in "${REAL_MODULES[@]}"; do
    [ -f "${d}compiler_flags.txt" ] && parse_plugin "$d"
  done

  echo "${C_CYAN}>>> removing core generated headers${C_RESET}"
  for g in "${CORE_GENERATED[@]}"; do
    [ -f "$g" ] && { echo "  ${C_DIM}rm $g${C_RESET}"; rm -f "$g"; }
  done

  echo "${C_CYAN}>>> removing plugin clean files${C_RESET}"
  for d in "${REAL_MODULES[@]}"; do
    [ "${P_ENABLE[$d]:-true}" = "false" ] && continue
    for rel in ${P_CLEAN[$d]:-}; do
      p="${d}${rel}"
      [ -e "$p" ] && { echo "  ${C_DIM}rm $p${C_RESET}"; rm -f "$p"; }
    done
  done

  echo "${C_CYAN}>>> removing root-level artifacts${C_RESET}"
  for p in node_modules .cache; do
    [ -d "$p" ] && { echo "  ${C_DIM}rm -rf $p${C_RESET}"; rm -rf "$p"; }
  done
  for p in gmon.out package-lock.json package.json compile_commands.json; do
    [ -f "$p" ] && { echo "  ${C_DIM}rm $p${C_RESET}"; rm -f "$p"; }
  done

  echo "${C_CYAN}>>> removing build directories${C_RESET}"
  for out in build/debug build/release; do
    [ -d "$out" ] || continue
    echo "  ${C_DIM}rm -rf $out${C_RESET}"
    rm -rf "$out" 2>/dev/null || true
    if [ ! -d "$out" ]; then
      continue
    fi

    echo "  ${C_YELLOW}warning:${C_RESET} $out not fully removed:"
    ls -la "$out" >&2

    if ls "$out"/.fuse_hidden* >/dev/null 2>&1; then
      echo "  ${C_YELLOW}hint:${C_RESET} .fuse_hidden* means a running process still holds"
      echo "         a deleted file (likely a running 'mew'). Stop it and retry."
    fi

    if [ -t 0 ]; then
      read -r -p "  retry with sudo to force-remove $out? [y/N] " ans
      case "$ans" in
        y|Y)
          if sudo rm -rf "$out"; then
            [ -d "$out" ] && echo "  ${C_RED}still failed:${C_RESET} $out" \
                          || echo "  ${C_GREEN}removed${C_RESET} $out"
          else
            echo "  ${C_RED}sudo rm failed:${C_RESET} $out"
          fi
          ;;
        *)
          echo "  ${C_DIM}skipped${C_RESET}"
          ;;
      esac
    else
      echo "  ${C_DIM}non-interactive: skipping sudo prompt${C_RESET}"
    fi
  done

  echo "${C_GREEN}>>> clean done${C_RESET}"
  exit 0
fi

# =========================================================
# BUILD
# =========================================================
echo "${C_BOLD}${C_CYAN}>>> mode:${C_RESET} ${C_BOLD}$MODE${C_RESET}"
mkdir -p "build/$MODE"

GEN_DIR="build/$MODE/generated"
GEN_CORE="$GEN_DIR/core"
GEN_PLUGINS="$GEN_DIR/plugins"
mkdir -p "$GEN_CORE" "$GEN_PLUGINS"

if [ "$MODE" = "debug" ]; then
  CXXFLAGS="-std=c++23 -g -pg -O0 -DDEBUG --coverage -Isrc -Isrc/plugins -I$GEN_CORE"
  BEAR_PREFIX="bear -- "
else
  CXXFLAGS="-std=c++23 -O2 -DNDEBUG -Isrc -Isrc/plugins -I$GEN_CORE"
  BEAR_PREFIX=""
fi

# ---------- core sources ----------
SRC=()
for f in src/*.cpp; do [ -f "$f" ] && SRC+=("$f"); done

# ---------- plugin infrastructure ----------
for d in "${PLUGIN_INFRA_DIRS[@]}"; do
  for f in "${d}"/*.cpp; do
    [ -f "$f" ] && SRC+=("$f")
  done
done

# ---------- core assets ----------
echo "${C_CYAN}>>> generating core asset data${C_RESET}"
xxd -i -n hurmit_ttf   ./assets/fonts/Hermit/HurmitNerdFont-Regular.otf  > "$GEN_CORE/hurmit_font_data.h"
xxd -i -n symbols_ttf  ./assets/fonts/SymbolsNerdFontMono-Regular.ttf    > "$GEN_CORE/symbols_font_data.h"
xxd -i -n logo_png     ./assets/images/logo.jpg                          > "$GEN_CORE/logo_data.h"
xxd -i -n logoFull_png ./assets/images/logoFull.jpg                      > "$GEN_CORE/logoFull_data.h"
xxd -i -n login_wav    ./assets/audio/login.wav                          > "$GEN_CORE/login_wav_data.h"
xxd -i -n logout_wav   ./assets/audio/logout.wav                         > "$GEN_CORE/logout_wav_data.h"

# ---------- plugin modules ----------
echo "${C_CYAN}>>> scanning plugin modules${C_RESET}"
LD_FLAGS="-lasound"
PKGS="x11 xft fontconfig freetype2 xcursor"

for d in "${REAL_MODULES[@]}"; do
  f="${d}compiler_flags.txt"
  if [ ! -f "$f" ]; then
    echo "  ${C_RED}skipping:${C_RESET} ${C_DIM}$(basename "$d") (no compiler_flags.txt)${C_RESET}"
    continue
  fi

  parse_plugin "$d"

  name="$(basename "$d")"
  if [ "${P_ENABLE[$d]:-true}" = "false" ]; then
    echo "  ${C_DIM}disabled:${C_RESET} ${C_DIM}${name}${C_RESET}"
    continue
  fi

  if [[ "$d" == *"/startMenu/"* ]]; then
    echo "  ${C_LGREEN}plugin:${C_RESET} ${C_LGREEN}${name}${C_RESET}"
  else
    echo "  ${C_ORANGE}plugin:${C_RESET} ${C_ORANGE}${name}${C_RESET}"
  fi

  # Per-plugin include path for generated headers (xxd outputs).
  mkdir -p "$GEN_PLUGINS/$name"
  CXXFLAGS="$CXXFLAGS -I$GEN_PLUGINS/$name"

  for s in "${d}"*.cpp; do
    [ -f "$s" ] && SRC+=("$s")
  done

  for s in ${P_EXTRA_SRC[$d]:-}; do
    p="${d}${s}"
    [ -f "$p" ] && SRC+=("$p")
  done

  [ -n "${P_LD[$d]:-}" ]   && LD_FLAGS="$LD_FLAGS ${P_LD[$d]}"
  [ -n "${P_PKGS[$d]:-}" ] && PKGS="$PKGS ${P_PKGS[$d]}"
done

# ---------- plugin XXD ----------
for d in "${REAL_MODULES[@]}"; do
  [ "${P_ENABLE[$d]:-true}" = "false" ] && continue
  [ -n "${P_XXD[$d]:-}" ] || continue
  name="$(basename "$d")"
  for entry in ${P_XXD[$d]}; do
    asset="${entry%%:*}"
    header="${entry#*:}"
    [ "$asset" = "$header" ] && continue
    ap="${d}${asset}"
    op="$GEN_PLUGINS/$name/$header"
    if [ ! -f "$ap" ]; then
      echo "    ${C_RED}warning:${C_RESET} asset not found: ${C_DIM}$ap${C_RESET}" >&2
      continue
    fi
    mkdir -p "$(dirname "$op")"
    echo "    ${C_DIM}${C_YELLOW}xxd:${C_RESET} ${C_DIM}$ap -> $op${C_RESET}"
    ( cd "$(dirname "$ap")" && xxd -i "$(basename "$ap")" ) > "$op"
  done
done

# ---------- dedupe ----------
declare -A SEEN
UNIQ=()
for s in "${SRC[@]}"; do
  [ -z "${SEEN[$s]:-}" ] || continue
  SEEN[$s]=1
  UNIQ+=("$s")
done

# ---------- compile ----------
echo "${C_CYAN}>>> compiling${C_RESET}"
echo "    sources  : ${#UNIQ[@]}"
echo "    packages : $PKGS"
echo "    ld_flags : $LD_FLAGS"
echo
$BEAR_PREFIX g++ $CXXFLAGS "${UNIQ[@]}" \
  $(pkg-config --cflags --libs $PKGS) \
  $LD_FLAGS \
  -o "build/$MODE/mew"
echo "${C_GREEN}>>> done:${C_RESET} ${C_BOLD}build/$MODE/mew${C_RESET}"

# =========================================================
# INSTALL
# =========================================================
if [ "$INSTALL" = "1" ]; then
  BIN="build/$MODE/mew"
  TARGET="/usr/bin/mew"

  if [ ! -f "$BIN" ]; then
    echo "build.sh: $BIN not found (build first)" >&2
    exit 1
  fi

  echo "${C_CYAN}>>> installing to $TARGET${C_RESET}"
  if ! install -m 755 "$BIN" "$TARGET" 2>/dev/null; then
    echo "  ${C_DIM}requires sudo${C_RESET}"
    sudo install -m 755 "$BIN" "$TARGET"
  fi
  echo "${C_GREEN}>>> installed:${C_RESET} ${C_BOLD}$TARGET${C_RESET}"
fi
