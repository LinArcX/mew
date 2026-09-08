#ifndef MEW_AUDIO_H
#define MEW_AUDIO_H

#include <string>

namespace Mew
{
  class Audio
  {
    public:
      /**
       * @brief Minimal WAV player (PCM, 16-bit).
       * Runs in a forked child so the WM stays responsive.
       *
       * @param path the address of .wav file
       */
      void playSound(const std::string& path) const noexcept;

      void toggleMute() noexcept;

      void updateVolume() noexcept;

      [[nodiscard]] int getVolumePercent() const noexcept { return m_volumePercent; }

      [[nodiscard]] bool isVolumeMuted() const noexcept { return m_bIsVolumeMuted; }

    private:
      /**
       * @brief acceptable range: 0-100
       *        -1 = unknown
       */
      int m_volumePercent = -1;

      bool m_bIsVolumeMuted = false;
  };
}

#endif // MEW_AUDIO_H
