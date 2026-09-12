#include "WeatherWidget.hpp"
#include "PanelWidgetRegistry.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

WeatherWidget::WeatherWidget(XConnection& xconn, FontRenderer& font)
  : m_xconn(xconn)
  , m_font(font)
{
  m_pIconFont = XftFontOpenName(
  xconn.display(), xconn.screen(), "Noto Sans Symbols2-12");
  if (!m_pIconFont)
  {
    m_pIconFont = XftFontOpenName(
      xconn.display(), xconn.screen(), "Symbola-12");
  }
}

WeatherWidget::~WeatherWidget()
{
  if (m_pIconFont)
  {
    XftFontClose(m_xconn.display(), m_pIconFont);
  }
}

const char* WeatherWidget::iconFor(const std::string& desc)
{
  if (desc.find("Sunny") != std::string::npos || desc.find("Clear") != std::string::npos)
  {
    return "\xe2\x98\x80";
  }
  if (desc.find("Partly") != std::string::npos || desc.find("Patchy") != std::string::npos)
  {
    return "\xe2\x9b\x85";
  }
  if (desc.find("Cloudy") != std::string::npos || desc.find("Overcast") != std::string::npos)
  {
    return "\xe2\x98\x81";
  }
  if (desc.find("rain") != std::string::npos || desc.find("drizzle") != std::string::npos
      || desc.find("sleet") != std::string::npos)
  {
    return "\xe2\x98\x94";
  }
  if (desc.find("snow") != std::string::npos || desc.find("blizzard") != std::string::npos
      || desc.find("ice") != std::string::npos)
  {
    return "\xe2\x98\x83";
  }
  if (desc.find("Thunder") != std::string::npos || desc.find("storm") != std::string::npos)
  {
    return "\xe2\x9a\xa1";
  }
  if (desc.find("Fog") != std::string::npos || desc.find("Mist") != std::string::npos)
  {
    return "\xf0\x9f\x8c\x81";
  }
  return "\xe2\x98\x81";
}

bool WeatherWidget::fetch()
{
  int pipeFd[2];
  if (pipe(pipeFd) != 0)
  {
    return false;
  }

  pid_t pid = fork();
  if (pid < 0)
  {
    close(pipeFd[0]);
    close(pipeFd[1]);
    return false;
  }

  if (pid == 0)
  {
    close(pipeFd[0]);
    dup2(pipeFd[1], STDOUT_FILENO);
    close(pipeFd[1]);

    std::string url = m_locationOverride.empty()
      ? "wttr.in/?format=j1"
      : "wttr.in/" + m_locationOverride + "?format=j1";

    execlp("curl", "curl",
      "-sf", "--max-time", "8",
      url.c_str(),
      static_cast<char*>(nullptr));
    _exit(1);
  }

  close(pipeFd[1]);
  std::string json;
  char buf[4096];
  ssize_t n = 0;
  while ((n = read(pipeFd[0], buf, sizeof(buf))) > 0)
  {
    json.append(buf, static_cast<size_t>(n));
    if (json.size() > 256 * 1024)
    {
      break;
    }
  }
  close(pipeFd[0]);

  int status = 0;
  waitpid(pid, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || json.empty())
  {
    return false;
  }

  // Direct string: "temp_C": "25"
  auto extractString = [&](const std::string& src, const std::string& key) -> std::string {
    size_t pos = src.find("\"" + key + "\"");
    if (pos == std::string::npos)
    {
      return "";
    }
    pos = src.find(':', pos);
    if (pos == std::string::npos)
    {
      return "";
    }
    pos = src.find('"', pos);
    if (pos == std::string::npos)
    {
      return "";
    }
    size_t end = src.find('"', pos + 1);
    if (end == std::string::npos)
    {
      return "";
    }
    return src.substr(pos + 1, end - pos - 1);
  };

  // Array-of-objects: "areaName": [ { "value": "Berlin" } ]
  auto extractArrayValue = [&](const std::string& src, const std::string& key) -> std::string {
    size_t pos = src.find("\"" + key + "\"");
    if (pos == std::string::npos)
    {
      return "";
    }
    size_t val = src.find("\"value\"", pos);
    if (val == std::string::npos)
    {
      return "";
    }
    size_t colon = src.find(':', val);
    if (colon == std::string::npos)
    {
      return "";
    }
    size_t open = src.find('"', colon);
    if (open == std::string::npos)
    {
      return "";
    }
    size_t close = src.find('"', open + 1);
    if (close == std::string::npos)
    {
      return "";
    }
    return src.substr(open + 1, close - open - 1);
  };

  m_temp = extractString(json, "temp_C");
  if (m_temp.empty())
  {
    return false;
  }

  m_desc = extractArrayValue(json, "weatherDesc");
  if (m_desc.empty())
  {
    m_desc = "Unknown";
  }

  m_location = extractArrayValue(json, "areaName");
  if (m_location.empty())
  {
    m_location = "Local";
  }

  m_icon = iconFor(m_desc);
  m_valid = true;
  m_lastFetch = time(nullptr);
  return true;
}

//bool WeatherWidget::fetch()
//{
//  int pipeFd[2];
//  if (pipe(pipeFd) != 0)
//  {
//    return false;
//  }
//
//  pid_t pid = fork();
//  if (pid < 0)
//  {
//    close(pipeFd[0]);
//    close(pipeFd[1]);
//    return false;
//  }
//
//  if (pid == 0)
//  {
//    close(pipeFd[0]);
//    dup2(pipeFd[1], STDOUT_FILENO);
//    close(pipeFd[1]);
//
//    std::string url = m_locationOverride.empty()
//      ? "wttr.in/?format=j1"
//      : "wttr.in/" + m_locationOverride + "?format=j1";
//
//    execlp("curl", "curl",
//      "-sf", "--max-time", "8",
//      url.c_str(),
//      static_cast<char*>(nullptr));
//    _exit(1);
//  }
//
//  close(pipeFd[1]);
//  std::string json;
//  char buf[4096];
//  ssize_t n = 0;
//  while ((n = read(pipeFd[0], buf, sizeof(buf))) > 0)
//  {
//    json.append(buf, static_cast<size_t>(n));
//    if (json.size() > 256 * 1024)
//    {
//      break;
//    }
//  }
//  close(pipeFd[0]);
//
//  int status = 0;
//  waitpid(pid, &status, 0);
//  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || json.empty())
//  {
//    return false;
//  }
//
//   // Direct string: "temp_C": "25"
//  auto extractString = [&](const std::string& src, const std::string& key) -> std::string {
//    size_t pos = src.find("\"" + key + "\"");
//    if (pos == std::string::npos)
//    {
//      return "";
//    }
//    pos = src.find(':', pos);
//    if (pos == std::string::npos)
//    {
//      return "";
//    }
//    pos = src.find('"', pos);
//    if (pos == std::string::npos)
//    {
//      return "";
//    }
//    size_t end = src.find('"', pos + 1);
//    if (end == std::string::npos)
//    {
//      return "";
//    }
//    return src.substr(pos + 1, end - pos - 1);
//  };
//
//  // Array-of-objects: "areaName": [ { "value": "Berlin" } ]
//  auto extractArrayValue = [&](const std::string& src, const std::string& key) -> std::string {
//    size_t pos = src.find("\"" + key + "\"");
//    if (pos == std::string::npos)
//    {
//      return "";
//    }
//    size_t val = src.find("\"value\"", pos);
//    if (val == std::string::npos)
//    {
//      return "";
//    }
//    size_t colon = src.find(':', val);
//    if (colon == std::string::npos)
//    {
//      return "";
//    }
//    size_t open = src.find('"', colon);
//    if (open == std::string::npos)
//    {
//      return "";
//    }
//    size_t close = src.find('"', open + 1);
//    if (close == std::string::npos)
//    {
//      return "";
//    }
//    return src.substr(open + 1, close - open - 1);
//  };
//
//  //auto extractQuoted = [&](const std::string& src, const std::string& key,
//  //                         const std::string& subKey) -> std::string {
//  //  size_t pos = src.find("\"" + key + "\"");
//  //  if (pos == std::string::npos)
//  //  {
//  //    return "";
//  //  }
//  //  if (!subKey.empty())
//  //  {
//  //    size_t sub = src.find("\"" + subKey + "\"", pos);
//  //    if (sub == std::string::npos)
//  //    {
//  //      return "";
//  //    }
//  //    pos = sub;
//  //  }
//  //  pos = src.find(':', pos);
//  //  if (pos == std::string::npos)
//  //  {
//  //    return "";
//  //  }
//  //  pos = src.find('"', pos);
//  //  if (pos == std::string::npos)
//  //  {
//  //    return "";
//  //  }
//  //  size_t end = src.find('"', pos + 1);
//  //  if (end == std::string::npos)
//  //  {
//  //    return "";
//  //  }
//  //  return src.substr(pos + 1, end - pos - 1);
//  //};
//
//  m_temp = extractQuoted(json, "temp_C", "");
//  if (m_temp.empty())
//  {
//    return false;
//  }
//
//  m_desc = extractQuoted(json, "weatherDesc", "value");
//  if (m_desc.empty())
//  {
//    m_desc = "Unknown";
//  }
//
//  m_location = extractQuoted(json, "nearest_area", "areaName");
//  if (m_location.empty())
//  {
//    m_location = "Local";
//  }
//
//  m_icon = iconFor(m_desc);
//  m_valid = true;
//  m_lastFetch = time(nullptr);
//  return true;
//}

void WeatherWidget::tick()
{
  time_t now = time(nullptr);
  if (m_valid && (now - m_lastFetch) < kRefreshSeconds)
  {
    return;
  }
  fetch();
}

void WeatherWidget::draw(Display* display, Window panel, int x, int baseline)
{
  if (!m_valid)
  {
    return;
  }

  // Draw icon with fallback font if available, else use main font
  if (m_pIconFont)
  {
    XftDraw* draw = XftDrawCreate(
      display, panel,
      DefaultVisual(display, m_xconn.screen()),
      DefaultColormap(display, m_xconn.screen()));
    XftColor color;
    XRenderColor renderColor;
    renderColor.red = 0xffff;
    renderColor.green = 0xffff;
    renderColor.blue = 0xffff;
    renderColor.alpha = 0xffff;
    XftColorAllocValue(
      display,
      DefaultVisual(display, m_xconn.screen()),
      DefaultColormap(display, m_xconn.screen()),
      &renderColor, &color);
    XftDrawStringUtf8(
      draw, &color, m_pIconFont, x, baseline,
      reinterpret_cast<const FcChar8*>(m_icon.c_str()),
      static_cast<int>(m_icon.size()));
    XftColorFree(
      display,
      DefaultVisual(display, m_xconn.screen()),
      DefaultColormap(display, m_xconn.screen()),
      &color);
    XftDrawDestroy(draw);
    x += m_pIconFont->max_advance_width;
  }

  // Draw location + temp with main font
  std::string text = m_location + " " + m_temp + "\xc2\xb0" + "C";
  m_font.draw(display, m_xconn.screen(), panel, x, baseline, text);
}

//void WeatherWidget::draw(Display* display, Window panel, int x, int baseline)
//{
//  if (!m_valid)
//  {
//    return;
//  }
//
//  std::string text = m_icon + " " + m_temp + "\xc2\xb0" + "C";
//  m_font.draw(display, m_xconn.screen(), panel, x, baseline, text);
//}

bool WeatherWidget::onClick()
{
  m_lastFetch = 0;
  fetch();
  return true;
}

std::string WeatherWidget::tooltip() const
{
  if (!m_valid)
  {
    return "Weather: loading...";
  }
  return m_location + ": " + m_desc + ", " + m_temp + "C";
}

static PanelWidget* createWeather(XConnection& xconn, FontRenderer& font)
{
  return new WeatherWidget(xconn, font);
}

void WeatherWidget::configure(const Config& config)
{
  m_locationOverride = config.weatherLocation();
}

static PanelWidgetRegistrar s_weatherRegistrar("weather", createWeather);
