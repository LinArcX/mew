#include "panel/PanelWidgetRegistry.hpp"
#include "VolumeWidget.hpp"
#include "Mew.hpp"
#include "Util.hpp"
#include <X11/Xutil.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#include <unistd.h>

VolumeWidget::~VolumeWidget()
{
  if (m_popup != None && m_xconn.display())
    XDestroyWindow(m_xconn.display(), m_popup);
}

void VolumeWidget::resolveControl(std::string& device, std::string& control) const
{
  device.clear();
  control = "Master";
  if (!m_pConfig) return;
  for (const KeyBinding& b : m_pConfig->keybindings())
  {
    if (b.command.find("amixer") == std::string::npos) continue;
    std::istringstream iss(b.command);
    std::string tok, dev, ctl;
    while (iss >> tok)
    {
      if (tok == "-D" && (iss >> tok)) dev = tok;
      else if ((tok == "set" || tok == "sset") && (iss >> tok)) ctl = tok;
    }
    if (!ctl.empty()) { device = dev; control = ctl; return; }
  }
}

void VolumeWidget::refresh()
{
  std::string device, control;
  resolveControl(device, control);
  std::string cmd = device.empty()
    ? ("amixer sget " + control + " 2>/dev/null")
    : ("amixer -D " + device + " sget " + control + " 2>/dev/null");

  FILE* p = popen(cmd.c_str(), "r");
  if (!p) return;
  char line[256];
  bool found = false, anyOff = false, anyOn = false;
  while (fgets(line, sizeof(line), p))
  {
    char* pct = std::strchr(line, '[');
    if (!pct) continue;
    int v = 0;
    if (std::sscanf(pct, "[%d%%]", &v) == 1) { m_percent = v; found = true; }
    if (std::strstr(line, "[off]")) { anyOff = true; found = true; }
    else if (std::strstr(line, "[on]")) { anyOn = true; }
  }
  pclose(p);
  if (found)
  {
    if (anyOff || (anyOn && !anyOff)) m_muted = anyOff;
    m_valid = true;
  }
  m_lastRefresh = time(nullptr);
}

void VolumeWidget::setPercent(int pct)
{
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  std::string device, control;
  resolveControl(device, control);
  std::string dp = device.empty() ? "" : ("-D " + device + " ");
  std::string cmd = "amixer " + dp + "sset " + control + " " + std::to_string(pct) + "%";
  std::system((cmd + " >/dev/null 2>&1").c_str());
  if (pct > 0 && m_muted)
  {
    std::string um = "amixer " + dp + "sset " + control + " unmute";
    std::system((um + " >/dev/null 2>&1").c_str());
  }
  struct timespec ts = {0, 40 * 1000 * 1000};
  nanosleep(&ts, nullptr);
  refresh();
}

void VolumeWidget::toggleMute()
{
  std::string device, control;
  resolveControl(device, control);
  std::string dp = device.empty() ? "" : ("-D " + device + " ");
  std::string cmd = "amixer " + dp + "set " + control + " toggle";
  std::system((cmd + " >/dev/null 2>&1").c_str());
  struct timespec ts = {0, 60 * 1000 * 1000};
  nanosleep(&ts, nullptr);
  refresh();
}

void VolumeWidget::draw(Display* d, Window panel, int x, int baseline)
{
  if (!m_valid) refresh();
  char buf[32];
  if (m_muted || m_percent <= 0)
    snprintf(buf, sizeof(buf), "\xf3\xb0\x96\x81");
  else
    snprintf(buf, sizeof(buf), "%d%%", m_percent);
  m_font.draw(d, m_xconn.screen(), panel, x, baseline, buf);
}

std::string VolumeWidget::tooltip() const
{
  return "Volume (click to open slider)";
}

bool VolumeWidget::tick()
{
  time_t now = time(nullptr);
  if (!m_valid || now - m_lastRefresh >= 3)
  {
    refresh();
    return true;
  }
  return false;
}

void VolumeWidget::showPopup(int screenX)
{
  Display* d = m_xconn.display();
  int sw = m_xconn.width();
  int sh = m_xconn.height();
  int px = screenX + 17 - kPopupW / 2;
  int py = sh - MewConst::panelHeight - kPopupH - 4;
  if (px < 0) px = 0;
  if (px + kPopupW > sw) px = sw - kPopupW;

  if (m_popup == None)
  {
    XSetWindowAttributes a{};
    a.override_redirect = True;
    a.background_pixel = 0x222222;
    a.event_mask = ExposureMask | ButtonPressMask | PointerMotionMask | ButtonReleaseMask;
    m_popup = XCreateWindow(d, m_xconn.root(), px, py, kPopupW, kPopupH, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask, &a);
  }
  else
  {
    XMoveResizeWindow(d, m_popup, px, py, kPopupW, kPopupH);
  }
  refresh();
  XMapRaised(d, m_popup);
  m_popupActive = true;
  XGrabKeyboard(d, m_popup, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  drawPopup();
}

void VolumeWidget::hidePopup()
{
  if (m_popup != None && m_popupActive)
  {
    XUnmapWindow(m_xconn.display(), m_popup);
    XUngrabKeyboard(m_xconn.display(), CurrentTime);
  }
  m_popupActive = false;
  m_dragging = false;
}

void VolumeWidget::drawPopup()
{
  if (m_popup == None || !m_popupActive) return;
  Display* d = m_xconn.display();
  int screen = m_xconn.screen();
  GC gc = XCreateGC(d, m_popup, 0, nullptr);

  XSetForeground(d, gc, 0x222222);
  XFillRectangle(d, m_popup, gc, 0, 0, kPopupW, kPopupH);
  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_popup, gc, 0, 0, kPopupW - 1, kPopupH - 1);

  const int barW = 10, barX = (kPopupW - barW) / 2;
  const int barTop = 15, barBottom = kPopupH - 60;
  const int barH = barBottom - barTop;
  XSetForeground(d, gc, 0x2a2a2a);
  XFillRectangle(d, m_popup, gc, barX, barTop, barW, barH);

  int pct = m_percent;
  unsigned long col = 0x22aa44;
  if (m_muted) col = 0x666666;
  else if (pct > 70) col = 0xcc2222;
  else if (pct > 30) col = 0xffcc44;

  int fh = barH * pct / 100;
  if (fh > 0)
  {
    XSetForeground(d, gc, col);
    XFillRectangle(d, m_popup, gc, barX, barBottom - fh, barW, fh);
  }

  XftFont* f = m_font.font();
  int asc = f ? f->ascent : 10;
  char buf[16];
  if (m_muted) snprintf(buf, sizeof(buf), "Mute");
  else         snprintf(buf, sizeof(buf), "%d%%", pct);
  m_font.setColor(0xffffff);
  m_font.draw(d, screen, m_popup, kPopupW / 2 - static_cast<int>(std::strlen(buf)) * 4, asc + 2, buf);

  const int bw = 32, bh = 26;
  int bx = (kPopupW - bw) / 2, by = kPopupH - 40;
  XSetForeground(d, gc, m_muted ? 0xcc2222 : 0x333333);
  XFillRectangle(d, m_popup, gc, bx, by, bw, bh);
  XSetForeground(d, gc, 0x666666);
  XDrawRectangle(d, m_popup, gc, bx, by, bw - 1, bh - 1);
  m_font.setColor(0xffffff);
  m_font.draw(d, screen, m_popup, bx + 12, by + 18, "\xf3\xb0\x96\x81");

  m_font.setColor(0xffffff);
  XFreeGC(d, gc);
}

bool VolumeWidget::handleLocalClick(int, int screenX)
{
  if (m_popupActive) hidePopup();
  else               showPopup(screenX);
  return true;
}

bool VolumeWidget::handleEscape()
{
  if (m_popupActive) { hidePopup(); return true; }
  return false;
}

bool VolumeWidget::handlePopupClick(XButtonEvent* e)
{
  if (!m_popupActive) return false;
  if (e->y >= kPopupH - 40 && e->y < kPopupH - 14)
  {
    toggleMute();
    drawPopup();
    return true;
  }
  const int barTop = 15, barBottom = kPopupH - 60;
  if (e->y >= barTop && e->y <= barBottom)
  {
    setPercent((barBottom - e->y) * 100 / (barBottom - barTop));
    drawPopup();
    m_dragging = true;
  }
  return true;
}

bool VolumeWidget::handlePopupMotion(XMotionEvent* e)
{
  if (!m_popupActive || !m_dragging) return false;
  if (!(e->state & Button1Mask)) return false;
  const int barTop = 15, barBottom = kPopupH - 60;
  if (e->y >= barTop && e->y <= barBottom)
  {
    setPercent((barBottom - e->y) * 100 / (barBottom - barTop));
    drawPopup();
  }
  return true;
}

static PanelWidget* createVolume(XConnection& x, FontRenderer& f) { return new VolumeWidget(x, f); }
static PanelWidgetRegistrar s_volume("volume", createVolume);
