#pragma once

#include <string>

/**
 * @brief Rasterizes an SVG file to non-premultiplied RGBA pixel data.
 * @param path Path to the SVG file.
 * @param outW Output width in pixels.
 * @param outH Output height in pixels.
 * @return RGBA data (caller frees with free()), or nullptr on failure.
 */
unsigned char* loadSvgRgba(const std::string& path, int* outW, int* outH);
