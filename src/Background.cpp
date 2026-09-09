#include "Background.hpp"

#include <X11/Xutil.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

Background::Background()
{
}

Background::~Background()
{
  if (m_pixmap != None && m_pDisplay)
  {
    XFreePixmap(m_pDisplay, m_pixmap);
    m_pixmap = None;
  }
}

void Background::apply(XConnection& xconn, const Config& config)
{
  Display* pDisplay = xconn.display();
  Window root = xconn.root();
  int screen = xconn.screen();

  m_pDisplay = pDisplay;
  m_root = root;

  if (m_pixmap != None)
  {
    XFreePixmap(pDisplay, m_pixmap);
    m_pixmap = None;
  }

  int screenW = xconn.width();
  int screenH = xconn.height();
  unsigned long bgColor = config.backgroundColor();

  if (config.useBackgroundImage() && !config.backgroundImage().empty())
  {
    int imgW = 0;
    int imgH = 0;
    int channels = 0;
    unsigned char* data = stbi_load(
      config.backgroundImage().c_str(),
      &imgW,
      &imgH,
      &channels,
      4);

    if (data && imgW > 0 && imgH > 0)
    {
      m_pixmap = XCreatePixmap(
        pDisplay,
        root,
        screenW,
        screenH,
        DefaultDepth(pDisplay, screen));

      Visual* visual = DefaultVisual(pDisplay, screen);
      int depth = DefaultDepth(pDisplay, screen);
      size_t bufSize = static_cast<size_t>(screenW) * static_cast<size_t>(screenH) * 4;
      char* xdata = static_cast<char*>(malloc(bufSize));

      if (xdata)
      {
        // Cover mode: scale to fill screen, crop center (no bad stretch)
        double scaleX = static_cast<double>(screenW) / static_cast<double>(imgW);
        double scaleY = static_cast<double>(screenH) / static_cast<double>(imgH);
        double scale = (scaleX > scaleY) ? scaleX : scaleY;
        int scaledW = static_cast<int>(imgW * scale + 0.5);
        int scaledH = static_cast<int>(imgH * scale + 0.5);
        int srcOffsetX = (scaledW - screenW) / 2;
        int srcOffsetY = (scaledH - screenH) / 2;

        for (int y = 0; y < screenH; ++y)
        {
          for (int x = 0; x < screenW; ++x)
          {
            int sx = static_cast<int>((x + srcOffsetX) / scale);
            int sy = static_cast<int>((y + srcOffsetY) / scale);
            if (sx < 0)
            {
              sx = 0;
            }
            if (sy < 0)
            {
              sy = 0;
            }
            if (sx >= imgW)
            {
              sx = imgW - 1;
            }
            if (sy >= imgH)
            {
              sy = imgH - 1;
            }
            unsigned char* src = data + (sy * imgW + sx) * 4;
            char* dst = xdata + (y * screenW + x) * 4;
            dst[0] = static_cast<char>(src[2]);
            dst[1] = static_cast<char>(src[1]);
            dst[2] = static_cast<char>(src[0]);
            dst[3] = 0;
          }
        }

        XImage* image = XCreateImage(
          pDisplay,
          visual,
          depth,
          ZPixmap,
          0,
          xdata,
          screenW,
          screenH,
          32,
          0);

        if (image)
        {
          GC gc = XCreateGC(pDisplay, m_pixmap, 0, nullptr);
          XPutImage(pDisplay, m_pixmap, gc, image, 0, 0, 0, 0, screenW, screenH);
          XFreeGC(pDisplay, gc);
          XDestroyImage(image);
        }
        else
        {
          free(xdata);
        }
      }

      stbi_image_free(data);
      XSetWindowBackgroundPixmap(pDisplay, root, m_pixmap);
      XClearWindow(pDisplay, root);
      printf("mew: background image set (cover): %s\n", config.backgroundImage().c_str());
      return;
    }

    if (data)
    {
      stbi_image_free(data);
    }
    fprintf(stderr, "mew: failed to load background image: %s\n",
            config.backgroundImage().c_str());
  }

  XSetWindowBackground(pDisplay, root, bgColor);
  XClearWindow(pDisplay, root);
}
