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

  /** @brief Play the embedded login WAV asynchronously. */
  void playEmbeddedLogin();

  /** @brief Play the embedded logout WAV asynchronously. */
  void playEmbeddedLogout();

private:
  /**
   * @brief Play a 16-bit PCM WAV embedded in memory asynchronously.
   * @param pData Pointer to the WAV file bytes.
   * @param len Length of the data in bytes.
   */
  void playEmbedded(const unsigned char* pData, size_t len);
};
