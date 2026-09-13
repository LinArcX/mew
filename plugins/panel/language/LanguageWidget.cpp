#include "LanguageWidget.hpp"
#include <X11/XKBlib.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

int LanguageWidget::width() const
{
  XftFont* pFont = m_font.font();
  if (!pFont)
  {
    return 30;
  }

  std::string name = m_name.empty() ? "??" : m_name;

  XGlyphInfo ext{};
  XftTextExtentsUtf8(
    m_xconn.display(),
    pFont,
    reinterpret_cast<const FcChar8*>(name.c_str()),
    static_cast<int>(name.size()),
    &ext);

  return ext.xOff + 16;
}

void LanguageWidget::refresh()
{
  XkbStateRec st{};
  if (XkbGetState(m_xconn.display(), XkbUseCoreKbd, &st) == Success)
    m_group = static_cast<int>(st.group);

  XkbDescPtr desc = XkbAllocKeyboard();
  if (!desc) return;
  desc->device_spec = XkbUseCoreKbd;
  if (XkbGetNames(m_xconn.display(), XkbGroupNamesMask, desc) != Success)
  {
    XkbFreeKeyboard(desc, 0, True);
    return;
  }
  m_count = 0;
  for (int i = 0; i < XkbNumKbdGroups; ++i)
    if (desc->names && desc->names->groups[i] != None) ++m_count;
  if (m_count < 1) m_count = 1;

  FILE* p = popen("setxkbmap -query 2>/dev/null", "r");
  if (p)
  {
    char line[256];
    while (fgets(line, sizeof(line), p))
    {
      if (std::strncmp(line, "layout:", 7) == 0)
      {
        std::string layouts = line + 7;
        size_t a = layouts.find_first_not_of(" \t\r\n");
        size_t b = layouts.find_last_not_of(" \t\r\n");
        if (a != std::string::npos)
        {
          layouts = layouts.substr(a, b - a + 1);
          std::vector<std::string> codes;
          std::string cur;
          for (char c : layouts)
          {
            if (c == ',') { if (!cur.empty()) { codes.push_back(cur); cur.clear(); } }
            else if (c != ' ') cur.push_back(c);
          }
          if (!cur.empty()) codes.push_back(cur);
          if (!codes.empty())
          {
            m_count = static_cast<int>(codes.size());
            int idx = std::clamp(m_group, 0, m_count - 1);
            m_name = codes[idx];
            for (char& c : m_name) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
          }
        }
        break;
      }
    }
    pclose(p);
  }
  XkbFreeKeyboard(desc, 0, True);
}

void LanguageWidget::draw(Display* d, Window panel, int x, int baseline)
{
  refresh();
  m_font.draw(d, m_xconn.screen(), panel, x, baseline, m_name);
}

bool LanguageWidget::handleLocalClick(int, int)
{
  refresh();
  int next = (m_group + 1) % m_count;
  XkbLockGroup(m_xconn.display(), XkbUseCoreKbd, static_cast<unsigned>(next));
  refresh();
  return true;
}

static PanelWidget* createLanguage(XConnection& x, FontRenderer& f) { return new LanguageWidget(x, f); }
static PanelWidgetRegistrar s_language("language", createLanguage);
