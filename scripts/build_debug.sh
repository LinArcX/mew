#!/bin/bash

clear
echo ">>> creating build/debug directory"
mkdir -p build/debug

echo ">>> generating font data"
xxd -i -n hurmit_ttf ./assets/fonts/Hermit/HurmitNerdFont-Regular.otf > src/hurmit_font_data.h
xxd -i -n logo_png ./assets/images/logo.jpg > src/logo_data.h
xxd -i -n logoFull_png ./assets/images/logoFull.jpg > src/logoFull_data.h
xxd -i -n login_wav ./assets/audio/login.wav > src/login_wav_data.h
xxd -i -n logout_wav ./assets/audio/logout.wav > src/logout_wav_data.h

echo ">>> deleting old .gcda/.gcno files in build/debug directory"
find . -name "*.gcda" -delete
find . -name "*.gcno" -delete

echo ">>> compiling (debug mode)"
bear -- g++ -std=c++23 -g -pg -O0 -DDEBUG --coverage \
  src/*.cpp $(pkg-config --cflags --libs x11 xft fontconfig freetype2 xcursor) -lasound -o build/debug/mew

#-Wall -Wextra -Werror \
##-Wformat=2 -Wunused-function -Wpedantic -Wno-unused-parameter \
#-Wredundant-decls -Wmissing-include-dirs -Wlogical-op \
#-Wshadow -Wwrite-strings -Wunused-result \
# -ldl -pthread -lmagic -lm \
