# Pong Widget

Minimal Pong game embedded in the panel. Player controls the left paddle with
the keyboard; the AI plays on the right.

## Registering the widget

Add `pong` to `panel_widgets` in `~/.config/mew/config`:

    `panel_widgets = weather,music,pong`

The widget occupies one fifth of the total screen width and the full panel
height.

## Layout

```
[▶/⏸] |  field                            | [☰]
 ^         (player | AI)                    ^
 play/pause button                          settings button
```

The button section and the field are drawn as separate regions but live inside
the same widget window.

## Controls

| Action | Result |
|---|---|
| Click play button (idle) | Starts the game, grabs the keyboard |
| Click pause button (playing) | Pauses and releases the keyboard |
| Click play button (paused) | Resumes and re-grabs the keyboard |
| Click settings (☰) | Opens the difficulty popup |
| ESC while playing | Stops the game and releases the keyboard |
| Up / W | Move the player paddle up |
| Down / S | Move the player paddle down |

While the game is active, the keyboard is grabbed by mew. No key press reaches
other windows or the WM until you pause, stop, or close the widget.

## Difficulty popup

Clicking the settings button opens a small popup with three options:

- Easy — ball 45 px/s, AI 30 px/s
- Medium — ball 70 px/s, AI 60 px/s
- Hard — ball 100 px/s, AI 110 px/s

The selected option is stored in `~/.config/mew/pong_difficulty`.

## Physics

- Sub-stepped at 4 ms intervals so collisions are pixel-accurate.
- Ball reverses exactly on the paddle surface (no visible gap).
- Player hits can add vertical spin based on where the ball lands.
- First to whatever — there is no win condition; scores accumulate until stopped.

## Config

No dedicated keys. Difficulty is set from the popup and persisted.

## Assets

None embedded. Pure X11 drawing primitives (rectangles and polygons).

## License

Source code follows the project license.
