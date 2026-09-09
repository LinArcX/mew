#pragma once

#include <string>

/**
 * @brief Plays 16-bit PCM WAV files via ALSA in a forked child process.
 */
class Sound
{
public:
  /**
   * @brief Play a WAV file asynchronously (no-op if path empty or invalid).
   * @param path Filesystem path to a 16-bit PCM WAV.
   */
  void play(const std::string& path);
};
