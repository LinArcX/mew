#include "Sound.hpp"
#include "login_wav_data.h"
#include "logout_wav_data.h"

#include <alsa/asoundlib.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>

void Sound::play(const std::string& path)
{
  if (path.empty())
  {
    return;
  }

  pid_t pid = fork();
  if (pid != 0)
  {
    return;
  }

  FILE* f = fopen(path.c_str(), "rb");
  if (!f)
  {
    _exit(1);
  }

  char riff[12];
  if (fread(riff, 1, 12, f) != 12 ||
      memcmp(riff, "RIFF", 4) != 0 ||
      memcmp(riff + 8, "WAVE", 4) != 0)
  {
    fclose(f);
    _exit(1);
  }

  uint16_t audioFormat = 0;
  uint16_t numChannels = 0;
  uint16_t bitsPerSample = 0;
  uint32_t sampleRate = 0;
  uint32_t dataSize = 0;
  long dataOffset = 0;

  while (true)
  {
    char chunkId[4];
    uint32_t chunkSize = 0;
    if (fread(chunkId, 1, 4, f) != 4 || fread(&chunkSize, 4, 1, f) != 1)
    {
      break;
    }

    if (memcmp(chunkId, "fmt ", 4) == 0)
    {
      uint16_t fmt = 0;
      uint16_t ch = 0;
      uint16_t bps = 0;
      uint32_t sr = 0;
      fread(&fmt, 2, 1, f);
      fread(&ch, 2, 1, f);
      fread(&sr, 4, 1, f);
      fseek(f, 6, SEEK_CUR);
      fread(&bps, 2, 1, f);
      audioFormat = fmt;
      numChannels = ch;
      sampleRate = sr;
      bitsPerSample = bps;
      if (chunkSize > 16)
      {
        fseek(f, chunkSize - 16, SEEK_CUR);
      }
    }
    else if (memcmp(chunkId, "data", 4) == 0)
    {
      dataSize = chunkSize;
      dataOffset = ftell(f);
      break;
    }
    else
    {
      fseek(f, chunkSize, SEEK_CUR);
    }
  }

  if (audioFormat != 1 || bitsPerSample != 16 || dataOffset == 0)
  {
    fclose(f);
    _exit(1);
  }

  fseek(f, dataOffset, SEEK_SET);

  snd_pcm_t* pcm = nullptr;
  if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
  {
    fclose(f);
    _exit(1);
  }

  snd_pcm_set_params(
    pcm,
    SND_PCM_FORMAT_S16_LE,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    numChannels,
    sampleRate,
    1,
    100000);

  const size_t bufFrames = 1024;
  std::vector<char> buf(bufFrames * numChannels * 2);
  size_t remaining = dataSize;

  while (remaining > 0)
  {
    size_t toRead = std::min(remaining, buf.size());
    size_t n = fread(buf.data(), 1, toRead, f);
    if (n == 0)
    {
      break;
    }
    snd_pcm_sframes_t frames = static_cast<snd_pcm_sframes_t>(n / (numChannels * 2));
    snd_pcm_sframes_t written = snd_pcm_writei(pcm, buf.data(), frames);
    if (written < 0)
    {
      snd_pcm_recover(pcm, static_cast<int>(written), 0);
    }
    remaining -= n;
  }

  snd_pcm_drain(pcm);
  snd_pcm_close(pcm);
  fclose(f);
  _exit(0);
}

void Sound::playEmbedded(const unsigned char* pData, size_t len)
{
  if (!pData || len < 44)
  {
    return;
  }

  pid_t pid = fork();
  if (pid != 0)
  {
    return;
  }

  // Parse WAV header from memory
  if (memcmp(pData, "RIFF", 4) != 0 || memcmp(pData + 8, "WAVE", 4) != 0)
  {
    _exit(1);
  }

  uint16_t audioFormat = 0;
  uint16_t numChannels = 0;
  uint16_t bitsPerSample = 0;
  uint32_t sampleRate = 0;
  uint32_t dataSize = 0;
  size_t dataOffset = 0;

  size_t pos = 12;
  while (pos + 8 <= len)
  {
    uint32_t chunkSize = 0;
    memcpy(&chunkSize, pData + pos + 4, 4);

    if (memcmp(pData + pos, "fmt ", 4) == 0 && pos + 8 + 16 <= len)
    {
      uint16_t fmt = 0;
      uint16_t ch = 0;
      uint16_t bps = 0;
      uint32_t sr = 0;
      memcpy(&fmt, pData + pos + 8, 2);
      memcpy(&ch, pData + pos + 10, 2);
      memcpy(&sr, pData + pos + 12, 4);
      memcpy(&bps, pData + pos + 22, 2);
      audioFormat = fmt;
      numChannels = ch;
      sampleRate = sr;
      bitsPerSample = bps;
    }
    else if (memcmp(pData + pos, "data", 4) == 0)
    {
      dataSize = chunkSize;
      dataOffset = pos + 8;
      break;
    }

    pos += 8 + chunkSize;
    if (chunkSize % 2 != 0)
    {
      pos++;
    }
  }

  if (audioFormat != 1 || bitsPerSample != 16 || dataOffset == 0 || dataOffset + dataSize > len)
  {
    _exit(1);
  }

  snd_pcm_t* pcm = nullptr;
  if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
  {
    _exit(1);
  }

  snd_pcm_set_params(
    pcm,
    SND_PCM_FORMAT_S16_LE,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    numChannels,
    sampleRate,
    1,
    100000);

  const size_t bufFrames = 1024;
  size_t frameSize = numChannels * 2;
  std::vector<char> buf(bufFrames * frameSize);
  size_t remaining = dataSize;
  const unsigned char* p = pData + dataOffset;

  while (remaining > 0)
  {
    size_t toRead = std::min(remaining, buf.size());
    memcpy(buf.data(), p, toRead);
    p += toRead;
    remaining -= toRead;

    snd_pcm_sframes_t frames = static_cast<snd_pcm_sframes_t>(toRead / frameSize);
    snd_pcm_sframes_t written = snd_pcm_writei(pcm, buf.data(), frames);
    if (written < 0)
    {
      snd_pcm_recover(pcm, static_cast<int>(written), 0);
    }
  }

  snd_pcm_drain(pcm);
  snd_pcm_close(pcm);
  _exit(0);
}

void Sound::playEmbeddedLogin()
{
  playEmbedded(login_wav, login_wav_len);
}

void Sound::playEmbeddedLogout()
{
  playEmbedded(logout_wav, logout_wav_len);
}
