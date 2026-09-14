#include "SvgLoader.hpp"

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg.h"
#include "nanosvgrast.h"

#include <cstdlib>

unsigned char* loadSvgRgba(const std::string& path, int* outW, int* outH)
{
  if (path.empty() || !outW || !outH)
  {
    return nullptr;
  }

  NSVGimage* pImage = nsvgParseFromFile(path.c_str(), "px", 96);
  if (!pImage)
  {
    return nullptr;
  }

  int w = static_cast<int>(pImage->width);
  int h = static_cast<int>(pImage->height);
  if (w <= 0 || h <= 0)
  {
    nsvgDelete(pImage);
    return nullptr;
  }

  unsigned char* pData = static_cast<unsigned char*>(std::malloc(static_cast<size_t>(w) * h * 4));
  if (!pData)
  {
    nsvgDelete(pImage);
    return nullptr;
  }

  NSVGrasterizer* pRast = nsvgCreateRasterizer();
  if (!pRast)
  {
    std::free(pData);
    nsvgDelete(pImage);
    return nullptr;
  }

  nsvgRasterize(pRast, pImage, 0, 0, 1.0f, pData, w, h, w * 4);

  nsvgDeleteRasterizer(pRast);
  nsvgDelete(pImage);

  *outW = w;
  *outH = h;
  return pData;
}
