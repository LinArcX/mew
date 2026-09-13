#include "WeatherWidget.hpp"
#include "../PanelWidgetRegistry.hpp"
#include "../../weather_font_data.h"

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>

namespace
{
  std::string trimStr(const std::string& s)
  {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
    {
      return "";
    }
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
  }

  std::string httpGet(const char* url, int maxSeconds)
  {
    int pipeFd[2];
    if (pipe(pipeFd) != 0)
    {
      return "";
    }
    pid_t pid = fork();
    if (pid < 0)
    {
      close(pipeFd[0]);
      close(pipeFd[1]);
      return "";
    }
    if (pid == 0)
    {
      close(pipeFd[0]);
      dup2(pipeFd[1], STDOUT_FILENO);
      close(pipeFd[1]);
      char secs[16];
      snprintf(secs, sizeof(secs), "%d", maxSeconds);
      execlp("curl", "curl", "-sf", "--max-time", secs, url,
        static_cast<char*>(nullptr));
      _exit(1);
    }
    close(pipeFd[1]);
    std::string out;
    char buf[4096];
    ssize_t n = 0;
    while ((n = read(pipeFd[0], buf, sizeof(buf))) > 0)
    {
      out.append(buf, static_cast<size_t>(n));
      if (out.size() > 256 * 1024)
      {
        break;
      }
    }
    close(pipeFd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
      return "";
    }
    return out;
  }

  // "key": "value"
  std::string jsonString(const std::string& src, const std::string& key)
  {
    size_t pos = src.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = src.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = src.find('"', pos);
    if (pos == std::string::npos) return "";
    size_t end = src.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return src.substr(pos + 1, end - pos - 1);
  }

  // "key": [ { "value": "x" } ]
  std::string jsonArrayValue(const std::string& src, const std::string& key)
  {
    size_t pos = src.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    size_t v = src.find("\"value\"", pos);
    if (v == std::string::npos) return "";
    size_t c = src.find(':', v);
    if (c == std::string::npos) return "";
    size_t o = src.find('"', c);
    if (o == std::string::npos) return "";
    size_t e = src.find('"', o + 1);
    if (e == std::string::npos) return "";
    return src.substr(o + 1, e - o - 1);
  }

   // "key": [ 1, 2, 3 ]
  std::vector<int> jsonIntArray(const std::string& src, const std::string& key)
  {
    std::vector<int> out;
    size_t pos = src.find("\"" + key + "\":[");
    if (pos == std::string::npos)
    {
      return out;
    }
    pos += key.size() + 4; // skip past "key":[
    size_t end = src.find(']', pos);
    if (end == std::string::npos)
    {
      return out;
    }
    std::string content = src.substr(pos, end - pos);
    std::stringstream ss(content);
    std::string item;
    while (std::getline(ss, item, ','))
    {
      item = trimStr(item);
      if (!item.empty() && item != "null")
      {
        out.push_back(static_cast<int>(std::atof(item.c_str())));
      }
    }
    return out;
  }

  // "key": ["a", "b", ...]
  std::vector<std::string> jsonStringArray(const std::string& src, const std::string& key)
  {
    std::vector<std::string> out;
    size_t pos = src.find("\"" + key + "\":[");
    if (pos == std::string::npos)
    {
      return out;
    }
    pos += key.size() + 4;
    size_t end = src.find(']', pos);
    if (end == std::string::npos)
    {
      return out;
    }
    std::string content = src.substr(pos, end - pos);
    size_t i = 0;
    while (i < content.size())
    {
      size_t o = content.find('"', i);
      if (o == std::string::npos) break;
      size_t c = content.find('"', o + 1);
      if (c == std::string::npos) break;
      out.push_back(content.substr(o + 1, c - o - 1));
      i = c + 1;
    }
    return out;
  }
}

WeatherWidget::WeatherWidget(XConnection& xconn, FontRenderer& font)
  : m_xconn(xconn)
  , m_font(font)
{
  Display* d = xconn.display();
  int screen = xconn.screen();

  m_iconFont.load(xconn.display(), xconn.screen(),
    weather_ttf,
    weather_ttf_len,
    14.0);
}

WeatherWidget::~WeatherWidget()
{
  hidePopup();
  if (m_popup != None && m_xconn.display())
  {
    XDestroyWindow(m_xconn.display(), m_popup);
    m_popup = None;
  }
}

const char* WeatherWidget::iconForDesc(const std::string& desc)
{
  if (desc.find("Sunny") != std::string::npos || desc.find("Clear") != std::string::npos)
    return "\xef\x80\x8d";     // wi-day-sunny U+F00D
  if (desc.find("Partly") != std::string::npos || desc.find("Patchy") != std::string::npos)
    return "\xef\x80\x82";     // wi-day-cloudy U+F002
  if (desc.find("Cloudy") != std::string::npos || desc.find("Overcast") != std::string::npos)
    return "\xef\x80\x93";     // wi-cloudy U+F013
  if (desc.find("rain") != std::string::npos || desc.find("drizzle") != std::string::npos
      || desc.find("sleet") != std::string::npos)
    return "\xef\x80\x99";     // wi-rain U+F019
  if (desc.find("snow") != std::string::npos || desc.find("blizzard") != std::string::npos
      || desc.find("ice") != std::string::npos)
    return "\xef\x80\x9b";     // wi-snow U+F01B
  if (desc.find("Thunder") != std::string::npos || desc.find("storm") != std::string::npos)
    return "\xef\x80\x9e";     // wi-thunderstorm U+F01E
  if (desc.find("Fog") != std::string::npos || desc.find("Mist") != std::string::npos)
    return "\xef\x80\x94";     // wi-fog U+F014
  return "\xef\x80\x93";
}

const char* WeatherWidget::iconForCode(int code)
{
  if (code == 0) return "\xef\x80\x8d";                             // clear -> day-sunny
  if (code <= 2) return "\xef\x80\x82";                             // few clouds -> day-cloudy
  if (code == 3) return "\xef\x80\x93";                             // overcast -> cloudy
  if (code == 45 || code == 48) return "\xef\x80\x94";              // fog
  if (code >= 51 && code <= 57) return "\xef\x80\x99";              // drizzle -> rain
  if (code >= 61 && code <= 67) return "\xef\x80\x99";              // rain
  if (code >= 71 && code <= 77) return "\xef\x80\x9b";              // snow
  if (code >= 80 && code <= 82) return "\xef\x80\x9a";              // showers -> shower
  if (code >= 85 && code <= 86) return "\xef\x80\x9b";              // snow showers
  if (code >= 95) return "\xef\x80\x9e";                            // thunderstorm
  return "\xef\x80\x93";
}

void WeatherWidget::configure(const Config& config)
{
  m_locationOverride = config.weatherLocation();
  m_iconColor = config.weatherIconColor();
  m_textColor = config.weatherTextColor();
}

bool WeatherWidget::fetch()
{
  std::string url = m_locationOverride.empty()
    ? "wttr.in/?format=j1"
    : "wttr.in/" + m_locationOverride + "?format=j1";

  std::string json = httpGet(url.c_str(), 8);
  if (json.empty())
  {
    return false;
  }

  m_temp = jsonString(json, "temp_C");
  if (m_temp.empty())
  {
    return false;
  }

  m_desc = jsonArrayValue(json, "weatherDesc");
  if (m_desc.empty())
  {
    m_desc = "Unknown";
  }

  m_location = jsonArrayValue(json, "areaName");
  if (m_location.empty())
  {
    m_location = "Local";
  }

  m_icon = iconForDesc(m_desc);
  m_valid = true;
  m_lastFetch = time(nullptr);

  // Latitude / longitude for the 10-day forecast.
  std::string lat = jsonString(json, "latitude");
  std::string lon = jsonString(json, "longitude");
  if (!lat.empty() && !lon.empty())
  {
    fetchForecast(lat, lon);
  }

  return true;
}

bool WeatherWidget::fetchForecast(const std::string& lat, const std::string& lon)
{
  std::string url =
    "api.open-meteo.com/v1/forecast?latitude=" + lat +
    "&longitude=" + lon +
    "&daily=weather_code,temperature_2m_max,temperature_2m_min"
    "&forecast_days=10&timezone=auto";

  std::string json = httpGet(url.c_str(), 8);
  if (json.empty())
  {
    return false;
  }

  std::vector<std::string> dates = jsonStringArray(json, "time");
  std::vector<int> codes = jsonIntArray(json, "weather_code");
  std::vector<int> maxs = jsonIntArray(json, "temperature_2m_max");
  std::vector<int> mins = jsonIntArray(json, "temperature_2m_min");

  size_t count = std::min(dates.size(), std::min(codes.size(), std::min(maxs.size(), mins.size())));
  m_forecast.clear();
  for (size_t i = 0; i < count; ++i)
  {
    ForecastDay d;
    // "2026-09-13" -> "Sat 13"
    if (dates[i].size() >= 10)
    {
      int month = std::atoi(dates[i].substr(5, 2).c_str());
      int day = std::atoi(dates[i].substr(8, 2).c_str());
      static const char* kMonths[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                      "Jul","Aug","Sep","Oct","Nov","Dec"};
      const char* m = (month >= 1 && month <= 12) ? kMonths[month - 1] : "?";
      char buf[16];
      snprintf(buf, sizeof(buf), "%s %02d", m, day);
      d.date = buf;
    }
    d.icon = iconForCode(codes[i]);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", maxs[i]);
    d.max = buf;
    snprintf(buf, sizeof(buf), "%d", mins[i]);
    d.min = buf;
    m_forecast.push_back(d);
  }
  return !m_forecast.empty();
}

bool WeatherWidget::tick()
{
  time_t now = time(nullptr);
  if (m_valid && (now - m_lastFetch) < kRefreshSeconds)
  {
    return false;
  }
  fetch();
  return false;
}

void WeatherWidget::drawPopup()
{
  if (m_popup == None || !m_popupActive)
  {
    return;
  }

  Display* d = m_xconn.display();
  int screen = m_xconn.screen();
  GC gc = XCreateGC(d, m_popup, 0, nullptr);

  XSetForeground(d, gc, 0x1e1e1e);
  XFillRectangle(d, m_popup, gc, 0, 0, m_popupW, m_popupH);
  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_popup, gc, 0, 0, m_popupW - 1, m_popupH - 1);

  XftFont* pFont = m_font.font();
  int ascent = pFont ? pFont->ascent : 10;
  int iconW = m_iconFont.advanceWidth();
  if (iconW <= 0)
  {
    iconW = 18;
  }

  const int colDate = kPopupPad + 4;
  const int colIcon = kPopupPad + 78;
  const int colTemp = colIcon + iconW + 14;

  for (size_t i = 0; i < m_forecast.size(); ++i)
  {
    const ForecastDay& day = m_forecast[i];

    int rowTop = kPopupPad + static_cast<int>(i) * kPopupRowH;
    int baseline = rowTop + ascent;

    int t = std::atoi(day.max.c_str());

    unsigned long rowBg = 0x1e1e1e;
    unsigned long textColor = 0xffffff;

    if (t >= 40)        { rowBg = 0xcc2222; textColor = 0xffffff; }
    else if (t >= 29)   { rowBg = 0xdd8822; textColor = 0x000000; }
    else if (t >= 18)   { rowBg = 0x22aa44; textColor = 0x000000; }
    else if (t >= 8)    { rowBg = 0xffffff; textColor = 0x000000; }
    else if (t >= 0)    { rowBg = 0xaaddff; textColor = 0x000000; }
    else if (t >= -10)  { rowBg = 0x2266cc; textColor = 0xffffff; }
    else if (t >= -30)  { rowBg = 0x662299; textColor = 0xffffff; }
    else                { rowBg = 0x3a1a55; textColor = 0xffffff; }

    if (rowBg != 0x1e1e1e)
    {
      XSetForeground(d, gc, rowBg);
      XFillRectangle(d, m_popup, gc, 2, rowTop, m_popupW - 4, kPopupRowH);
    }

    m_font.setColor(textColor);
    m_font.draw(d, screen, m_popup, colDate, baseline, day.date);

    if (m_iconFont.font())
    {
      m_iconFont.setColor(textColor);
      m_iconFont.draw(d, screen, m_popup, colIcon, baseline, day.icon);
    }

    std::string temps = day.max + "\xc2\xb0" + " / " + day.min + "\xc2\xb0";
    m_font.draw(d, screen, m_popup, colTemp, baseline, temps);
  }

  m_font.setColor(0xffffff);

  XFreeGC(d, gc);
}

void WeatherWidget::showPopup(int screenX)
{
  Display* d = m_xconn.display();
  int rows = static_cast<int>(m_forecast.size());
  if (rows == 0)
  {
    return;
  }

  m_popupW = kPopupWidth;
  m_popupH = kPopupPad * 2 + rows * kPopupRowH + 4;

  int screenW = m_xconn.width();
  int screenH = m_xconn.height();
  int px = screenX + kWidth - m_popupW;
  int py = screenH - MewConst::panelHeight - m_popupH - 4;
  if (px < 0) px = 0;
  if (px + m_popupW > screenW) px = screenW - m_popupW;

  if (m_popup == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0x1e1e1e;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    m_popup = XCreateWindow(
      d, m_xconn.root(),
      px, py, m_popupW, m_popupH, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_popup, px, py, m_popupW, m_popupH);
  }

  XMapRaised(d, m_popup);
  m_popupActive = true;
  drawPopup();

  // Grab ESC so we can close while pointer is over the popup.
  KeyCode esc = XKeysymToKeycode(d, XK_Escape);
  if (esc != 0)
  {
    unsigned int locks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    for (unsigned int lock : locks)
    {
      XGrabKey(d, esc, lock, m_xconn.root(), True, GrabModeAsync, GrabModeAsync);
    }
    m_escGrabbed = true;
  }
}

void WeatherWidget::hidePopup()
{
  if (m_popup != None && m_popupActive)
  {
    XUnmapWindow(m_xconn.display(), m_popup);
  }
  if (m_escGrabbed)
  {
    KeyCode esc = XKeysymToKeycode(m_xconn.display(), XK_Escape);
    if (esc != 0)
    {
      XUngrabKey(m_xconn.display(), esc, AnyModifier, m_xconn.root());
    }
    m_escGrabbed = false;
  }
  m_popupActive = false;
}

void WeatherWidget::onHover(int screenX)
{
  (void)screenX;
}

void WeatherWidget::onUnhover()
{
  // Popup stays open until ESC or click outside. Nothing to do here.
}

std::string WeatherWidget::tooltip() const
{
  if (!m_valid)
  {
    return "Weather: loading...";
  }
  return m_location + ": " + m_desc + ", " + m_temp + "C";
}

bool WeatherWidget::onClick(int screenX)
{
  time_t now = time(nullptr);
  if (!m_valid || (now - m_lastFetch) >= kRefreshSeconds)
  {
    fetch();
  }
  if (m_forecast.empty())
  {
    return false;
  }
  showPopup(screenX);
  return true;
}

bool WeatherWidget::handleEscape()
{
  if (m_popupActive)
  {
    hidePopup();
    return true;
  }
  return false;
}

void WeatherWidget::draw(Display* display, Window panel, int x, int baseline)
{
  if (!m_valid)
  {
    return;
  }

  int screen = m_xconn.screen();

  if (m_iconFont.font())
  {
    m_iconFont.setColor(m_iconColor);
    m_iconFont.draw(display, screen, panel, x, baseline, m_icon);
    x += m_iconFont.advanceWidth() + 4;
  }

  m_font.setColor(m_textColor);
  std::string text = m_location + " " + m_temp + "\xc2\xb0" + "C";
  m_font.draw(display, screen, panel, x, baseline, text);
}

static PanelWidget* createWeather(XConnection& xconn, FontRenderer& font)
{
  return new WeatherWidget(xconn, font);
}

static PanelWidgetRegistrar s_weatherRegistrar("weather", createWeather);


