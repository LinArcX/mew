#include "ClockWidget.hpp"
#include "../PanelWidgetRegistry.hpp"

void ClockWidget::draw(Display* d, Window panel, int x, int baseline)
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

std::string ClockWidget::tooltip() const { return "Date and time"; }

bool ClockWidget::tick()
{
  time_t now = time(nullptr);
  if (now == m_lastDraw) return false;
  m_lastDraw = now;
  return true;
}

static PanelWidget* createClock(XConnection& x, FontRenderer& f) { return new ClockWidget(x, f); }
static PanelWidgetRegistrar s_clock("clock", createClock);
