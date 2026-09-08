#include "configs.h"
#include "strings.h"
#include "stb_image.h"

#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <X11/Xlib.h>

namespace Mew
{
  std::string get_pidfile()
  {
    return "/tmp/mew.pid";
  }
  
  void write_pidfile()
  {
    std::ofstream f(get_pidfile());
    if (f) {
      f << getpid() << '\n';
    }
  }
  
  void removePIDFile()
  {
    unlink(get_pidfile().c_str());
  }
  
  void create_config_directory()
  {
    const char* home = getenv("HOME");
    if (!home) {
      return;
    }
  
    std::string config = std::string(home) + "/.config";
    std::string mew = config + "/mew";
  
    mkdir(config.c_str(), 0755);
    mkdir(mew.c_str(), 0755);
  }
  
  std::string Configs::getConfigDirectory()
  {
    const char* home = getenv("HOME");
    if (!home) {
      return "";
    }
    return std::string(home) + "/.config/mew";
  }

  void Configs::load(Panel * const panel)
  {
    // Reset to defaults before reading (last entry in file wins for bg)
    backgroundColor = 0x3B3C3C;
    backgroundImage.clear();
    useBackgroundImage = false;
  
    std::string path = getConfigDirectory() + "/config";
    std::ifstream file(path);
    if (!file.is_open()) {
      fprintf(stderr, "mew: no config file: %s (using defaults)\n", path.c_str());
      return;
    }
  
    std::string line;
    while (std::getline(file, line)) {
      line = Strings::trim(line);
      if (line.empty() || line[0] == '#') {
        continue;
      }
  
      size_t eq = line.find('=');
      if (eq == std::string::npos) {
        continue;
      }
  
      std::string key = Strings::trim(line.substr(0, eq));
      std::string val = Strings::trim(line.substr(eq + 1));
  
      // strip optional quotes
      if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
        val = val.substr(1, val.size() - 2);
      }
      if (key == "titleFontSize") {
        titleFontSize = std::atof(val.c_str());
        if (titleFontSize < 8.0) {
          titleFontSize = 8.0;
        }
      }
      else if (key == "mouseTheme") {
        mouseTheme = val;
      }
      else if (key == "mouseSize") {
        mouseSize = std::atoi(val.c_str());
        if (mouseSize < 8) {
          mouseSize = 8;
        }
      }
      else if (key == "backgroundColor") {
        if (!val.empty() && val[0] == '#') {
          val = "0x" + val.substr(1);
        }
        backgroundColor = std::strtoul(val.c_str(), nullptr, 0);
        useBackgroundImage = false; // last wins
      }
      else if (key == "panelColor") {
        if (!val.empty() && val[0] == '#') {
          val = "0x" + val.substr(1);
        }
        panelColor = std::strtoul(val.c_str(), nullptr, 0);
        panel->setColorBackground(panelColor);
      }
      else if (key == "backgroundImage") {
        backgroundImage = Strings::expandHome(val);
        useBackgroundImage = true; // last wins
      }
      else if (key == "loginSound") {
        loginSound = Strings::expandHome(val);
      }
      else if (key == "logoutSound") {
        logoutSound = Strings::expandHome(val);
      }
      else if (key == "windowTheme") {
        windowTheme = val;
      }
    }
  }
  
  void Configs::applyBackground(
    Window root,
    const int screen,
    Display * display)
  {
    // Free previous pixmap if any
    if (backgroundPixmap != None) {
      XFreePixmap(display, backgroundPixmap);
      backgroundPixmap = None;
    }
  
    int screen_w = DisplayWidth(display, screen);
    int screen_h = DisplayHeight(display, screen);
  
    if (useBackgroundImage && !backgroundImage.empty()) {
      int img_w = 0, img_h = 0, channels = 0;
      unsigned char* data = stbi_load(backgroundImage.c_str(),
        &img_w, &img_h, &channels, 4); // force RGBA
  
      if (data && img_w > 0 && img_h > 0) {
        backgroundPixmap = XCreatePixmap(
          display, root, screen_w, screen_h,
          DefaultDepth(display, screen));
  
        // Build XImage from stretched pixels (nearest-neighbor)
        Visual* visual = DefaultVisual(display, screen);
        int depth = DefaultDepth(display, screen);
  
        // Create 32-bit ZPixmap buffer
        size_t bufSize = (size_t)screen_w * screen_h * 4;
        char* xdata = (char*)malloc(bufSize);
        if (xdata) {
          for (int y = 0; y < screen_h; ++y) {
            int sy = y * img_h / screen_h;
            for (int x = 0; x < screen_w; ++x) {
              int sx = x * img_w / screen_w;
              unsigned char* src = data + (sy * img_w + sx) * 4;
              // X11 typically wants BGRX on little-endian
              char* dst = xdata + (y * screen_w + x) * 4;
              dst[0] = (char)src[2]; // B
              dst[1] = (char)src[1]; // G
              dst[2] = (char)src[0]; // R
              dst[3] = 0;
            }
          }
  
          XImage* image = XCreateImage(
            display, visual, depth, ZPixmap, 0,
            xdata, screen_w, screen_h, 32, 0
          );
  
          if (image) {
            GC gc = XCreateGC(display, backgroundPixmap, 0, nullptr);
            XPutImage(display, backgroundPixmap, gc, image,
                      0, 0, 0, 0, screen_w, screen_h);
            XFreeGC(display, gc);
            // XDestroyImage frees xdata
            XDestroyImage(image);
          }
          else {
            free(xdata);
          }
        }
        stbi_image_free(data);
  
        XSetWindowBackgroundPixmap(display, root, backgroundPixmap);
        XClearWindow(display, root);
        printf("mew: background image set: %s\n", backgroundImage.c_str());
        return;
      }
  
      if (data) {
        stbi_image_free(data);
      }
      fprintf(stderr, "mew: failed to load background image: %s\n", backgroundImage.c_str());
    }
  
    // Solid color fallback / explicit color
    XSetWindowBackground(display, root, backgroundColor);
    XClearWindow(display, root);
  }
}
