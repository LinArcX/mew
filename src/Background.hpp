#pragma once

#include "Config.hpp"
#include "XConnection.hpp"
#include <X11/Xlib.h>

/**
 * @brief Applies root window background color or stretched image wallpaper.
 */
class Background
{
public:
  /** @brief Construct with no pixmap. */
  Background();

  /** @brief Free wallpaper pixmap if any. */
  ~Background();

  /**
   * @brief Apply color or image from config to the root window.
   * @param xconn Open X connection.
   * @param config Loaded configuration.
   */
  void apply(XConnection& xconn, const Config& config);

private:
  Pixmap m_pixmap = None;
  Display* m_pDisplay = nullptr;
  Window m_root = None;
};
