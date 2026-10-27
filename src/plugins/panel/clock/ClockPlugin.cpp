#include "ClockPlugin.hpp"
#include "../PanelPluginRegistry.hpp"

void ClockPlugin::draw(Display* d, Window panel, int x, int baseline)
{
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char timePart[16];
  strftime(timePart, sizeof(timePart), "%H:%M:%S", t);
  char buf[80];
  snprintf(buf, sizeof(buf), "\xee\x99\x81 %s", timePart);
  m_font.draw(d, m_xconn.screen(), panel, x, baseline, buf);
}

int ClockPlugin::width() const
{
  std::string sample = std::string("\xee\x99\x81 ") + "00:00:00";

  XftFont* pFont = m_font.font();
  if (!pFont)
  {
    return 120;
  }

  XGlyphInfo ext{};
  XftTextExtentsUtf8(
    m_xconn.display(),
    pFont,
    reinterpret_cast<const FcChar8*>(sample.c_str()),
    static_cast<int>(sample.size()),
    &ext);

  return ext.xOff + 12;
}

std::string ClockPlugin::tooltip() const { return "Time"; }

bool ClockPlugin::tick()
{
  time_t now = time(nullptr);
  if (now == m_lastDraw) return false;
  m_lastDraw = now;
  return true;
}

static PanelPlugin* createClock(XConnection& x, FontRenderer& f) { return new ClockPlugin(x, f); }
static PanelPluginRegistrar s_clock("clock", createClock);
