#!/bin/bash

Xephyr :0 -screen 1280x800 -ac &
sh -c "DISPLAY=:0 ./build/debug/mew &"
#DISPLAY=:1 wezterm start &
#startx ~/.xinitrc.mew -- :1
