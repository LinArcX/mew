#pragma once

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

private:
  bool parseKeybinding(const std::string& line, std::string& key, std::string& command);
  bool parseKey(const std::string& keyString, unsigned int& modifiers, std::string& keyName);
  std::string expandKeyString(const std::string& keyString);

  double m_titleFontSize = 17.0;
  std::string m_mouseTheme;
  int m_mouseSize = 24;
  unsigned long m_backgroundColor = 0x425645;
  unsigned long m_panelColor = 0x222222;
  std::string m_backgroundImage;
  bool m_useBackgroundImage = false;
  std::string m_loginSound;
  std::string m_logoutSound;
  std::string m_windowTheme;
  std::vector<KeyBinding> m_keybindings;
};
