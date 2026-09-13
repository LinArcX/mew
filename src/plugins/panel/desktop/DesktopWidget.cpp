#include "DesktopWidget.hpp"
#include "Mew.hpp"
static PanelWidget* createDesktop(XConnection& x, FontRenderer& f)
{
  return new DesktopWidget(x, f, *Mew::instance()->clientManager());
}

void DesktopWidget::draw(Display* d, Window panel, int x, int baseline)
{
  m_font.draw(d, m_xconn.screen(), panel, x, baseline, "\xef\x92\xa9");
}

bool DesktopWidget::handleLocalClick(int, int)
{
  if (!m_showing)
  {
    for (Client* c : m_clients.clients())
      if (!c->minimized) m_clients.minimize(c);
    m_showing = true;
  }
  else
  {
    for (Client* c : m_clients.clients())
    {
      if (c->minimized)
      {
        c->minimized = false;
        XMapWindow(m_xconn.display(), c->frame);
      }
    }
    m_showing = false;
    if (!m_clients.clients().empty())
      m_clients.focus(m_clients.clients().back());
  }
  return true;
}

static PanelWidget* createDesktop(XConnection& x, FontRenderer& f) { return new DesktopWidget(x, f, *Mew::instance()->clients()); }
static PanelWidgetRegistrar s_desktop("desktop", createDesktop);
