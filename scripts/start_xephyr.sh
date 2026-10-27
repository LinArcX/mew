#!/bin/bash

Xephyr :0 -screen 1280x800 -ac &
sh -c "DISPLAY=:0 ./build/debug/mew &"
DISPLAY=:0 ./build/debug/mew 2>&1 | tee /tmp/mew.log
#DISPLAY=:1 wezterm start &
#startx ~/.xinitrc.mew -- :1
