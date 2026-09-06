#!/bin/sh

Xephyr :1 -screen 1280x800 -ac &
DISPLAY=:1 /mnt/D/workspace/c++/active/mew/build/debug/mew
DISPLAY=:1 wezterm &
