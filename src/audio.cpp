#include "audio.h"

#include <vector>
#include <alsa/asoundlib.h>

namespace Mew
{
  void Audio::playSound(const std::string& path) const noexcept
  {
    if (path.empty()) {
      return;
    }
  
    pid_t pid = fork();
    if (pid != 0) {
      return; // parent continues
    }
  
    // --- child ---
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
      _exit(1);
    }
  
    // Parse RIFF/WAV header (minimal)
    char riff[12];
    if (fread(riff, 1, 12, f) != 12 || memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
      fclose(f);
      _exit(1);
    }
  
    uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0, dataSize = 0;
    long dataOffset = 0;
  
    while (true) {
      char chunkId[4];
      uint32_t chunkSize = 0;
      if (fread(chunkId, 1, 4, f) != 4 || fread(&chunkSize, 4, 1, f) != 1) {
        break;
      }
  
      if (memcmp(chunkId, "fmt ", 4) == 0) {
        uint16_t fmt, ch, bps;
        uint32_t sr;
        fread(&fmt, 2, 1, f);
        fread(&ch, 2, 1, f);
        fread(&sr, 4, 1, f);
        fseek(f, 6, SEEK_CUR); // skip byte rate + block align
        fread(&bps, 2, 1, f);
        audioFormat = fmt;
        numChannels = ch;
        sampleRate = sr;
        bitsPerSample = bps;
        if (chunkSize > 16) {
          fseek(f, chunkSize - 16, SEEK_CUR);
        }
      }
      else if (memcmp(chunkId, "data", 4) == 0) {
        dataSize = chunkSize;
        dataOffset = ftell(f);
        break;
      }
      else {
        fseek(f, chunkSize, SEEK_CUR);
      }
    }
  
    if (audioFormat != 1 || bitsPerSample != 16 || dataOffset == 0) {
      fclose(f);
      _exit(1);
    }
    fseek(f, dataOffset, SEEK_SET);
  
    snd_pcm_t* pcm = nullptr;
    if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
      fclose(f);
      _exit(1);
    }
  
    snd_pcm_set_params(pcm,
                       SND_PCM_FORMAT_S16_LE,
                       SND_PCM_ACCESS_RW_INTERLEAVED,
                       numChannels,
                       sampleRate,
                       1,          // soft resample
                       100000);    // 100 ms latency
  
    const size_t bufFrames = 1024;
    std::vector<char> buf(bufFrames * numChannels * 2);
    size_t remaining = dataSize;
  
    while (remaining > 0) {
      size_t toRead = std::min(remaining, buf.size());
      size_t n = fread(buf.data(), 1, toRead, f);
      if (n == 0) {
        break;
      }
      snd_pcm_sframes_t frames = n / (numChannels * 2);
      snd_pcm_sframes_t written = snd_pcm_writei(pcm, buf.data(), frames);
      if (written < 0) {
        snd_pcm_recover(pcm, (int)written, 0);
      }
      remaining -= n;
    }
  
    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
    fclose(f);
    _exit(0);
  }

  void Audio::updateVolume() noexcept
  {
    snd_mixer_t* handle = nullptr;
    if (snd_mixer_open(&handle, 0) < 0) {
      return;
    }
    if (snd_mixer_attach(handle, "default") < 0) {
      snd_mixer_close(handle);
      return;
    }
    if (snd_mixer_selem_register(handle, nullptr, nullptr) < 0) {
      snd_mixer_close(handle);
      return;
    }
    if (snd_mixer_load(handle) < 0) {
      snd_mixer_close(handle);
      return;
    }
  
    snd_mixer_selem_id_t* sid = nullptr;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, "Master");
  
    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
      // Try "PCM" as fallback
      snd_mixer_selem_id_set_name(sid, "PCM");
      elem = snd_mixer_find_selem(handle, sid);
    }
  
    if (elem) {
      if (snd_mixer_selem_has_playback_switch(elem)) {
        int muted = 0;
        snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &muted);
        m_bIsVolumeMuted = (muted == 0);
      } else {
        m_bIsVolumeMuted = false;
      }
  
      long minv = 0, maxv = 0, valv = 0;
      snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv);
      snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &valv);
      if (maxv > minv) {
        m_volumePercent = (int)(((valv - minv) * 100) / (maxv - minv));
      }
      else {
        m_volumePercent = 0;
      }
    }
    snd_mixer_close(handle);
  }
  
  void Audio::toggleMute() noexcept
  {
    snd_mixer_t* handle = nullptr;
    if (snd_mixer_open(&handle, 0) < 0) {
      return;
    }
    if (snd_mixer_attach(handle, "default") < 0) {
      snd_mixer_close(handle);
      return;
    }
    snd_mixer_selem_register(handle, nullptr, nullptr);
    snd_mixer_load(handle);
  
    snd_mixer_selem_id_t* sid = nullptr;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, "Master");
  
    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
      snd_mixer_selem_id_set_name(sid, "PCM");
      elem = snd_mixer_find_selem(handle, sid);
    }
  
    if (elem && snd_mixer_selem_has_playback_switch(elem)) {
      int muted = 0;
      snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &muted);
      int newState = muted ? 0 : 1; // toggle
      snd_mixer_selem_set_playback_switch_all(elem, newState);
    }
    snd_mixer_close(handle);
    updateVolume();
  }
}
