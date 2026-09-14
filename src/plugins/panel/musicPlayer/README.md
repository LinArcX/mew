# Music Player Widget

Panel widget that plays audio files from user-configured directories and
provides playback controls plus a seek slider and an animated equalizer.

## Registering the widget

Add `music` to `panel_widgets` in `~/.config/mew/config`:

    panel_widgets = weather,music,pong

## Layout

```
[♪] [⏮] [▶/⏸] [⏹] [⏭]  |||    <equalizer>
 ^   ^    ^     ^    ^
 |   |    |     |    next track
 |   |    |     stop
 |   |    play/pause toggle
 |   prev track
 opens the directories popup
```

## Controls

| Action | Result |
|---|---|
| Click ♪ | Opens the directories popup |
| Click ⏮ | Previous track |
| Click ▶ / ⏸ | Play / pause |
| Click ⏹ | Stop playback |
| Click ⏭ | Next track |
| Click equalizer | Opens the seek slider popup |
| Hover | Tooltip shows the current track name |

## Music directories popup

Clicking ♪ opens a list of directories that the widget scans for audio files.

- **Add**: type a path in the input row at the bottom and press Enter.
- **Remove**: click the red trash icon on the right of a row.
- **Close**: ESC, or click anywhere outside.

Directories are stored in `~/.config/mew/music_dirs`, one per line. If the file
does not exist, `~/Music` is used as the default when it exists.

## Seek slider popup

Clicking the equalizer opens a horizontal slider.

- Drag to preview a position.
- Release to seek.
- Close with ESC, by clicking the equalizer again, or by clicking elsewhere.

Seeking requires `mpv` (JSON IPC). Other players fall back to playback-only.

## Config

    music_note_color   = #44ccff
    music_button_color = #ffffff
    music_eq_bars      = 6
    music_eq_colors    = #44ccff,#22aa44,#ffcc44,#dd8822,#cc2222,#662299

| Key | Meaning |
|---|---|
| `music_note_color` | Color of the ♪ icon |
| `music_button_color` | Color of play/stop/next/prev icons and unused equalizer bars |
| `music_eq_bars` | Number of equalizer columns (1–16) |
| `music_eq_colors` | Per-column colors; if fewer than bars, cycles |

## Player priority

1. `mpv` (preferred — supports seeking via IPC socket)
2. `mpg123`
3. `ffplay`

`mpv` is launched with `--no-video --vo=null --force-window=no` so no window
appears. The IPC socket is created at `/tmp/mew-mpv-<pid>.sock` and removed
automatically when playback ends.

## Supported file extensions

`.mp3 .flac .ogg .wav .m4a .opus`

Directories are scanned recursively.

## Assets

None embedded. Icons come from the shared embedded Nerd Font Symbols face
(`src/nerd_symbols_data.h`).

## License

Source code follows the project license. Icons are covered by the Nerd Fonts
license (MIT).
