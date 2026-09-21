#include "ClockPlugin.hpp"
#include "../PanelPluginRegistry.hpp"

void ClockPlugin::draw(Display* d, Window panel, int x, int baseline)
{
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char datePart[48], timePart[16];
  strftime(datePart, sizeof(datePart), "%Y-%B-%d", t);
  strftime(timePart, sizeof(timePart), "%H:%M:%S", t);
  char buf[80];
  snprintf(buf, sizeof(buf), "\xee\xaa\xb0 %s \xee\x99\x81 %s", datePart, timePart);
  m_font.draw(d, m_xconn.screen(), panel, x, baseline, buf);
}

int ClockPlugin::width() const
{
  // Match the exact string produced by draw(): "%Y-%B-%d" plus time.
  std::string sample =
    std::string("\xee\xaa\xb0 ") + "0000-September-00" +
    " \xee\x99\x81 " + "00:00:00";

  XftFont* pFont = m_font.font();
  if (!pFont)
  {
    return 220;
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

std::string ClockPlugin::tooltip() const { return "Date and time"; }

bool ClockPlugin::tick()
{
  time_t now = time(nullptr);
  if (now == m_lastDraw) return false;
  m_lastDraw = now;
  return true;
}

static PanelPlugin* createClock(XConnection& x, FontRenderer& f) { return new ClockPlugin(x, f); }
static PanelPluginRegistrar s_clock("clock", createClock);
