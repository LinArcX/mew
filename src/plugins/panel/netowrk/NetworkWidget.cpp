#include "panel/PanelWidgetRegistry.hpp"
#include "NetworkWidget.hpp"
#include <X11/Xutil.h>
#include <dirent.h>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

int NetworkWidget::width() const
{
  XftFont* pFont = m_font.font();
  if (!pFont)
  {
    return 110;
  }

  std::string label = m_selected.empty() ? "net" : m_selected;
  if (label.size() > 8)
  {
    label = label.substr(0, 8);
  }

  XGlyphInfo ext{};
  XftTextExtentsUtf8(
    m_xconn.display(),
    pFont,
    reinterpret_cast<const FcChar8*>(label.c_str()),
    static_cast<int>(label.size()),
    &ext);

  // 22 = 16px indicator + 6px gap, + 12px padding.
  return 22 + ext.xOff + 12;
}

NetworkWidget::~NetworkWidget()
{
  if (m_popup != None && m_xconn.display())
    XDestroyWindow(m_xconn.display(), m_popup);
}

bool NetworkWidget::isUp(const std::string& n) const
{
  std::ifstream f("/sys/class/net/" + n + "/operstate");
  std::string s;
  f >> s;
  return s == "up";
}

void NetworkWidget::refresh()
{
  m_ifaces.clear();
  DIR* d = opendir("/sys/class/net");
  if (d)
  {
    struct dirent* e = nullptr;
    while ((e = readdir(d)) != nullptr)
    {
      std::string n = e->d_name;
      if (n == "." || n == ".." || n == "lo") continue;
      m_ifaces.push_back(n);
    }
    closedir(d);
    std::sort(m_ifaces.begin(), m_ifaces.end());
  }
  if (std::find(m_ifaces.begin(), m_ifaces.end(), m_selected) == m_ifaces.end())
  {
    m_selected.clear();
    for (const std::string& n : m_ifaces)
      if (isUp(n)) { m_selected = n; break; }
    if (m_selected.empty() && !m_ifaces.empty()) m_selected = m_ifaces.front();
  }
  m_lastRefresh = time(nullptr);
}

void NetworkWidget::killSwitch()
{
  if (m_selected.empty()) refresh();
  if (m_selected.empty()) return;
  bool up = isUp(m_selected);
  std::string cmd = "ip link set dev " + m_selected + (up ? " down" : " up");
  if (std::system((cmd + " >/dev/null 2>&1").c_str()) != 0)
  {
    if (std::system(("pkexec " + cmd + " >/dev/null 2>&1").c_str()) != 0)
      std::system(("sudo -n " + cmd + " >/dev/null 2>&1").c_str());
  }
  struct timespec ts = {0, 150 * 1000 * 1000};
  nanosleep(&ts, nullptr);
  refresh();
}

void NetworkWidget::draw(Display* d, Window panel, int x, int baseline)
{
  if (m_lastRefresh == 0) refresh();
  GC gc = XCreateGC(d, panel, 0, nullptr);
  bool up = !m_selected.empty() && isUp(m_selected);
  XSetForeground(d, gc, up ? 0x22cc44 : 0xcc2222);
  XFillRectangle(d, panel, gc, x, 6, 16, 16);
  XFreeGC(d, gc);
  std::string label = m_selected.empty() ? "net" : m_selected;
  if (label.size() > 8) label = label.substr(0, 8);
  m_font.draw(d, m_xconn.screen(), panel, x + 22, baseline, label);
}

bool NetworkWidget::tick()
{
  time_t now = time(nullptr);
  if (now - m_lastRefresh >= 5)
  {
    refresh();
    return true;
  }
  return false;
}

void NetworkWidget::showPopup(int screenX)
{
  Display* d = m_xconn.display();
  refresh();
  int rows = std::max(1, static_cast<int>(m_ifaces.size()));
  int h = rows * kRowH;
  int sw = m_xconn.width();
  int sh = m_xconn.height();
  int px = screenX + width() - kPopupW;
  int py = sh - MewConst::panelHeight - h - 4;
  if (px < 0) px = 0;
  if (px + kPopupW > sw) px = sw - kPopupW;

  if (m_popup == None)
  {
    XSetWindowAttributes a{};
    a.override_redirect = True;
    a.background_pixel = 0x222222;
    a.event_mask = ExposureMask | ButtonPressMask;
    m_popup = XCreateWindow(d, m_xconn.root(), px, py, kPopupW, h, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask, &a);
  }
  else
  {
    XMoveResizeWindow(d, m_popup, px, py, kPopupW, h);
  }
  XMapRaised(d, m_popup);
  m_popupActive = true;
  XGrabKeyboard(d, m_popup, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  drawPopup();
}

void NetworkWidget::hidePopup()
{
  if (m_popup != None && m_popupActive)
  {
    XUnmapWindow(m_xconn.display(), m_popup);
    XUngrabKeyboard(m_xconn.display(), CurrentTime);
  }
  m_popupActive = false;
}

void NetworkWidget::drawPopup()
{
  if (m_popup == None || !m_popupActive) return;
  Display* d = m_xconn.display();
  int screen = m_xconn.screen();
  int rows = std::max(1, static_cast<int>(m_ifaces.size()));
  int h = rows * kRowH;
  GC gc = XCreateGC(d, m_popup, 0, nullptr);
  XSetForeground(d, gc, 0x222222);
  XFillRectangle(d, m_popup, gc, 0, 0, kPopupW, h);
  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_popup, gc, 0, 0, kPopupW - 1, h - 1);

  XftFont* f = m_font.font();
  if (m_ifaces.empty())
  {
    m_font.setColor(0x888888);
    m_font.draw(d, screen, m_popup, 12, 18, "(no interfaces)");
  }
  else
  {
    for (size_t i = 0; i < m_ifaces.size(); ++i)
    {
      int yy = static_cast<int>(i) * kRowH;
      if (m_ifaces[i] == m_selected)
      {
        XSetForeground(d, gc, 0x0a64c8);
        XFillRectangle(d, m_popup, gc, 2, yy + 1, kPopupW - 4, kRowH - 2);
      }
      std::string s = m_ifaces[i] + (isUp(m_ifaces[i]) ? "  [up]" : "  [down]");
      int bl = yy + (kRowH + (f ? f->ascent : 10)) / 2 - 2;
      m_font.setColor(0xffffff);
      m_font.draw(d, screen, m_popup, 12, bl, s);
    }
  }
  XFreeGC(d, gc);
}

bool NetworkWidget::handleLocalClick(int localX, int screenX)
{
  if (localX < 20)
  {
    killSwitch();
    return true;
  }
  if (m_popupActive) hidePopup();
  else               showPopup(screenX);
  return true;
}

bool NetworkWidget::handleEscape()
{
  if (m_popupActive) { hidePopup(); return true; }
  return false;
}

bool NetworkWidget::handlePopupClick(XButtonEvent* e)
{
  if (!m_popupActive) return false;
  int idx = e->y / kRowH;
  if (idx >= 0 && idx < static_cast<int>(m_ifaces.size()))
  {
    std::string prev = m_selected;
    m_selected = m_ifaces[idx];
    if (!isUp(m_selected) && !prev.empty() && isUp(prev))
    {
      std::string cmd = "ip link set dev " + prev + " down";
      if (std::system((cmd + " >/dev/null 2>&1").c_str()) != 0)
        if (std::system(("pkexec " + cmd + " >/dev/null 2>&1").c_str()) != 0)
          std::system(("sudo -n " + cmd + " >/dev/null 2>&1").c_str());
    }
  }
  hidePopup();
  return true;
}

static PanelWidget* createNetwork(XConnection& x, FontRenderer& f) { return new NetworkWidget(x, f); }
static PanelWidgetRegistrar s_network("network", createNetwork);
