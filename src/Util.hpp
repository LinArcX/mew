#pragma once

#include <string>

namespace Util
{
  /** @brief Trim leading/trailing whitespace. */
  std::string trim(const std::string& str);

  /** @brief Expand leading ~ or ~/ using $HOME. */
  std::string expandHome(const std::string& path);

  /** @brief Return $HOME/.config/mew. */
  std::string getConfigDirectory();

  /** @brief Create ~/.config/mew if missing. */
  void createConfigDirectory();

  /** @brief Path to the pid file (/tmp/mew.pid). */
  std::string getPidfile();

  /** @brief Write current pid to the pid file. */
  void writePidfile();

  /** @brief Remove the pid file. */
  void removePidfile();
}
