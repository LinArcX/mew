#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# ---------- arguments ----------
MODE="debug"
for arg in "$@"; do
  case "$arg" in
    --debug)   MODE="debug" ;;
    --release) MODE="release" ;;
    --clean)   MODE="clean" ;;
    -h|--help)
      echo "Usage: scripts/build.sh [--debug|--release|--clean]"
      echo "  --debug    (default) -g -pg -O0 -DDEBUG --coverage"
      echo "  --release  -O2 -DNDEBUG"
      echo "  --clean    remove generated files and build outputs"
      exit 0
      ;;
    *) echo "build.sh: unknown option: $arg" >&2; exit 1 ;;
  esac
done

# ---------- core generated headers ----------
CORE_GENERATED=(
  src/hurmit_font_data.h
  src/symbols_font_data.h
  src/logo_data.h
  src/logoFull_data.h
  src/login_wav_data.h
  src/logout_wav_data.h
)

# ---------- plugin directories ----------
shopt -s nullglob
PLUGIN_DIRS=( src/panel/*/ )
shopt -u nullglob

REAL_PLUGINS=()
for d in "${PLUGIN_DIRS[@]}"; do
  [ -d "$d" ] && REAL_PLUGINS+=("$d")
done

# ---------- plugin metadata ----------
declare -A P_LD P_PKGS P_CLEAN P_XXD P_EXTRA_SRC

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
        echo "  warning: unknown key '$key' in ${file}" >&2
        ;;
    esac
  done < "$file"
}

# =========================================================
# CLEAN MODE
# =========================================================
if [ "$MODE" = "clean" ]; then
  echo ">>> clean mode"

  for d in "${REAL_PLUGINS[@]}"; do
    f="${d}compiler_flags.txt"
    if [ ! -f "$f" ]; then
      echo "  skipping: $(basename "$d") (no compiler_flags.txt)"
      continue
    fi
    parse_plugin "$d"
  done

  echo ">>> removing core generated headers"
  for g in "${CORE_GENERATED[@]}"; do
    if [ -f "$g" ]; then
      echo "  rm $g"
      rm -f "$g"
    fi
  done

  echo ">>> removing plugin clean files"
  for d in "${REAL_PLUGINS[@]}"; do
    for rel in ${P_CLEAN[$d]:-}; do
      p="${d}${rel}"
      if [ -e "$p" ]; then
        echo "  rm $p"
        rm -f "$p"
      fi
    done
  done

  echo ">>> removing build directories"
  for out in build/debug build/release; do
    if [ -d "$out" ]; then
      echo "  rm -rf $out"
      rm -rf "$out"
    fi
  done

  echo ">>> clean done"
  exit 0
fi

# =========================================================
# BUILD MODE
# =========================================================
echo ">>> mode: $MODE"
mkdir -p "build/$MODE"

if [ "$MODE" = "debug" ]; then
  CXXFLAGS="-std=c++23 -g -pg -O0 -DDEBUG --coverage -Isrc -Isrc/panel"
  BEAR_PREFIX="bear -- "
else
  CXXFLAGS="-std=c++23 -O2 -DNDEBUG -Isrc -Isrc/panel"
  BEAR_PREFIX=""
fi

# ---------- core sources ----------
SRC=()
for f in src/*.cpp;       do [ -f "$f" ] && SRC+=("$f"); done
for f in src/panel/*.cpp; do [ -f "$f" ] && SRC+=("$f"); done

# ---------- core asset generation ----------
echo ">>> generating core asset data"
xxd -i -n hurmit_ttf   ./assets/fonts/Hermit/HurmitNerdFont-Regular.otf  > src/hurmit_font_data.h
xxd -i -n symbols_ttf  ./assets/fonts/SymbolsNerdFontMono-Regular.ttf    > src/symbols_font_data.h
xxd -i -n logo_png     ./assets/images/logo.jpg                          > src/logo_data.h
xxd -i -n logoFull_png ./assets/images/logoFull.jpg                      > src/logoFull_data.h
xxd -i -n login_wav    ./assets/audio/login.wav                          > src/login_wav_data.h
xxd -i -n logout_wav   ./assets/audio/logout.wav                         > src/logout_wav_data.h

# ---------- plugins ----------
echo ">>> scanning plugins"
LD_FLAGS="-lasound"
PKGS="x11 xft fontconfig freetype2 xcursor"

for d in "${REAL_PLUGINS[@]}"; do
  f="${d}compiler_flags.txt"
  if [ ! -f "$f" ]; then
    echo "  skipping: $(basename "$d") (no compiler_flags.txt)"
    continue
  fi
  echo "  plugin: $(basename "$d")"
  parse_plugin "$d"

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

# ---------- plugin XXD assets ----------
for d in "${REAL_PLUGINS[@]}"; do
  [ -n "${P_XXD[$d]:-}" ] || continue
  for entry in ${P_XXD[$d]}; do
    asset="${entry%%:*}"
    header="${entry#*:}"
    [ "$asset" = "$header" ] && continue
    ap="${d}${asset}"
    op="${d}${header}"
    if [ ! -f "$ap" ]; then
      echo "    warning: asset not found: $ap" >&2
      continue
    fi
    mkdir -p "$(dirname "$op")"
    echo "    xxd: $ap -> $op"
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
echo ">>> compiling"
echo "    sources  : ${#UNIQ[@]}"
echo "    packages : $PKGS"
echo "    ld_flags : $LD_FLAGS"
echo
$BEAR_PREFIX g++ $CXXFLAGS "${UNIQ[@]}" \
  $(pkg-config --cflags --libs $PKGS) \
  $LD_FLAGS \
  -o "build/$MODE/mew"
echo ">>> done: build/$MODE/mew"
