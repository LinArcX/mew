# mew
Minimal, fast window manager for X.

<p align="center">
  <img src="assets/images/screenshot.png" width="800">
</p>

## Features

- **Small and fast.** Plain C++, raw Xlib. No desktop-environment dependencies.
- **Keyboard-first.** Everything is a keybinding. Snap, maximize, fullscreen, launcher, power — all scriptable.
- **Batteries included, nothing bloated.** Panel, app launcher, switcher, background, sounds in one ~15k-line codebase.
- **Hackable.** Flat modules, readable code, text-file config.
- **Optional Widgets** Write your widgets and drop them in src/panel. re-compile mew, enjoy!

## dependencies
### tools
```
git
gcc
Bear
pkgconf
xxd
fzf (optioanl)
```

### build-time
```
libXft-devel
freetype-devel
libXcursor-devel
alsa-lib-devel
```

## build
First you need to clone the project:

  `git clone https://github.com/LinArcX/mew`

Then, for building it there are two possible ways:
1. using `./scripts/build_debug.sh`
```sh
chmod +x scripts/build_debug.sh
./scripts/build_debug.sh
```
2. using `./p`. Which gives you an interactive cli with more options. NOTE that fzf should be installed in this case.
```sh
chmod +x p
./p
```

Note: You should give `mew` the execute permission:

  `chmod +x ./build/debug/mew`

## run
### xephyr
  `./start_xephyr.sh`

### tty
put this line into: `~/.xinitrc.mew`

  `exec <PATH>/mew`

Then:

  `startx ~/.xinitrc.mew`

**Note**: to make life easier, you can put this line into your `~/.bashrc`:

  `alias startmew="startx ~/.xinitrc.mew"`

And simply run: `startmew`

## configure mew
configuration files reside here:
  `~/.config/mew`

1. `~/.config/mew/autostart` --> contains the tools/software/scritps that you want to run at start-up time:
```
wezterm &
```

2. `~/.config/mew/keybindings` --> you can define your custom keybindings here:
```ini
# Available modifers: W(indow, Meta), A(lt), C(ontrol), S(hift)

# Apps Shortcuts
key="W-q", command="/home/$USER/scripts/power_manager.sh"
key="W-t", command="wezterm start"
key="W-n", command="nemo"

# General Shortcuts
key="W-z", command="minimize"
key="W-x", command="maximize"
key="W-f", command="fullscreen"
key="W-c", command="center"
key="W-Left", command="snap-left"
key="W-Right", command="snap-right"
key="W-Up", command="snap-top"
key="W-Down", command="snap-bottom"

key="Print", command="/home/$USER/scripts/screenshot_fullscreen.sh"
key="W-Print", command="/home/$USER/scripts/screenshot_region.sh"
key="XF86AudioRaiseVolume", command="amixer set PCM 5%+"
key="XF86AudioLowerVolume", command="amixer set PCM 5%-"

# StartMenu Shortcuts
key="W-a" command="apps"
key="W-k" command="keybindings"
key="W-p" command="power-manager"
```

3. `~/.config/mew/config` --> contains general configurations:
```ini
title_font_size=15

background_color=#3B3C3C
background_image=~/Pictures/logo.jpg

panel_color=0x222222

mouse_size=24
mouse_theme=dmz-white

login_sound=/home/audio/startup.wav
logout_sound=/home/audio/shutdown.wav

# panel
panel_color=#222222 #524C4B  
panel_item_color=#ffffff #9FA192  #CC8B88 
panel_hover_color=#0a64c8 #6E6481  
```

## gtk customization
Gtk applications has their own way of configuration. you may need to customize:
- theme
- icon-theme
- font-name

**NOTE**: to install thems/fonts, you have different options:
- through your system package manger
- through websites like: https://www.gnome-look.org, https://fonts.google.com/, etc..

### cursor-theme
For example let's install this package with void xbps package manager:

  `sudo xbps-install -S xcursor-vanilla-dmz`

Then to set this icon-theme, you have two options:
1. by using some apps like **lxappearance**. 
2. manually editing `~/.config/gtk-3.0/settings.ini` by adding this line into it:
  
  `gtk-cursor-theme-name=Vanilla-DMZ`

### windows-theme
Let's install this package:

  `sudo xbps-install -S xcursor-vanilla-dmz`

Again, to set this theme, you have two options:
1. by using some apps like **lxappearance**. 
2. manually editing `~/.config/gtk-3.0/settings.ini` by adding this line into it:
  
  `gtk-theme-name=Yaru-dark`

### font
Let's:
- download [sofia-sans font](https://fonts.google.com/specimen/Sofia+Sans).
- extract it and put it in your `~/.fonts` directory.
- to set this font, you have two options:
  - by using some apps like **lxappearance**. 
  - manually editing `~/.config/gtk-3.0/settings.ini` by adding this line into it:
 
    `gtk-font-name=Sofia Sans Light 11`

## Limitations
- mew only supports `.wav` audio files. you can convert any format to `.wav` with ffmpeg:

  `ffmpeg -i xp_shutdown.mp3 xp_shutdown.wav`
