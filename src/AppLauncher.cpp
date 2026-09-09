#include "AppLauncher.hpp"
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
  if (m_window != None && m_xconn.display())
  {
    XDestroyWindow(m_xconn.display(), m_window);
    m_window = None;
  }
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

  std::sort(m_apps.begin(), m_apps.end(),
    [](const DesktopApp& a, const DesktopApp& b) {
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
    int bl = y + (kLineH + (pFont ? pFont->ascent : 10)) / 2 - 2;
    m_font.draw(d, m_xconn.screen(), m_window, kPad + 8, bl, m_apps[static_cast<size_t>(appIdx)].name);
  }

  XFreeGC(d, gc);
}

void AppLauncher::show()
{
  if (m_apps.empty())
  {
    scanApps();
  }

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
    if (m_index > 0)
    {
      --m_index;
      ensureVisible();
    }
    draw();
    return;
  }
  if (sym == XK_Down || sym == XK_KP_Down)
  {
    if (!m_filtered.empty() && m_index + 1 < m_filtered.size())
    {
      ++m_index;
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
