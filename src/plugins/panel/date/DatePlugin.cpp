#include "DatePlugin.hpp"
#include "../PanelPluginRegistry.hpp"
#include <cstdio>

void DatePlugin::draw(Display* d, Window panel, int x, int baseline)
{
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char datePart[48];
  strftime(datePart, sizeof(datePart), "%Y-%B-%d", t);
  char buf[80];
  snprintf(buf, sizeof(buf), "\xee\xaa\xb0 %s", datePart);
  m_font.draw(d, m_xconn.screen(), panel, x, baseline, buf);
}

int DatePlugin::width() const
{
  std::string sample = std::string("\xee\xaa\xb0 ") + "0000-September-00";

  XftFont* pFont = m_font.font();
  if (!pFont)
  {
    return 140;
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

bool DatePlugin::tick()
{
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char datePart[48];
  strftime(datePart, sizeof(datePart), "%Y-%B-%d", t);
  
  if (m_lastDate == datePart) return false;
  m_lastDate = datePart;
  return true;
}

static PanelPlugin* createDate(XConnection& x, FontRenderer& f) { return new DatePlugin(x, f); }
static PanelPluginRegistrar s_date("date", createDate);
