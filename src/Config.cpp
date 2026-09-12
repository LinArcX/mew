#include "Config.hpp"
#include "Util.hpp"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

void Config::load()
{
  m_backgroundColor = 0x222222;
  m_backgroundImage.clear();
  m_useBackgroundImage = false;
  m_useEmbeddedBackground = true;
  m_appGeometry.clear();

  std::string path = Util::getConfigDirectory() + "/config";
  std::ifstream file(path);
  if (!file.is_open())
  {
    fprintf(stderr, "mew: no config file: %s (using defaults)\n", path.c_str());
    return;
  }

  std::string line;
  while (std::getline(file, line))
  {
    line = Util::trim(line);
    if (line.empty() || line[0] == '#')
    {
      continue;
    }

    size_t eq = line.find('=');
    if (eq == std::string::npos)
    {
      continue;
    }

    std::string key = Util::trim(line.substr(0, eq));
    std::string val = Util::trim(line.substr(eq + 1));

    if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
    {
      val = val.substr(1, val.size() - 2);
    }

    if (key == "title_font_size")
    {
      m_titleFontSize = std::atof(val.c_str());
      if (m_titleFontSize < 8.0)
      {
        m_titleFontSize = 8.0;
      }
    }
    else if (key == "mouse_theme")
    {
      m_mouseTheme = val;
    }
    else if (key == "mouse_size")
    {
      m_mouseSize = std::atoi(val.c_str());
      if (m_mouseSize < 8)
      {
        m_mouseSize = 8;
      }
    }
    else if (key == "background_color")
    {
      if (!val.empty() && val[0] == '#')
      {
        val = "0x" + val.substr(1);
      }
      m_backgroundColor = std::strtoul(val.c_str(), nullptr, 0);
      m_useBackgroundImage = false;
      m_useEmbeddedBackground = false;
    }
    else if (key == "panel_color")
    {
      if (!val.empty() && val[0] == '#')
      {
        val = "0x" + val.substr(1);
      }
      m_panelColor = std::strtoul(val.c_str(), nullptr, 0);
    }
    else if (key == "panel_item_color")
    {
      if (!val.empty() && val[0] == '#')
      {
        val = "0x" + val.substr(1);
      }
      m_panelItemColor = std::strtoul(val.c_str(), nullptr, 0);
    }
    else if (key == "panel_hover_color")
    {
      if (!val.empty() && val[0] == '#')
      {
        val = "0x" + val.substr(1);
      }
      m_panelHoverColor = std::strtoul(val.c_str(), nullptr, 0);
    }
    else if (key == "active_border_color")
    {
      if (!val.empty() && val[0] == '#')
      {
        val = "0x" + val.substr(1);
      }
      m_activeBorderColor = std::strtoul(val.c_str(), nullptr, 0);
    }
    else if (key == "active_title_color")
    {
      if (!val.empty() && val[0] == '#')
      {
        val = "0x" + val.substr(1);
      }
      m_activeTitleColor = std::strtoul(val.c_str(), nullptr, 0);
    }
    else if (key == "inactive_border_color")
    {
      if (!val.empty() && val[0] == '#')
      {
        val = "0x" + val.substr(1);
      }
      m_inactiveBorderColor = std::strtoul(val.c_str(), nullptr, 0);
    }
    else if (key == "inactive_title_color")
    {
      if (!val.empty() && val[0] == '#')
      {
        val = "0x" + val.substr(1);
      }
      m_inactiveTitleColor = std::strtoul(val.c_str(), nullptr, 0);
    }
    else if (key == "background_image")
    {
      m_backgroundImage = Util::expandHome(val);
      m_useBackgroundImage = true;
      m_useEmbeddedBackground = false;
    }
    else if (key == "login_sound")
    {
      m_loginSound = Util::expandHome(val);
    }
    else if (key == "logout_sound")
    {
      m_logoutSound = Util::expandHome(val);
    }
    else if (key == "window_theme")
    {
      m_windowTheme = val;
    }
    else
    {
      // Per-app geometry: app.pos.x / app.pos.y / app.width / app.height / app.maximized
      size_t dot1 = key.find('.');
      if (dot1 != std::string::npos && dot1 > 0)
      {
        std::string app = key.substr(0, dot1);
        std::string rest = key.substr(dot1 + 1);
        for (char& c : app)
        {
          if (c >= 'A' && c <= 'Z')
          {
            c = static_cast<char>(c + 32);
          }
        }
        AppGeometry& geo = m_appGeometry[app];
        if (rest == "pos.x")
        {
          geo.x = static_cast<int>(std::strtol(val.c_str(), nullptr, 10));
          geo.hasX = true;
        }
        else if (rest == "pos.y")
        {
          geo.y = static_cast<int>(std::strtol(val.c_str(), nullptr, 10));
          geo.hasY = true;
        }
        else if (rest == "width")
        {
          geo.width = static_cast<int>(std::strtol(val.c_str(), nullptr, 10));
          geo.hasW = true;
        }
        else if (rest == "height")
        {
          geo.height = static_cast<int>(std::strtol(val.c_str(), nullptr, 10));
          geo.hasH = true;
        }
        else if (rest == "maximized")
        {
          geo.maximized = (val == "true" || val == "1" || val == "yes");
        }
      }
    }
  }
}

std::string Config::expandKeyString(const std::string& keyString)
{
  std::stringstream ss(keyString);
  std::string part;
  std::vector<std::string> parts;
  while (std::getline(ss, part, '-'))
  {
    parts.push_back(part);
  }
  if (parts.empty())
  {
    return keyString;
  }

  std::string result;
  for (size_t i = 0; i + 1 < parts.size(); ++i)
  {
    const std::string& m = parts[i];
    std::string full;
    if (m == "W")
    {
      full = "Win";
    }
    else if (m == "A")
    {
      full = "Alt";
    }
    else if (m == "C")
    {
      full = "Control";
    }
    else if (m == "S")
    {
      full = "Shift";
    }
    else
    {
      full = m;
    }
    result += full + "-";
  }
  result += parts.back();
  return result;
}

bool Config::parseKeybinding(
  const std::string& line,
  std::string& key,
  std::string& command)
{
  size_t keyPos = line.find("key=");
  if (keyPos == std::string::npos)
  {
    return false;
  }

  size_t keyStart = line.find('"', keyPos);
  if (keyStart == std::string::npos)
  {
    return false;
  }

  size_t keyEnd = line.find('"', keyStart + 1);
  if (keyEnd == std::string::npos)
  {
    return false;
  }

  key = line.substr(keyStart + 1, keyEnd - keyStart - 1);

  size_t commandPos = line.find("command=", keyEnd);
  if (commandPos == std::string::npos)
  {
    return false;
  }

  size_t commandStart = line.find('"', commandPos);
  if (commandStart == std::string::npos)
  {
    return false;
  }

  size_t commandEnd = line.find('"', commandStart + 1);
  if (commandEnd == std::string::npos)
  {
    return false;
  }

  command = line.substr(commandStart + 1, commandEnd - commandStart - 1);
  return true;
}

bool Config::parseKey(
  const std::string& keyString,
  unsigned int& modifiers,
  std::string& keyName)
{
  modifiers = 0;
  std::stringstream ss(keyString);
  std::string part;
  std::vector<std::string> parts;
  while (std::getline(ss, part, '-'))
  {
    parts.push_back(part);
  }

  if (parts.empty())
  {
    return false;
  }

  for (size_t i = 0; i + 1 < parts.size(); ++i)
  {
    const std::string& modifier = parts[i];
    if (modifier == "W")
    {
      modifiers |= Mod4Mask;
    }
    else if (modifier == "A")
    {
      modifiers |= Mod1Mask;
    }
    else if (modifier == "C")
    {
      modifiers |= ControlMask;
    }
    else if (modifier == "S")
    {
      modifiers |= ShiftMask;
    }
    else
    {
      fprintf(stderr, "mew: unknown modifier '%s'\n", modifier.c_str());
      return false;
    }
  }

  keyName = parts.back();
  return true;
}

void Config::loadKeybindings(Display* pDisplay)
{
  m_keybindings.clear();
  if (!pDisplay)
  {
    return;
  }

  std::string path = Util::getConfigDirectory() + "/keybindings";
  std::ifstream file(path);
  if (!file.is_open())
  {
    fprintf(stderr, "mew: no keybindings file: %s\n", path.c_str());
    return;
  }

  std::string line;
  while (std::getline(file, line))
  {
    line = Util::trim(line);
    if (line.empty() || line[0] == '#')
    {
      continue;
    }

    std::string keyString;
    std::string command;
    if (!parseKeybinding(line, keyString, command))
    {
      fprintf(stderr, "mew: invalid keybinding: %s\n", line.c_str());
      continue;
    }

    unsigned int modifiers = 0;
    std::string keyName;
    if (!parseKey(keyString, modifiers, keyName))
    {
      continue;
    }

    KeySym keysym = XStringToKeysym(keyName.c_str());
    if (keysym == NoSymbol)
    {
      fprintf(stderr, "mew: unknown key: %s\n", keyName.c_str());
      continue;
    }

    KeyCode keycode = XKeysymToKeycode(pDisplay, keysym);
    if (keycode == 0)
    {
      fprintf(stderr, "mew: cannot find keycode: %s\n", keyName.c_str());
      continue;
    }

    KeyBinding binding;
    binding.keycode = keycode;
    binding.modifiers = modifiers;
    binding.command = Util::expandHome(command);
    binding.display = expandKeyString(keyString);
    m_keybindings.push_back(binding);

    printf("mew: keybinding %s -> %s\n", keyString.c_str(), binding.command.c_str());
  }
}


const Config::AppGeometry* Config::appGeometry(const std::string& appName) const
{
  std::string key = appName;
  for (char& c : key)
  {
    if (c >= 'A' && c <= 'Z')
    {
      c = static_cast<char>(c + 32);
    }
  }
  auto it = m_appGeometry.find(key);
  if (it == m_appGeometry.end())
  {
    return nullptr;
  }
  return &it->second;
}
