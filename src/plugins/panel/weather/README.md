# Weather Widget

Panel widget that shows the current weather for an auto-detected or
user-specified location. Uses Unicode-free Weather Icons PUA glyphs that are
embedded in the mew binary — no system font install required.

## Registering the widget

Add `weather` to `panel_widgets` in `~/.config/mew/config`:

    panel_widgets = weather,music,pong

## Layout

```
<icon> Berlin 24°C
```

The icon uses the embedded Weather Icons font. The text uses the embedded
Hurmit font.

## Interaction

| Action | Result |
|---|---|
| Hover | Panel hover highlight + tooltip with description |
| Click | Opens the 10-day forecast popup |
| ESC / click elsewhere | Closes the popup |

## Forecast popup

A 10-day forecast from Open-Meteo. Each row is colored by maximum temperature:

| Range | Background | Text |
|---|---|---|
| ≥ 40 °C | red | white |
| 29–39 °C | orange | black |
| 18–28 °C | green | black |
| 8–17 °C | white | black |
| 0–7 °C | light blue | black |
| −10–0 °C | blue | white |
| −30–−10 °C | purple | white |
| < −30 °C | dark purple | white |

Columns: date | icon | max° / min°.

## Location detection

By default, `wttr.in/?format=j1` auto-detects the location from the caller's
public IP. To override:

    weather_location = Berlin

Any string accepted by wttr.in works (city, `City,Country`, coordinates).

## Data sources

- `wttr.in/?format=j1` — current temperature, description, coordinates, and
  nearest-area name.
- `api.open-meteo.com` — 10-day forecast (wttr.in only provides 3 days).

Both are fetched with `curl` via a forked child and a pipe, with an 8-second
timeout. No HTTP library is linked.

## Refresh

Refreshed every 30 minutes. Clicking the widget forces an immediate refresh.

## Config

    weather_location    = Berlin
    weather_icon_color  = #ffcc44
    weather_text_color  = #ffffff

| Key | Meaning |
|---|---|
| `weather_location` | Optional. Overrides auto-detection |
| `weather_icon_color` | Color of the weather icon |
| `weather_text_color` | Color of the location/temperature text |

## Assets

- `weather_font_data.h` — embedded Weather Icons TTF. Weather Icons is
  licensed under SIL OFL 1.1.

## Notes

- If `curl` is missing, the widget stays hidden and prints nothing to the panel.
- The forecast popup's row colors are not configurable; they are tied to the
  temperature ranges by design.
- Only `weather_icon_color` and `weather_text_color` affect the panel strip,
  not the popup.

## License

Source code follows the project license. Embedded Weather Icons are under
SIL OFL 1.1.
