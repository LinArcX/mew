#include "logoFull_data.h"
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

  bool wantFile = config.useBackgroundImage() && !config.backgroundImage().empty();
  bool wantEmbedded = !wantFile && config.useEmbeddedBackground();

  unsigned char* data = nullptr;
  int imgW = 0;
  int imgH = 0;
  int channels = 0;

  if (wantFile)
  {
    data = stbi_load(config.backgroundImage().c_str(), &imgW, &imgH, &channels, 4);
  }
  else if (wantEmbedded)
  {
    data = stbi_load_from_memory(logoFull_png,
      static_cast<int>(logoFull_png_len),
      &imgW, &imgH, &channels, 4);
  }

  if (data && imgW > 0 && imgH > 0)
  {
    m_pixmap = XCreatePixmap(
      pDisplay, root, screenW, screenH, DefaultDepth(pDisplay, screen));

    Visual* visual = DefaultVisual(pDisplay, screen);
    int depth = DefaultDepth(pDisplay, screen);
    size_t bufSize = static_cast<size_t>(screenW) * static_cast<size_t>(screenH) * 4;
    char* xdata = static_cast<char*>(malloc(bufSize));

    if (xdata)
    {
      bool canCoverWithoutUpscale = (imgW >= screenW && imgH >= screenH);

      if (canCoverWithoutUpscale)
      {
        // Cover mode: scale to fill screen, crop center.
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
            if (sx < 0) sx = 0;
            if (sy < 0) sy = 0;
            if (sx >= imgW) sx = imgW - 1;
            if (sy >= imgH) sy = imgH - 1;
            unsigned char* src = data + (sy * imgW + sx) * 4;
            char* dst = xdata + (y * screenW + x) * 4;
            dst[0] = static_cast<char>(src[2]);
            dst[1] = static_cast<char>(src[1]);
            dst[2] = static_cast<char>(src[0]);
            dst[3] = 0;
          }
        }
      }
      else
      {
        // Image smaller than screen: fill with bgColor, blit centered at native res.
        unsigned char bgR = static_cast<unsigned char>((bgColor >> 16) & 0xff);
        unsigned char bgG = static_cast<unsigned char>((bgColor >> 8) & 0xff);
        unsigned char bgB = static_cast<unsigned char>(bgColor & 0xff);

        for (int y = 0; y < screenH; ++y)
        {
          for (int x = 0; x < screenW; ++x)
          {
            char* dst = xdata + (y * screenW + x) * 4;
            dst[0] = static_cast<char>(bgB);
            dst[1] = static_cast<char>(bgG);
            dst[2] = static_cast<char>(bgR);
            dst[3] = 0;
          }
        }

        int offsetX = (screenW - imgW) / 2;
        int offsetY = (screenH - imgH) / 2;

        for (int y = 0; y < imgH; ++y)
        {
          int destY = y + offsetY;
          if (destY < 0 || destY >= screenH) continue;
          for (int x = 0; x < imgW; ++x)
          {
            int destX = x + offsetX;
            if (destX < 0 || destX >= screenW) continue;
            unsigned char* src = data + (y * imgW + x) * 4;
            char* dst = xdata + (destY * screenW + destX) * 4;
            dst[0] = static_cast<char>(src[2]);
            dst[1] = static_cast<char>(src[1]);
            dst[2] = static_cast<char>(src[0]);
            dst[3] = 0;
          }
        }
      }

      XImage* image = XCreateImage(
        pDisplay, visual, depth, ZPixmap, 0,
        xdata, screenW, screenH, 32, 0);

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

    if (m_pixmap != None)
    {
      XSetWindowBackgroundPixmap(pDisplay, root, m_pixmap);
      XClearWindow(pDisplay, root);
      printf("mew: background image set (%s)\n",
             wantFile ? config.backgroundImage().c_str() : "embedded");
      return;
    }
  }

  if (data)
  {
    stbi_image_free(data);
  }

  XSetWindowBackground(pDisplay, root, bgColor);
  XClearWindow(pDisplay, root);
}

//void Background::apply(XConnection& xconn, const Config& config)
//{
//  Display* pDisplay = xconn.display();
//  Window root = xconn.root();
//  int screen = xconn.screen();
//
//  m_pDisplay = pDisplay;
//  m_root = root;
//
//  if (m_pixmap != None)
//  {
//    XFreePixmap(pDisplay, m_pixmap);
//    m_pixmap = None;
//  }
//
//  int screenW = xconn.width();
//  int screenH = xconn.height();
//  unsigned long bgColor = config.backgroundColor();
//
//  if (config.useBackgroundImage() && !config.backgroundImage().empty())
//  {
//    int imgW = 0;
//    int imgH = 0;
//    int channels = 0;
//    unsigned char* data = nullptr;
//    if (config.useBackgroundImage() && !config.backgroundImage().empty())
//    {
//      data = stbi_load(config.backgroundImage().c_str(),
//        &imgW,
//        &imgH,
//        &channels,
//        4);
//    }
//    else if (config.useEmbeddedBackground())
//    {
//      data = stbi_load_from_memory(assets_images_logoFull_jpg,
//        static_cast<int>(assets_images_logoFull_jpg_len),
//        &imgW,
//        &imgH,
//        &channels,
//        4);
//    }
//
//      //stbi_load(config.backgroundImage().c_str(),
//      //&imgW,
//      //&imgH,
//      //&channels,
//      //4);
//
//  //  if (data && imgW > 0 && imgH > 0)
//  //  {
//  //    m_pixmap = XCreatePixmap(
//  //      pDisplay,
//  //      root,
//  //      screenW,
//  //      screenH,
//  //      DefaultDepth(pDisplay, screen));
//
//  //    Visual* visual = DefaultVisual(pDisplay, screen);
//  //    int depth = DefaultDepth(pDisplay, screen);
//  //    size_t bufSize = static_cast<size_t>(screenW) * static_cast<size_t>(screenH) * 4;
//  //    char* xdata = static_cast<char*>(malloc(bufSize));
//
//  //    if (xdata)
//  //    {
//  //      // Only upscale-and-crop when the image is at least as large as the
//  //      // screen in both dimensions; otherwise upscaling a low-res image
//  //      // just makes it blurry/stretchy. In that case, center it at its
//  //      // native resolution and pad the rest with background_color.
//  //      bool canCoverWithoutUpscale = (imgW >= screenW && imgH >= screenH);
//
//  //      if (canCoverWithoutUpscale)
//  //      {
//  //        // Cover mode: scale to fill screen, crop center (no bad stretch)
//  //        double scaleX = static_cast<double>(screenW) / static_cast<double>(imgW);
//  //        double scaleY = static_cast<double>(screenH) / static_cast<double>(imgH);
//  //        double scale = (scaleX > scaleY) ? scaleX : scaleY;
//  //        int scaledW = static_cast<int>(imgW * scale + 0.5);
//  //        int scaledH = static_cast<int>(imgH * scale + 0.5);
//  //        int srcOffsetX = (scaledW - screenW) / 2;
//  //        int srcOffsetY = (scaledH - screenH) / 2;
//
//  //        for (int y = 0; y < screenH; ++y)
//  //        {
//  //          for (int x = 0; x < screenW; ++x)
//  //          {
//  //            int sx = static_cast<int>((x + srcOffsetX) / scale);
//  //            int sy = static_cast<int>((y + srcOffsetY) / scale);
//  //            if (sx < 0)
//  //            {
//  //              sx = 0;
//  //            }
//  //            if (sy < 0)
//  //            {
//  //              sy = 0;
//  //            }
//  //            if (sx >= imgW)
//  //            {
//  //              sx = imgW - 1;
//  //            }
//  //            if (sy >= imgH)
//  //            {
//  //              sy = imgH - 1;
//  //            }
//  //            unsigned char* src = data + (sy * imgW + sx) * 4;
//  //            char* dst = xdata + (y * screenW + x) * 4;
//  //            dst[0] = static_cast<char>(src[2]);
//  //            dst[1] = static_cast<char>(src[1]);
//  //            dst[2] = static_cast<char>(src[0]);
//  //            dst[3] = 0;
//  //          }
//  //        }
//  //      }
//  //      else
//  //      {
//  //        // Fill with background_color first, then blit the image centered
//  //        // at native resolution, preserving its original quality.
//  //        unsigned char bgR = static_cast<unsigned char>((bgColor >> 16) & 0xff);
//  //        unsigned char bgG = static_cast<unsigned char>((bgColor >> 8) & 0xff);
//  //        unsigned char bgB = static_cast<unsigned char>(bgColor & 0xff);
//
//  //        for (int y = 0; y < screenH; ++y)
//  //        {
//  //          for (int x = 0; x < screenW; ++x)
//  //          {
//  //            char* dst = xdata + (y * screenW + x) * 4;
//  //            dst[0] = static_cast<char>(bgB);
//  //            dst[1] = static_cast<char>(bgG);
//  //            dst[2] = static_cast<char>(bgR);
//  //            dst[3] = 0;
//  //          }
//  //        }
//
//  //        int offsetX = (screenW - imgW) / 2;
//  //        int offsetY = (screenH - imgH) / 2;
//
//  //        for (int y = 0; y < imgH; ++y)
//  //        {
//  //          int destY = y + offsetY;
//  //          if (destY < 0 || destY >= screenH)
//  //          {
//  //            continue;
//  //          }
//  //          for (int x = 0; x < imgW; ++x)
//  //          {
//  //            int destX = x + offsetX;
//  //            if (destX < 0 || destX >= screenW)
//  //            {
//  //              continue;
//  //            }
//  //            unsigned char* src = data + (y * imgW + x) * 4;
//  //            char* dst = xdata + (destY * screenW + destX) * 4;
//  //            dst[0] = static_cast<char>(src[2]);
//  //            dst[1] = static_cast<char>(src[1]);
//  //            dst[2] = static_cast<char>(src[0]);
//  //            dst[3] = 0;
//  //          }
//  //        }
//  //      }
//
//  //      XImage* image = XCreateImage(
//  //        pDisplay,
//  //        visual,
//  //        depth,
//  //        ZPixmap,
//  //        0,
//  //        xdata,
//  //        screenW,
//  //        screenH,
//  //        32,
//  //        0);
//
//  //      if (image)
//  //      {
//  //        GC gc = XCreateGC(pDisplay, m_pixmap, 0, nullptr);
//  //        XPutImage(pDisplay, m_pixmap, gc, image, 0, 0, 0, 0, screenW, screenH);
//  //        XFreeGC(pDisplay, gc);
//  //        XDestroyImage(image);
//  //      }
//  //      else
//  //      {
//  //        free(xdata);
//  //      }
//  //    }
//
//  if (data && imgW > 0 && imgH > 0)
//  {
//      stbi_image_free(data);
//      XSetWindowBackgroundPixmap(pDisplay, root, m_pixmap);
//      XClearWindow(pDisplay, root);
//      printf("mew: background image set (cover): %s\n", config.backgroundImage().c_str());
//      return;
//    }
//
//    if (data)
//    {
//      stbi_image_free(data);
//    }
//    fprintf(stderr, "mew: failed to load background image: %s\n",
//            config.backgroundImage().c_str());
//  }
//
//  XSetWindowBackground(pDisplay, root, bgColor);
//  XClearWindow(pDisplay, root);
//}
