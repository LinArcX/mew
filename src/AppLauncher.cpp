#include "AppLauncher.hpp"
#include "stb_image.h"
#include <X11/Xutil.h>
#include <cstdlib>
#include <map>
#include "Util.hpp"

#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>

AppLauncher::AppLauncher(XConnection& xconn, FontRenderer& font)
  : m_xconn(xconn)
  , m_font(font)
{
}

AppLauncher::~AppLauncher()
{
  hide();
  clearIconCache();
  if (m_window != None && m_xconn.display())
  {
    XDestroyWindow(m_xconn.display(), m_window);
    m_window = None;
  }
}

void AppLauncher::clearIconCache()
{
  Display* d = m_xconn.display();
  if (d)
  {
    for (auto& kv : m_iconCache)
    {
      if (kv.second != None)
      {
        XFreePixmap(d, kv.second);
      }
    }
  }
  m_iconCache.clear();
}

std::string AppLauncher::desktopField(const std::string& content, const std::string& key)
{
  std::string prefix = key + "=";
  std::istringstream ss(content);
  std::string line;
  while (std::getline(ss, line))
  {
    line = Util::trim(line);
    if (line.rfind(prefix, 0) == 0)
    {
      return line.substr(prefix.size());
    }
  }
  return "";
}

void AppLauncher::scanDir(const std::string& dir)
{
  DIR* d = opendir(dir.c_str());
  if (!d)
  {
    return;
  }

  struct dirent* ent = nullptr;
  while ((ent = readdir(d)) != nullptr)
  {
    std::string name = ent->d_name;
    if (name.size() < 9 || name.substr(name.size() - 8) != ".desktop")
    {
      continue;
    }

    std::string path = dir + "/" + name;
    std::ifstream f(path);
    if (!f.is_open())
    {
      continue;
    }

    std::ostringstream oss;
    oss << f.rdbuf();
    std::string content = oss.str();

    if (content.find("NoDisplay=true") != std::string::npos)
    {
      continue;
    }
    if (content.find("Hidden=true") != std::string::npos)
    {
      continue;
    }

    std::string appName = desktopField(content, "Name");
    std::string exec = desktopField(content, "Exec");
    std::string icon = desktopField(content, "Icon");
    if (appName.empty() || exec.empty())
    {
      continue;
    }

    std::string cleaned;
    for (size_t i = 0; i < exec.size(); ++i)
    {
      if (exec[i] == '%' && i + 1 < exec.size())
      {
        ++i;
        continue;
      }
      cleaned.push_back(exec[i]);
    }
    exec = Util::trim(cleaned);

    DesktopApp app;
    app.name = appName;
    app.exec = exec;
    app.icon = icon;
    m_apps.push_back(app);
  }
  closedir(d);
}

void AppLauncher::scanApps()
{
  m_apps.clear();
  const char* home = getenv("HOME");
  if (home)
  {
    scanDir(std::string(home) + "/.local/share/applications");
  }
  scanDir("/usr/share/applications");
  scanDir("/usr/local/share/applications");

  loadFrequency();
  std::sort(m_apps.begin(), m_apps.end(),
    [](const DesktopApp& a, const DesktopApp& b) {
      if (a.useCount != b.useCount)
      {
        return a.useCount > b.useCount;
      }
      return a.name < b.name;
    });
}

void AppLauncher::filter()
{
  m_filtered.clear();
  std::string q = m_query;
  for (char& c : q)
  {
    if (c >= 'A' && c <= 'Z')
    {
      c = static_cast<char>(c + 32);
    }
  }

  for (size_t i = 0; i < m_apps.size(); ++i)
  {
    std::string n = m_apps[i].name;
    for (char& c : n)
    {
      if (c >= 'A' && c <= 'Z')
      {
        c = static_cast<char>(c + 32);
      }
    }
    if (q.empty() || n.find(q) != std::string::npos)
    {
      m_filtered.push_back(static_cast<int>(i));
    }
  }

  if (m_index >= m_filtered.size())
  {
    m_index = m_filtered.empty() ? 0 : m_filtered.size() - 1;
  }
}


void AppLauncher::ensureVisible()
{
  if (m_filtered.empty())
  {
    m_scroll = 0;
    m_index = 0;
    return;
  }
  if (m_index >= m_filtered.size())
  {
    m_index = m_filtered.size() - 1;
  }
  if (m_index < m_scroll)
  {
    m_scroll = m_index;
  }
  if (m_index >= m_scroll + static_cast<size_t>(kMaxVisible))
  {
    m_scroll = m_index - static_cast<size_t>(kMaxVisible) + 1;
  }
}

int AppLauncher::windowHeight() const
{
  return kPad * 2 + kLineH + kMaxVisible * kLineH + 8;
}

void AppLauncher::hide()
{
  if (m_window != None && m_active)
  {
    XUnmapWindow(m_xconn.display(), m_window);
    XUngrabKeyboard(m_xconn.display(), CurrentTime);
  }
  m_active = false;
  m_query.clear();
  m_index = 0;
  m_scroll = 0;
}

void AppLauncher::draw()
{
  if (m_window == None || !m_active)
  {
    return;
  }

  Display* d = m_xconn.display();
  int height = windowHeight();
  GC gc = XCreateGC(d, m_window, 0, nullptr);

  XSetForeground(d, gc, 0x1e1e1e);
  XFillRectangle(d, m_window, gc, 0, 0, kWidth, height);

  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_window, gc, 0, 0, kWidth - 1, height - 1);

  XSetForeground(d, gc, 0x2a2a2a);
  XFillRectangle(d, m_window, gc, kPad, kPad, kWidth - kPad * 2, kLineH);

  XftFont* pFont = m_font.font();
  std::string prompt = "> " + m_query + "_";
  int baseline = kPad + (kLineH + (pFont ? pFont->ascent : 10)) / 2 - 2;
  m_font.draw(d, m_xconn.screen(), m_window, kPad + 8, baseline, prompt);

  int y0 = kPad + kLineH + 4;
  size_t end = std::min(m_filtered.size(), m_scroll + static_cast<size_t>(kMaxVisible));
  for (size_t i = m_scroll; i < end; ++i)
  {
    int row = static_cast<int>(i - m_scroll);
    int y = y0 + row * kLineH;
    if (i == m_index)
    {
      XSetForeground(d, gc, 0x0a64c8);
      XFillRectangle(d, m_window, gc, 4, y, kWidth - 8, kLineH);
    }
    int appIdx = m_filtered[i];
    const DesktopApp& app = m_apps[static_cast<size_t>(appIdx)];
    std::string iconPath = resolveIconPath(app.icon);
    if (!iconPath.empty())
    {
      drawIcon(d, m_window, kPad + 4, y + 2, iconPath);
    }
    int bl = y + (kLineH + (pFont ? pFont->ascent : 10)) / 2 - 2;
    m_font.draw(d, m_xconn.screen(), m_window, kPad + 28, bl, app.name);
  }

  XFreeGC(d, gc);
}

void AppLauncher::show()
{
  // Always rescan + reload frequency so sort by useCount is up to date
  scanApps();

  m_query.clear();
  m_index = 0;
  m_scroll = 0;
  filter();

  Display* d = m_xconn.display();
  int height = windowHeight();
  int x = (m_xconn.width() - kWidth) / 2;
  int y = (m_xconn.height() - height) / 3;

  if (m_window == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0x1e1e1e;
    attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

    m_window = XCreateWindow(
      d, m_xconn.root(),
      x, y, kWidth, height, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_window, x, y, kWidth, height);
  }

  XMapRaised(d, m_window);
  XClearWindow(d, m_window);
  XGrabKeyboard(d, m_window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  m_active = true;
  draw();
}

void AppLauncher::launchSelected()
{
  if (m_filtered.empty() || m_index >= m_filtered.size())
  {
    return;
  }
  int appIdx = m_filtered[m_index];
  m_apps[static_cast<size_t>(appIdx)].useCount += 1;
  saveFrequency();
  std::string cmd = m_apps[static_cast<size_t>(appIdx)].exec + " >/dev/null 2>&1 &";
  std::system(cmd.c_str());
  hide();
}

void AppLauncher::handleKey(XKeyEvent* pEvent)
{
  if (!pEvent)
  {
    return;
  }

  KeySym sym = XLookupKeysym(pEvent, 0);
  char buf[8] = {};
  XLookupString(pEvent, buf, sizeof(buf) - 1, &sym, nullptr);

  if (sym == XK_Escape)
  {
    hide();
    return;
  }
  if (sym == XK_Return)
  {
    launchSelected();
    return;
  }
  if (sym == XK_Up || sym == XK_KP_Up)
  {
    if (!m_filtered.empty())
    {
      if (m_index > 0)
      {
        --m_index;
      }
      else
      {
        m_index = m_filtered.size() - 1;
      }
      ensureVisible();
    }
    draw();
    return;
  }
  if (sym == XK_Down || sym == XK_KP_Down)
  {
    if (!m_filtered.empty())
    {
      if (m_index + 1 < m_filtered.size())
      {
        ++m_index;
      }
      else
      {
        m_index = 0;
      }
      ensureVisible();
    }
    draw();
    return;
  }
  if (sym == XK_BackSpace)
  {
    if (!m_query.empty())
    {
      m_query.pop_back();
      m_index = 0;
      m_scroll = 0;
      filter();
      ensureVisible();
      draw();
    }
    return;
  }

  if (buf[0] >= 32 && buf[0] < 127)
  {
    m_query.push_back(buf[0]);
    m_index = 0;
    m_scroll = 0;
    filter();
    ensureVisible();
    draw();
  }
}

void AppLauncher::handleClick(XButtonEvent* pEvent)
{
  if (!pEvent)
  {
    return;
  }

  int y0 = kPad + kLineH + 4;
  if (pEvent->y < y0)
  {
    return;
  }

  size_t row = static_cast<size_t>((pEvent->y - y0) / kLineH);
  size_t idx = m_scroll + row;
  if (idx < m_filtered.size() && row < static_cast<size_t>(kMaxVisible))
  {
    m_index = idx;
    launchSelected();
  }
}


void AppLauncher::loadFrequency()
{
  std::string path = Util::getConfigDirectory() + "/app_freq";
  std::ifstream f(path);
  if (!f.is_open())
  {
    return;
  }
  std::map<std::string, int> counts;
  std::string line;
  while (std::getline(f, line))
  {
    size_t tab = line.find('\t');
    if (tab == std::string::npos)
    {
      continue;
    }
    std::string name = line.substr(0, tab);
    int count = static_cast<int>(std::strtol(line.c_str() + static_cast<long>(tab) + 1, nullptr, 10));
    counts[name] = count;
  }
  for (DesktopApp& app : m_apps)
  {
    auto it = counts.find(app.name);
    if (it != counts.end())
    {
      app.useCount = it->second;
    }
  }
}

void AppLauncher::saveFrequency()
{
  std::string path = Util::getConfigDirectory() + "/app_freq";
  std::ofstream f(path);
  if (!f.is_open())
  {
    return;
  }
  for (const DesktopApp& app : m_apps)
  {
    if (app.useCount > 0)
    {
      f << app.name << '\t' << app.useCount << '\n';
    }
  }
}

std::string AppLauncher::resolveIconPath(const std::string& icon) const
{
  if (icon.empty())
  {
    return "";
  }
  if (icon[0] == '/')
  {
    std::ifstream test(icon);
    if (test.good())
    {
      return icon;
    }
    return "";
  }
  // Prefer higher-res icons for better downscale quality
  const char* bases[] = {
    "/usr/share/icons/hicolor/64x64/apps/",
    "/usr/share/icons/hicolor/48x48/apps/",
    "/usr/share/icons/hicolor/32x32/apps/",
    "/usr/share/icons/hicolor/scalable/apps/",
    "/usr/share/icons/Adwaita/64x64/apps/",
    "/usr/share/icons/Adwaita/48x48/apps/",
    "/usr/share/icons/Adwaita/32x32/apps/",
    "/usr/share/icons/Yaru/64x64/apps/",
    "/usr/share/icons/Yaru/48x48/apps/",
    "/usr/share/icons/Yaru/32x32/apps/",
    "/usr/share/pixmaps/",
    "/usr/share/icons/",
    nullptr
  };
  const char* exts[] = {".png", ".svg", ".xpm", "", nullptr};
  for (int i = 0; bases[i]; ++i)
  {
    for (int e = 0; exts[e]; ++e)
    {
      std::string p = std::string(bases[i]) + icon + exts[e];
      std::ifstream test(p);
      if (test.good())
      {
        return p;
      }
    }
  }
  return "";
}

void AppLauncher::drawIcon(Display* d, Window win, int x, int y, const std::string& path)
{
  const int size = 24;
  auto it = m_iconCache.find(path);
  if (it != m_iconCache.end() && it->second != None)
  {
    GC gc = XCreateGC(d, win, 0, nullptr);
    XCopyArea(d, it->second, win, gc, 0, 0, size, size, x, y);
    XFreeGC(d, gc);
    return;
  }

  int iw = 0;
  int ih = 0;
  int ch = 0;
  unsigned char* data = stbi_load(path.c_str(), &iw, &ih, &ch, 4);
  if (!data || iw <= 0 || ih <= 0)
  {
    GC gc = XCreateGC(d, win, 0, nullptr);
    XSetForeground(d, gc, 0x6688aa);
    XFillRectangle(d, win, gc, x, y, size, size);
    XFreeGC(d, gc);
    if (data)
    {
      stbi_image_free(data);
    }
    return;
  }

  char* xdata = static_cast<char*>(std::malloc(static_cast<size_t>(size * size * 4)));
  if (!xdata)
  {
    stbi_image_free(data);
    return;
  }
  // Box-filter downsample for better quality
  for (int py = 0; py < size; ++py)
  {
    for (int px = 0; px < size; ++px)
    {
      int x0 = px * iw / size;
      int y0 = py * ih / size;
      int x1 = (px + 1) * iw / size;
      int y1 = (py + 1) * ih / size;
      if (x1 <= x0)
      {
        x1 = x0 + 1;
      }
      if (y1 <= y0)
      {
        y1 = y0 + 1;
      }
      unsigned int r = 0;
      unsigned int g = 0;
      unsigned int b = 0;
      unsigned int n = 0;
      for (int sy = y0; sy < y1; ++sy)
      {
        for (int sx = x0; sx < x1; ++sx)
        {
          unsigned char* s = data + (sy * iw + sx) * 4;
          r += s[0];
          g += s[1];
          b += s[2];
          ++n;
        }
      }
      if (n == 0)
      {
        n = 1;
      }
      char* dst = xdata + (py * size + px) * 4;
      dst[0] = static_cast<char>(b / n);
      dst[1] = static_cast<char>(g / n);
      dst[2] = static_cast<char>(r / n);
      dst[3] = 0;
    }
  }
  stbi_image_free(data);

  Pixmap pm = XCreatePixmap(d, win, size, size, DefaultDepth(d, m_xconn.screen()));
  XImage* image = XCreateImage(
    d, DefaultVisual(d, m_xconn.screen()), DefaultDepth(d, m_xconn.screen()),
    ZPixmap, 0, xdata, size, size, 32, 0);
  if (image && pm != None)
  {
    GC gc = XCreateGC(d, pm, 0, nullptr);
    XPutImage(d, pm, gc, image, 0, 0, 0, 0, size, size);
    XCopyArea(d, pm, win, gc, 0, 0, size, size, x, y);
    XFreeGC(d, gc);
    XDestroyImage(image);
    m_iconCache[path] = pm;
  }
  else
  {
    if (image)
    {
      XDestroyImage(image);
    }
    else
    {
      std::free(xdata);
    }
    if (pm != None)
    {
      XFreePixmap(d, pm);
    }
  }
}
