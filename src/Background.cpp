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
        // Fill with background_color (centered image, no stretch)
        unsigned char br = static_cast<unsigned char>((bgColor >> 16) & 0xff);
        unsigned char bg = static_cast<unsigned char>((bgColor >> 8) & 0xff);
        unsigned char bb = static_cast<unsigned char>(bgColor & 0xff);
        for (size_t i = 0; i < bufSize; i += 4)
        {
          xdata[i + 0] = static_cast<char>(bb);
          xdata[i + 1] = static_cast<char>(bg);
          xdata[i + 2] = static_cast<char>(br);
          xdata[i + 3] = 0;
        }

        // Center original image (clip if larger than screen)
        int dstW = imgW;
        int dstH = imgH;
        int srcX = 0;
        int srcY = 0;
        if (dstW > screenW)
        {
          srcX = (dstW - screenW) / 2;
          dstW = screenW;
        }
        if (dstH > screenH)
        {
          srcY = (dstH - screenH) / 2;
          dstH = screenH;
        }
        int offsetX = (screenW - dstW) / 2;
        int offsetY = (screenH - dstH) / 2;

        for (int y = 0; y < dstH; ++y)
        {
          for (int x = 0; x < dstW; ++x)
          {
            unsigned char* src = data + ((srcY + y) * imgW + (srcX + x)) * 4;
            char* dst = xdata + ((offsetY + y) * screenW + (offsetX + x)) * 4;
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
      printf("mew: background image set (centered): %s\n", config.backgroundImage().c_str());
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
