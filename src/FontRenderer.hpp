#pragma once

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>

/**
 * @brief Renders UTF-8 text with the embedded high-quality font via Xft.
 */
class FontRenderer
{
public:
  /** @brief Construct an unloaded renderer. */
  FontRenderer();

  /** @brief Release Xft font and FreeType resources. */
  ~FontRenderer();

  /**
   * @brief Load the embedded font at the given pixel size.
   * @param pDisplay Open Display.
   * @param screen Screen number.
   * @param pixelSize Font size in pixels.
   */
  void load(Display* pDisplay, int screen, double pixelSize);

  /**
   * @brief Draw UTF-8 text at baseline (x, y).
   * @param pDisplay Open Display.
   * @param screen Screen number.
   * @param window Drawable window.
   * @param x Baseline x.
   * @param y Baseline y.
   * @param text UTF-8 string.
   */
  void draw(Display* pDisplay, int screen, Window window, int x, int y, const std::string& text);

  /** @brief Underlying XftFont, or nullptr if load failed. */
  XftFont* font() const { return m_pFont; }

private:
  FT_Library m_ftLibrary = nullptr;
  FT_Face m_ftFace = nullptr;
  XftFont* m_pFont = nullptr;
  XftColor m_textColor{};
  bool m_colorReady = false;
  Display* m_pDisplay = nullptr;
  int m_screen = 0;
};
