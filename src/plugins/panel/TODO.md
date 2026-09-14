- [ ] ability to define position of panel.(top, bottom, left, right)
  it should be possible to adjust the position through conf file.

- [ ] ability to change margin/padding of items in panel.
  There should be settings for it in conf file.

## plugins for panel
- [+] Pong game as panel widget.
- [ ] System tray:	Own the _NET_SYSTEM_TRAY_S0 selection; embed StatusNotifierItem icons
- [ ] Notification daemon(something like dunst): Libnotify daemon stub; show bell + count, click to open list
- [ ] CPU: Overall or per-core usage, temp if /sys/class/thermal present	(read /proc/stat)
  - [ ] when you click on it, it should shows you all cores and usage of each
- [ ] Brightness: Slider popup, reads/writes /sys/class/backlight	(sysfs)
- [ ] RAM: Used / total, swap	(read /proc/meminfo)
  - [ ] when you click on it, it should show you the 10 top apps --sorted-- that uses most ram
- [ ] Disk: Free space on a chosen mountpoint	(statvfs())
- [ ] Clipboard: Show last copied text; click to paste via xdotool / wl-paste
- [ ] Screenshot	Click → region select → maim / scrot → save or copy
- [ ] IP / geo:	Public IP from ifconfig.me, city from ipapi.co
- [ ] Moon phase: Local calculation, no network
- [ ] Pomodoro:	25/5 timer with a small progress ring
- [ ] CPU temp:	Package temp	(/sys/class/thermal/thermal_zone*/temp)
- [ ] Note scratch:	Tiny text input, saved to a file

- [ ] Battery: Percent, charging state	(/sys/class/power_supply/BAT*)
- [ ] Battery history:Sparkline of last hour drawn from sysfs samples
- [ ] Stock ticker	Same idea, a symbol from config
- [ ] Caffeine: Toggle screen-sleep inhibitor (blocks DPMS via xset)
- [ ] Cryptocurrency	BTC/ETH price from a public API; curl + JSON
- [ ] System monitor graph	CPU+RAM mini graph over 60s
