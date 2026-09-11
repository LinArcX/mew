#!/bin/bash

Xephyr :1 -screen 1280x800 -ac &
sh -c "DISPLAY=:1 ./build/debug/mew &"
#DISPLAY=:1 wezterm start &
