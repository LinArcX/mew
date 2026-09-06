# mew
a wm for x

# run
## xephyr
  ./start_xephyr.sh

## tty
put this line into: ~/.xinitrc.mew
  exec <PATH>/mew

Then:
  startx ~/.xinitrc.mew -- :1
