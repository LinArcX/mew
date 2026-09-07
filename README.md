# mew
a wm for x

# dependencies
## libs
  libXft-devel
  freetype-devel
## runtime
  mpg123

# run
## xephyr
  ./start_xephyr.sh

## tty
put this line into: ~/.xinitrc.mew
  exec <PATH>/mew

Then:
  startx ~/.xinitrc.mew -- :1

## configure mew
configuration files reside here:
  ~/.config/mew

1. ~/.config/mew/autostart: the things you want to start automatically when mew starts.
  # terminal
  wezterm &

  # startup background
  feh --no-fehbg --bg-fill '/home/@USER/pictures/spacecraft.jpg' &

  # startup music
  mpg123 -f 3000 /home/$USER/assets/startup.mp3 > /dev/null 2>&1 &

2. ~/.config/mew/keybindings: all keybindings in your mew setup.
  # available modifers: W(indow, Meta), A(lt), C(ontrol), S(hift)
  key="W-q", command="/home/@USER/scripts/power_manager.sh"
  key="A-t", command="wezterm start"
  key="A-n", command="nemo"
  key="A-e", command="rofi -show drun -show-icons -icon-theme Yaru -font "Cascadia Code 13""
  key="Print", command="/home/@USER/scripts/screenshot_fullscreen.sh"
  key="W-Print", command="/home/@USER/scripts/screenshot_region.sh"
  key="XF86AudioRaiseVolume", command="amixer set PCM 5%+" # pi5: amixer -D default set Master 5%+
  key="XF86AudioLowerVolume", command="amixer set PCM 5%-" # pi5: amixer -D default set Master 5%-
  key="XF86AudioMute", command="amixer -D pulse set Master toggle" # amixer -D pulse set Master toggle
