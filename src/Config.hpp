#pragma once

#include <map>
#include <string>
#include <vector>
#include "Types.hpp"
#include <X11/Xlib.h>

/**
 * @brief Loads ~/.config/mew/config and keybindings.
 */
class Config
{
public:
  /**
   * @brief Load config file; resets background defaults first (last entry wins).
   */
  void load();

  /**
   * @brief Load keybindings file and resolve keycodes.
   * @param pDisplay Open Display used for XKeysymToKeycode.
   */
  void loadKeybindings(Display* pDisplay);

  /** @brief Title font pixel size. */
  double titleFontSize() const { return m_titleFontSize; }
  /** @brief Xcursor theme name. */
  const std::string& mouseTheme() const { return m_mouseTheme; }
  /** @brief Xcursor size. */
  int mouseSize() const { return m_mouseSize; }
  /** @brief Root background solid color. */
  unsigned long backgroundColor() const { return m_backgroundColor; }
  /** @brief Panel background color. */
  unsigned long panelColor() const { return m_panelColor; }
  /** @brief Default panel item (icon/text) color. */
  unsigned long panelItemColor() const { return m_panelItemColor; }
  /** @brief Panel item color while hovered. */
  unsigned long panelHoverColor() const { return m_panelHoverColor; }
  /** @brief Path to wallpaper image. */
  const std::string& backgroundImage() const { return m_backgroundImage; }
  /** @brief True if last background_* key in config was the image. */
  bool useBackgroundImage() const { return m_useBackgroundImage; }
  /** @brief Path to login WAV. */
  const std::string& loginSound() const { return m_loginSound; }
  /** @brief Path to logout WAV. */
  const std::string& logoutSound() const { return m_logoutSound; }
  /** @brief Reserved window theme name. */
  const std::string& windowTheme() const { return m_windowTheme; }
  /** @brief Parsed keybindings. */
  const std::vector<KeyBinding>& keybindings() const { return m_keybindings; }

  unsigned long activeBorderColor() const { return m_activeBorderColor; }
  unsigned long activeTitleColor() const { return m_activeTitleColor; }
  unsigned long inactiveBorderColor() const { return m_inactiveBorderColor; }
  unsigned long inactiveTitleColor() const { return m_inactiveTitleColor; }

  bool useEmbeddedBackground() const { return m_useEmbeddedBackground; }
  bool useEmbeddedSound() const { return m_useEmbeddedSound; }
  bool useEmbeddedLogoutSound() const { return m_useEmbeddedLogoutSound; }

  struct AppGeometry
  {
    bool hasX = false;
    bool hasY = false;
    bool hasW = false;
    bool hasH = false;
    bool maximized = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
  };

  /**
   * @brief Look up per-app geometry by WM_CLASS instance/class (lowercase).
   * @param appName Application name key from config (e.g. "mpv", "meld").
   * @return Pointer to geometry, or nullptr if unset.
   */
  const AppGeometry* appGeometry(const std::string& appName) const;

private:
  bool parseKeybinding(const std::string& line, std::string& key, std::string& command);
  bool parseKey(const std::string& keyString, unsigned int& modifiers, std::string& keyName);
  std::string expandKeyString(const std::string& keyString);

  double m_titleFontSize = 17.0;
  std::string m_mouseTheme;
  int m_mouseSize = 24;
  unsigned long m_backgroundColor = 0x222222;
  unsigned long m_panelColor = 0x222222;
  unsigned long m_panelItemColor = 0xffffff;
  unsigned long m_panelHoverColor = 0x0a64c8;
  std::string m_backgroundImage;
  bool m_useBackgroundImage = false;
  bool m_useEmbeddedBackground = true;
  bool m_useEmbeddedSound = true;
  bool m_useEmbeddedLogoutSound = true;
  std::string m_loginSound;
  std::string m_logoutSound;
  std::string m_windowTheme;
  std::vector<KeyBinding> m_keybindings;
  std::map<std::string, AppGeometry> m_appGeometry;

  unsigned long m_activeBorderColor = 0x333333;
  unsigned long m_activeTitleColor = 0x444444;
  unsigned long m_inactiveBorderColor = 0x1a1a1a;
  unsigned long m_inactiveTitleColor = 0x222222;
};
