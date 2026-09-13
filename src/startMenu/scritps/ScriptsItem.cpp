#include "ScriptsItem.hpp"
#include "../StartMenuRegistry.hpp"
#include "../../Util.hpp"

#include <X11/Xutil.h>
#include <dirent.h>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>

ScriptsItem::~ScriptsItem()
{
  hideSubmenu();
  if (m_submenu != None && m_ctx.pXconn)
  {
    XDestroyWindow(m_ctx.pXconn->display(), m_submenu);
    m_submenu = None;
  }
}

void ScriptsItem::loadScripts()
{
  m_scripts.clear();
  std::string dir = Util::getConfigDirectory() + "/scripts";
  DIR* d = opendir(dir.c_str());
  if (!d)
  {
    return;
  }
  struct dirent* ent = nullptr;
  while ((ent = readdir(d)) != nullptr)
  {
    std::string n = ent->d_name;
    if (n.size() > 3 && n.substr(n.size() - 3) == ".sh")
    {
      m_scripts.push_back(dir + "/" + n);
    }
  }
  closedir(d);
  std::sort(m_scripts.begin(), m_scripts.end());
}

bool ScriptsItem::onActivate(StartMenuContext& ctx,
                             int parentX, int parentY, int parentW)
{
  m_ctx = ctx;
  if (m_submenu != None)
  {
    hideSubmenu();
    return true;
  }
  loadScripts();
  if (m_scripts.empty())
  {
    return false;
  }
  openSubmenu(ctx.pXconn->display(), parentX + parentW, parentY);
  return true;
}

void ScriptsItem::openSubmenu(Display* d, int x, int y)
{
  int h = static_cast<int>(m_scripts.size()) * kRowH;
  if (m_submenu == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0x222222;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    m_submenu = XCreateWindow(
      d, DefaultRootWindow(d),
      x, y, kWidth, h, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_submenu, x, y, kWidth, h);
  }

  XMapRaised(d, m_submenu);
  drawSubmenu(d, DefaultScreen(d));
}

void ScriptsItem::drawSubmenu(Display* d, int screen)
{
  if (m_submenu == None || !m_ctx.pFont)
  {
    return;
  }
  int h = static_cast<int>(m_scripts.size()) * kRowH;
  GC gc = XCreateGC(d, m_submenu, 0, nullptr);
  XSetForeground(d, gc, 0x222222);
  XFillRectangle(d, m_submenu, gc, 0, 0, kWidth, h);
  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_submenu, gc, 0, 0, kWidth - 1, h - 1);

  XftFont* pFont = m_ctx.pFont->font();
  for (size_t i = 0; i < m_scripts.size(); ++i)
  {
    const std::string& path = m_scripts[i];
    size_t slash = path.find_last_of('/');
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    if (name.size() > 3)
    {
      name = name.substr(0, name.size() - 3);   // strip ".sh"
    }
    int yy = static_cast<int>(i) * kRowH;
    int bl = yy + (kRowH + (pFont ? pFont->ascent : 10)) / 2 - 2;
    m_ctx.pFont->setColor(0xffffff);
    m_ctx.pFont->draw(d, screen, m_submenu, 12, bl, name);
  }
  XFreeGC(d, gc);
}

bool ScriptsItem::handleSubmenuClick(int y)
{
  int idx = y / kRowH;
  if (idx < 0 || idx >= static_cast<int>(m_scripts.size()))
  {
    hideSubmenu();
    return true;
  }
  std::string cmd = m_scripts[static_cast<size_t>(idx)] + " >/dev/null 2>&1 &";
  std::system(cmd.c_str());
  hideSubmenu();
  return true;
}

void ScriptsItem::hideSubmenu()
{
  if (m_submenu != None && m_ctx.pXconn)
  {
    XUnmapWindow(m_ctx.pXconn->display(), m_submenu);
  }
}

static StartMenuItem* createScripts() { return new ScriptsItem(); }
static StartMenuItemRegistrar s_scripts("scripts", createScripts);
