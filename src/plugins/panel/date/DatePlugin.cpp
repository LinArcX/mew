#include "DatePlugin.hpp"
#include "../PanelPluginRegistry.hpp"
#include <cstdio>
#include <cstring>

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
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char datePart[48];
  strftime(datePart, sizeof(datePart), "%Y-%B-%d", t);
  char buf[80];
  snprintf(buf, sizeof(buf), "\xee\xaa\xb0 %s", datePart);

  XftFont* pFont = m_font.font();
  if (!pFont)
  {
    return 140;
  }

  XGlyphInfo ext{};
  XftTextExtentsUtf8(
    m_xconn.display(),
    pFont,
    reinterpret_cast<const FcChar8*>(buf),
    static_cast<int>(std::strlen(buf)),
    &ext);

  return ext.xOff;
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
