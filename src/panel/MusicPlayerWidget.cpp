#include "MusicPlayerWidget.hpp"
#include "PanelWidgetRegistry.hpp"
#include "../Util.hpp"
#include "../symbols_font_data.h"

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

namespace
{
  double parseMpvDouble(const std::string& response)
  {
    size_t p = response.find("\"data\":");
    if (p == std::string::npos) return -1.0;
    p += 7;
    while (p < response.size() && (response[p] == ' ' || response[p] == '\t')) p++;
    if (p >= response.size() || response[p] == 'n') return -1.0;
    return std::atof(response.c_str() + p);
  }

  std::string mpvSend(const std::string& socketPath, const std::string& jsonCmd)
  {
    if (socketPath.empty()) return "";

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return "";

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 80000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
      close(fd);
      return "";
    }

    std::string req = jsonCmd + "\n";
    if (write(fd, req.c_str(), req.size()) < 0)
    {
      close(fd);
      return "";
    }

    char buf[1024] = {};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return "";
    return std::string(buf, static_cast<size_t>(n));
  }

  const char* kExts[] = {".mp3", ".flac", ".ogg", ".wav", ".m4a", ".opus", nullptr};

  bool hasAudioExt(const std::string& name)
  {
    for (int i = 0; kExts[i]; ++i)
    {
      size_t l = std::strlen(kExts[i]);
      if (name.size() >= l)
      {
        bool ok = true;
        for (size_t j = 0; j < l; ++j)
        {
          char a = name[name.size() - l + j];
          char b = kExts[i][j];
          if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + 32);
          if (a != b) { ok = false; break; }
        }
        if (ok) return true;
      }
    }
    return false;
  }

  void scanRecursive(const std::string& dir, std::vector<std::string>& out)
  {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* ent = nullptr;
    while ((ent = readdir(d)) != nullptr)
    {
      std::string n = ent->d_name;
      if (n == "." || n == "..") continue;
      std::string full = dir + "/" + n;
      struct stat st{};
      if (lstat(full.c_str(), &st) != 0) continue;
      if (S_ISDIR(st.st_mode))
      {
        scanRecursive(full, out);
      }
      else if (S_ISREG(st.st_mode) && hasAudioExt(n))
      {
        out.push_back(full);
      }
    }
    closedir(d);
  }

  std::string baseName(const std::string& path)
  {
    size_t p = path.find_last_of('/');
    return (p == std::string::npos) ? path : path.substr(p + 1);
  }

  std::string dirsFilePath()
  {
    return Util::getConfigDirectory() + "/music_dirs";
  }
}

MusicPlayerWidget::MusicPlayerWidget(XConnection& xconn, FontRenderer& font)
  : m_xconn(xconn)
  , m_font(font)
{
  m_iconFont.load(xconn.display(), xconn.screen(), symbols_ttf, symbols_ttf_len, 14.0);

  loadDirs();
  scanFiles();
}

MusicPlayerWidget::~MusicPlayerWidget()
{
  hidePopup();
  stopPlayback();
  if (m_popup != None && m_xconn.display())
  {
    XDestroyWindow(m_xconn.display(), m_popup);
    m_popup = None;
  }
}

void MusicPlayerWidget::configure(const Config& config)
{
  (void)config;
  m_noteColor = config.musicNoteColor();
  m_buttonColor = config.musicButtonColor();
  m_eqBars = config.musicEqBars();
  if (m_eqBars < 1) m_eqBars = 1;
  if (m_eqBars > 16) m_eqBars = 16;
  m_eqColors = config.musicEqColors();

  // Recompute width based on the actual bar count.
  m_computedWidth = kMusicIconW + 4 * kBtnW + 8 +
                    m_eqBars * (kEqBarW + kEqBarGap) + 8;
}

MusicPlayerWidget::Btn MusicPlayerWidget::buttonAt(int localX) const
{
  if (localX < 0 || localX >= m_computedWidth) return Btn::NoBtn;
  if (localX < kMusicIconW) return Btn::Note;
  int p = localX - kMusicIconW;
  int idx = p / kBtnW;
  if (idx == 0) return Btn::Prev;
  if (idx == 1) return Btn::Play;
  if (idx == 2) return Btn::Stop;
  if (idx == 3) return Btn::Next;
  return Btn::Label;
}

void MusicPlayerWidget::loadDirs()
{
  m_dirs.clear();
  std::ifstream f(dirsFilePath());
  if (f.is_open())
  {
    std::string line;
    while (std::getline(f, line))
    {
      line = Util::trim(line);
      if (!line.empty()) m_dirs.push_back(line);
    }
    return;
  }
  const char* home = getenv("HOME");
  if (home)
  {
    std::string music = std::string(home) + "/Music";
    struct stat st{};
    if (stat(music.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
    {
      m_dirs.push_back(music);
    }
  }
}

void MusicPlayerWidget::saveDirs()
{
  std::ofstream f(dirsFilePath());
  if (!f.is_open()) return;
  for (const std::string& d : m_dirs) f << d << '\n';
}

void MusicPlayerWidget::scanFiles()
{
  m_files.clear();
  for (const std::string& d : m_dirs) scanRecursive(d, m_files);
  if (m_current >= m_files.size()) m_current = 0;
}

pid_t spawnPlayer(const std::string& path, const std::string& socketPath)
{
  pid_t pid = fork();
  if (pid != 0)
  {
    return pid;
  }

  setsid();
  freopen("/dev/null", "r", stdin);
  freopen("/dev/null", "w", stdout);
  freopen("/dev/null", "w", stderr);

  std::string ipc = "--input-ipc-server=" + socketPath;
  execlp("mpv", "mpv",
         "--no-video",
         "--vo=null",
         "--no-terminal",
         "--really-quiet",
         "--audio-display=no",
         "--force-window=no",
         "--no-resume-playback",
         ipc.c_str(),
         path.c_str(), static_cast<char*>(nullptr));

  _exit(1);
}

void MusicPlayerWidget::playIndex(size_t idx)
{
  stopPlayback();
  if (m_files.empty()) return;
  if (idx >= m_files.size()) idx = 0;
  m_current = idx;

  m_mpvSocket = "/tmp/mew-mpv-" + std::to_string(static_cast<long>(getpid())) + ".sock";
  unlink(m_mpvSocket.c_str());

  m_playerPid = spawnPlayer(m_files[m_current], m_mpvSocket);
  m_paused = false;
  m_trackName = baseName(m_files[m_current]);
  m_playStart = time(nullptr);
}

void MusicPlayerWidget::stopPlayback()
{
  if (m_playerPid > 0)
  {
    kill(m_playerPid, SIGTERM);
    // Non-blocking: reap in tick(). If a previous pending kill exists,
    // do a quick non-blocking reap on it too.
    if (m_pendingKill > 0)
    {
      int status = 0;
      waitpid(m_pendingKill, &status, WNOHANG);
    }
    m_pendingKill = m_playerPid;
    m_playerPid = -1;
  }
  m_paused = false;
}

void MusicPlayerWidget::togglePause()
{
  if (m_playerPid <= 0)
  {
    // Nothing playing — start the current track.
    if (!m_files.empty())
    {
      playIndex(m_current);
    }
    return;
  }
  if (m_paused)
  {
    kill(m_playerPid, SIGCONT);
    m_paused = false;
  }
  else
  {
    kill(m_playerPid, SIGSTOP);
    m_paused = true;
  }
}

void MusicPlayerWidget::playNext()
{
  if (m_files.empty()) return;
  playIndex((m_current + 1) % m_files.size());
}

void MusicPlayerWidget::playPrev()
{
  if (m_files.empty()) return;
  playIndex((m_current == 0) ? m_files.size() - 1 : m_current - 1);
}

void MusicPlayerWidget::draw(Display* display, Window panel, int x, int baseline)
{
  int screen = m_xconn.screen();

  if (m_iconFont.font())
  {
    // Music note: opens the popup
    m_iconFont.setColor(m_noteColor);
    m_iconFont.draw(display, screen, panel, x + 4, baseline, "\xef\x80\x81");

    // Playback buttons
    m_iconFont.setColor(m_buttonColor);
    m_iconFont.draw(display, screen, panel, x + kMusicIconW + 4, baseline,
                    "\xef\x81\x88");
    m_iconFont.draw(display, screen, panel, x + kMusicIconW + kBtnW + 4, baseline,
                    m_paused ? "\xef\x81\x8b" : "\xef\x81\x8c");
    m_iconFont.draw(display, screen, panel, x + kMusicIconW + 2 * kBtnW + 4, baseline,
                    "\xef\x81\x8d");
    m_iconFont.draw(display, screen, panel, x + kMusicIconW + 3 * kBtnW + 4, baseline,
                    "\xef\x81\x91");
  }

  // Equalizer
  int eqX = x + kMusicIconW + 4 * kBtnW + 8;
  int eqBase = baseline + 2;
  GC gc = XCreateGC(display, panel, 0, nullptr);

  bool playing = (m_playerPid > 0 && !m_paused);
  for (int i = 0; i < m_eqBars; ++i)
  {
    int h;
    if (playing)
    {
      int phase = (m_eqFrame + i * 3) % 12;
      h = 2 + (phase < 6 ? phase : 12 - phase);
    }
    else
    {
      h = 2;
    }

    unsigned long c = m_eqColors.empty()
      ? m_buttonColor
      : m_eqColors[static_cast<size_t>(i) % m_eqColors.size()];

    XSetForeground(display, gc, c);
    int barX = eqX + i * (kEqBarW + kEqBarGap);
    XFillRectangle(display, panel, gc, barX, eqBase - h, kEqBarW, h);
  }

  XFreeGC(display, gc);
}

//void MusicPlayerWidget::draw(Display* display, Window panel, int x, int baseline)
//{
//  int screen = m_xconn.screen();
//
//  if (m_iconFont.font())
//  {
//    m_iconFont.setColor(m_iconColor);
//    // Music note: opens popup
//    m_iconFont.draw(display, screen, panel, x + 4, baseline, "\xef\x80\x81");
//    // Prev
//    m_iconFont.draw(display, screen, panel, x + kMusicIconW + 4, baseline,
//                    "\xef\x81\x88");
//    // Play / Pause
//    m_iconFont.draw(display, screen, panel, x + kMusicIconW + kBtnW + 4, baseline,
//                    m_paused ? "\xef\x81\x8b" : "\xef\x81\x8c");
//    // Stop
//    m_iconFont.draw(display, screen, panel, x + kMusicIconW + 2 * kBtnW + 4, baseline,
//                    "\xef\x81\x8d");
//    // Next
//    m_iconFont.draw(display, screen, panel, x + kMusicIconW + 3 * kBtnW + 4, baseline,
//                    "\xef\x81\x91");
//  }
//
//  // Equalizer
//  int eqX = x + kMusicIconW + 4 * kBtnW + 8;
//  int eqBase = baseline + 2;
//  GC gc = XCreateGC(display, panel, 0, nullptr);
//  XSetForeground(display, gc, m_iconColor);
//
//  bool playing = (m_playerPid > 0 && !m_paused);
//  for (int i = 0; i < kEqBars; ++i)
//  {
//    int h;
//    if (playing)
//    {
//      int phase = (m_eqFrame + i * 3) % 12;
//      h = 2 + (phase < 6 ? phase : 12 - phase);
//    }
//    else
//    {
//      h = 2;
//    }
//    int barX = eqX + i * (kEqBarW + kEqBarGap);
//    XFillRectangle(display, panel, gc, barX, eqBase - h, kEqBarW, h);
//  }
//
//  XFreeGC(display, gc);
//}

bool MusicPlayerWidget::tick()
{
  bool needRedraw = false;

  if (m_pendingKill > 0)
  {
    int status = 0;
    pid_t r = waitpid(m_pendingKill, &status, WNOHANG);
    if (r == m_pendingKill || r == -1)
    {
      m_pendingKill = -1;
    }
  }

  if (m_playerPid > 0 && !m_paused)
  {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long long ms = static_cast<long long>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
    if (ms - m_lastQueryMs >= 500)
    {
      m_lastQueryMs = ms;
      m_position = queryMpv("time-pos");
      m_duration = queryMpv("duration");
      needRedraw = true;
    }
    if (ms - m_lastEqMs >= 100)
    {
      m_lastEqMs = ms;
      m_eqFrame++;
      needRedraw = true;
    }
  }

  if (m_playerPid <= 0)
  {
    return needRedraw;
  }

  int status = 0;
  pid_t r = waitpid(m_playerPid, &status, WNOHANG);
  if (r != m_playerPid)
  {
    return needRedraw;
  }

  m_playerPid = -1;
  m_paused = false;

  if (m_playStart == 0 || (time(nullptr) - m_playStart) >= 1)
  {
    playNext();
    needRedraw = true;
  }
  return needRedraw;
}

//bool MusicPlayerWidget::tick()
//{
//  if (m_pendingKill > 0)
//  {
//    int status = 0;
//    pid_t r = waitpid(m_pendingKill, &status, WNOHANG);
//    if (r == m_pendingKill || r == -1)
//    {
//      m_pendingKill = -1;
//    }
//  }
//  if (m_playerPid > 0 && !m_paused)
//  {
//    struct timespec ts;
//    clock_gettime(CLOCK_MONOTONIC, &ts);
//    long long ms = static_cast<long long>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
//    if (ms - m_lastQueryMs >= 500)
//    {
//      m_lastQueryMs = ms;
//      m_position = queryMpv("time-pos");
//      m_duration = queryMpv("duration");
//      needRedraw = true;
//    }
//  }
//
//
//  bool needRedraw = false;
//
//  if (m_playerPid > 0 && !m_paused)
//  {
//    struct timespec ts;
//    clock_gettime(CLOCK_MONOTONIC, &ts);
//    long long ms = static_cast<long long>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
//    if (ms - m_lastEqMs >= 100)
//    {
//      m_lastEqMs = ms;
//      m_eqFrame++;
//      needRedraw = true;
//    }
//  }
//
//  if (m_playerPid <= 0)
//  {
//    return needRedraw;
//  }
//
//  int status = 0;
//  pid_t r = waitpid(m_playerPid, &status, WNOHANG);
//  if (r != m_playerPid)
//  {
//    return needRedraw;
//  }
//
//  m_playerPid = -1;
//  m_paused = false;
//
//  if (m_playStart == 0 || (time(nullptr) - m_playStart) >= 1)
//  {
//    playNext();
//    needRedraw = true;
//  }
//  return needRedraw;
//}

bool MusicPlayerWidget::onClick(int screenX)
{
  (void)screenX;
  // The caller passes the widget's left edge; but we need localX.
  // PanelWidget interface doesn't give us localX, so we ask Panel to route
  // raw x through handleClick -> we get it via popupWindow focus instead.
  // For simplicity: this widget expects Panel to have already computed
  // localX and dispatched to buttonAt(). See Panel::handleClick().
  return false;
}

std::string MusicPlayerWidget::tooltip() const
{
  if (m_playerPid <= 0 && m_trackName.empty())
  {
    return "Music player (no track)";
  }
  if (m_paused)
  {
    return "Paused: " + m_trackName;
  }
  return m_trackName;
}

bool MusicPlayerWidget::handleEscape()
{
  if (m_seekPopupActive)
  {
    hideSeekPopup();
    return true;
  }
  if (m_popupActive)
  {
    hidePopup();
    return true;
  }
  return false;
}

void MusicPlayerWidget::showPopup(int screenX)
{
  Display* d = m_xconn.display();
  if (m_dirs.empty() && m_files.empty())
  {
    scanFiles();
  }

  m_popupDirCount = static_cast<int>(m_dirs.size());
  int visible = m_popupDirCount;
  if (visible > kPopupMaxRows) visible = kPopupMaxRows;

  m_popupW = kPopupWidth;
  m_popupH = kPopupPad * 2 + kPopupRowH * (visible + 2) + 4;

  int screenW = m_xconn.width();
  int screenH = m_xconn.height();
  int px = screenX + m_computedWidth - m_popupW;
  int py = screenH - MewConst::panelHeight - m_popupH - 4;
  if (px < 0) px = 0;
  if (px + m_popupW > screenW) px = screenW - m_popupW;

  if (m_popup == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0x1e1e1e;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    m_popup = XCreateWindow(
      d, m_xconn.root(),
      px, py, m_popupW, m_popupH, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_popup, px, py, m_popupW, m_popupH);
  }

  XMapRaised(d, m_popup);
  m_popupActive = true;
  m_inputBuffer.clear();

  XGrabKeyboard(d, m_popup, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  m_escGrabbed = true;

  drawPopup();
}

void MusicPlayerWidget::hidePopup()
{
  if (m_popup != None && m_popupActive)
  {
    XUnmapWindow(m_xconn.display(), m_popup);
  }
  if (m_escGrabbed)
  {
    XUngrabKeyboard(m_xconn.display(), CurrentTime);
    m_escGrabbed = false;
  }
  m_popupActive = false;
}

void MusicPlayerWidget::drawPopup()
{
  if (m_popup == None || !m_popupActive) return;

  Display* d = m_xconn.display();
  int screen = m_xconn.screen();
  GC gc = XCreateGC(d, m_popup, 0, nullptr);

  XSetForeground(d, gc, 0x1e1e1e);
  XFillRectangle(d, m_popup, gc, 0, 0, m_popupW, m_popupH);
  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_popup, gc, 0, 0, m_popupW - 1, m_popupH - 1);

  XftFont* pFont = m_font.font();
  int ascent = pFont ? pFont->ascent : 10;
  int y = kPopupPad + ascent;

  m_font.setColor(0xffffff);
  m_font.draw(d, screen, m_popup, kPopupPad + 4, y, "Music directories");
  y += kPopupRowH;

  size_t start = 0;
  if (m_dirs.size() > static_cast<size_t>(kPopupMaxRows))
  {
    start = m_dirs.size() - kPopupMaxRows;
  }

  for (size_t i = start; i < m_dirs.size(); ++i)
  {
    m_font.setColor(0xcccccc);
    m_font.draw(d, screen, m_popup, kPopupPad + 4, y, m_dirs[i]);

    // Trash icon (fa-trash-o U+F014) at the right edge.
    if (m_iconFont.font())
    {
      m_iconFont.setColor(0xcc4444);
      m_iconFont.draw(d, screen, m_popup, m_popupW - 24, y, "\xef\x80\x94");
    }

    y += kPopupRowH;
  }

  if (m_dirs.empty())
  {
    m_font.setColor(0x888888);
    m_font.draw(d, screen, m_popup, kPopupPad + 4, y, "(none)");
    y += kPopupRowH;
  }

  XSetForeground(d, gc, 0x2a2a2a);
  XFillRectangle(d, m_popup, gc, kPopupPad, y - ascent - 2,
                 m_popupW - kPopupPad * 2, kPopupRowH);

  std::string prompt = "> " + m_inputBuffer + "_";
  m_font.setColor(0xffffff);
  m_font.draw(d, screen, m_popup, kPopupPad + 4, y, prompt);

  XFreeGC(d, gc);
}

//void MusicPlayerWidget::drawPopup()
//{
//  if (m_popup == None || !m_popupActive) return;
//
//  Display* d = m_xconn.display();
//  int screen = m_xconn.screen();
//  GC gc = XCreateGC(d, m_popup, 0, nullptr);
//
//  XSetForeground(d, gc, 0x1e1e1e);
//  XFillRectangle(d, m_popup, gc, 0, 0, m_popupW, m_popupH);
//  XSetForeground(d, gc, 0x555555);
//  XDrawRectangle(d, m_popup, gc, 0, 0, m_popupW - 1, m_popupH - 1);
//
//  XftFont* pFont = m_font.font();
//  int ascent = pFont ? pFont->ascent : 10;
//  int y = kPopupPad + ascent;
//
//  m_font.setColor(0xffffff);
//  m_font.draw(d, screen, m_popup, kPopupPad + 4, y, "Music directories");
//  y += kPopupRowH;
//
//  size_t start = 0;
//  if (m_dirs.size() > static_cast<size_t>(kPopupMaxRows))
//  {
//    start = m_dirs.size() - kPopupMaxRows;
//  }
//  for (size_t i = start; i < m_dirs.size(); ++i)
//  {
//    m_font.setColor(0xcccccc);
//    m_font.draw(d, screen, m_popup, kPopupPad + 4, y, m_dirs[i]);
//    y += kPopupRowH;
//  }
//
//  if (m_dirs.empty())
//  {
//    m_font.setColor(0x888888);
//    m_font.draw(d, screen, m_popup, kPopupPad + 4, y, "(none)");
//    y += kPopupRowH;
//  }
//
//  // Input row
//  XSetForeground(d, gc, 0x2a2a2a);
//  XFillRectangle(d, m_popup, gc, kPopupPad, y - ascent - 2,
//                 m_popupW - kPopupPad * 2, kPopupRowH);
//
//  std::string prompt = "> " + m_inputBuffer + "_";
//  m_font.setColor(0xffffff);
//  m_font.draw(d, screen, m_popup, kPopupPad + 4, y, prompt);
//
//  XFreeGC(d, gc);
//}

bool MusicPlayerWidget::handlePopupKey(XKeyEvent* pEvent)
{
  if (!pEvent || !m_popupActive) return false;

  KeySym sym = XLookupKeysym(pEvent, 0);
  char buf[8] = {};
  XLookupString(pEvent, buf, sizeof(buf) - 1, &sym, nullptr);

  if (sym == XK_Escape)
  {
    hidePopup();
    return true;
  }
  if (sym == XK_Return)
  {
    std::string dir = Util::trim(m_inputBuffer);
    if (!dir.empty())
    {
      struct stat st{};
      if (stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
      {
        m_dirs.push_back(dir);
        saveDirs();
        scanFiles();
      }
    }
    m_inputBuffer.clear();
    drawPopup();
    return true;
  }
  if (sym == XK_BackSpace)
  {
    if (!m_inputBuffer.empty())
    {
      m_inputBuffer.pop_back();
      drawPopup();
    }
    return true;
  }
  if (buf[0] >= 32 && buf[0] < 127)
  {
    m_inputBuffer.push_back(buf[0]);
    drawPopup();
    return true;
  }
  return true;
}

bool MusicPlayerWidget::handlePopupMotion(XMotionEvent* pEvent)
{
  if (!pEvent || !m_seekPopupActive || !m_seekDragging)
  {
    return false;
  }
  const int barX = 12;
  const int barW = kSeekPopupW - 24;
  if (m_duration > 0 && pEvent->x >= barX && pEvent->x <= barX + barW)
  {
    double frac = static_cast<double>(pEvent->x - barX) / barW;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    m_position = frac * m_duration;
    drawSeekPopup();
  }
  return true;
}

void MusicPlayerWidget::commitSeek()
{
  if (m_seekPopupActive && m_seekDragging && m_duration > 0)
  {
    sendMpvSeek(m_position);
  }
  m_seekDragging = false;
}

bool MusicPlayerWidget::handlePopupClick(XButtonEvent* pEvent)
{
  if (!pEvent) return false;

  if (m_seekPopupActive && pEvent->window == m_seekPopup)
  {
    const int barX = 12;
    const int barW = kSeekPopupW - 24;
    if (m_duration > 0 && pEvent->x >= barX && pEvent->x <= barX + barW)
    {
      double frac = static_cast<double>(pEvent->x - barX) / barW;
      if (frac < 0) frac = 0;
      if (frac > 1) frac = 1;
      m_position = frac * m_duration;
      drawSeekPopup();
      m_seekDragging = true;
    }
    return true;
  }

  if (!m_popupActive) return false;

  // Rows start right after the header row.
  int ascent = m_font.font() ? m_font.font()->ascent : 10;
  int rowStart = kPopupPad + ascent + kPopupRowH;

  int rel = pEvent->y - rowStart;
  if (rel < 0)
  {
    return false;
  }

  int rowIdx = rel / kPopupRowH;

  size_t start = 0;
  if (m_dirs.size() > static_cast<size_t>(kPopupMaxRows))
  {
    start = m_dirs.size() - kPopupMaxRows;
  }
  size_t absIdx = start + static_cast<size_t>(rowIdx);

  if (absIdx >= m_dirs.size())
  {
    return false;
  }

  // Only remove if the click landed on the trash icon area (right side).
  if (pEvent->x < m_popupW - 28)
  {
    return false;
  }

  m_dirs.erase(m_dirs.begin() + static_cast<long>(absIdx));
  saveDirs();
  scanFiles();
  drawPopup();
  return true;
}

double MusicPlayerWidget::queryMpv(const char* property) const
{
  if (m_playerPid <= 0 || m_mpvSocket.empty()) return -1.0;
  std::string cmd = std::string("{\"command\":[\"get_property\",\"") + property + "\"]}";
  std::string resp = mpvSend(m_mpvSocket, cmd);
  return parseMpvDouble(resp);
}

bool MusicPlayerWidget::sendMpvSeek(double seconds) const
{
  if (m_playerPid <= 0 || m_mpvSocket.empty()) return false;
  char buf[128];
  std::snprintf(buf, sizeof(buf),
    "{\"command\":[\"seek\",%.3f,\"absolute\"]}", seconds);
  std::string resp = mpvSend(m_mpvSocket, buf);
  return resp.find("\"error\":\"success\"") != std::string::npos;
}

void MusicPlayerWidget::showSeekPopup(int screenX)
{
  Display* d = m_xconn.display();

  int screenW = m_xconn.width();
  int screenH = m_xconn.height();
  int px = screenX + m_computedWidth / 2 - kSeekPopupW / 2;
  int py = screenH - MewConst::panelHeight - kSeekPopupH - 4;
  if (px < 0) px = 0;
  if (px + kSeekPopupW > screenW) px = screenW - kSeekPopupW;

  if (m_seekPopup == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0x1e1e1e;
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

    m_seekPopup = XCreateWindow(
      d, m_xconn.root(),
      px, py, kSeekPopupW, kSeekPopupH, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_seekPopup, px, py, kSeekPopupW, kSeekPopupH);
  }

  XMapRaised(d, m_seekPopup);
  m_seekPopupActive = true;
  m_seekDragging = false;
  drawSeekPopup();
}

void MusicPlayerWidget::hideSeekPopup()
{
  if (m_seekPopup != None && m_seekPopupActive)
  {
    XUnmapWindow(m_xconn.display(), m_seekPopup);
  }
  m_seekPopupActive = false;
  m_seekDragging = false;
}

void MusicPlayerWidget::drawSeekPopup()
{
  if (m_seekPopup == None || !m_seekPopupActive) return;

  Display* d = m_xconn.display();
  int screen = m_xconn.screen();
  GC gc = XCreateGC(d, m_seekPopup, 0, nullptr);

  XSetForeground(d, gc, 0x1e1e1e);
  XFillRectangle(d, m_seekPopup, gc, 0, 0, kSeekPopupW, kSeekPopupH);
  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_seekPopup, gc, 0, 0, kSeekPopupW - 1, kSeekPopupH - 1);

  const int barX = 12;
  const int barW = kSeekPopupW - 24;
  const int barY = 20;
  const int barH = 8;

  double pos = m_position < 0 ? 0 : m_position;
  double dur = m_duration <= 0 ? 1 : m_duration;
  if (pos > dur) pos = dur;

  int fillW = static_cast<int>((pos / dur) * barW);

  XSetForeground(d, gc, 0x2a2a2a);
  XFillRectangle(d, m_seekPopup, gc, barX, barY, barW, barH);

  XSetForeground(d, gc, m_noteColor);
  XFillRectangle(d, m_seekPopup, gc, barX, barY, fillW, barH);

  XSetForeground(d, gc, 0xffffff);
  XFillRectangle(d, m_seekPopup, gc,
                 barX + fillW - 2, barY - 3, 4, barH + 6);

  auto fmtTime = [](double s) -> std::string {
    if (s < 0) s = 0;
    int total = static_cast<int>(s);
    int m = total / 60;
    int sec = total % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", m, sec);
    return buf;
  };

  m_font.setColor(0xaaaaaa);
  m_font.draw(d, screen, m_seekPopup, barX, kSeekPopupH - 10, fmtTime(pos));

  std::string durStr = fmtTime(dur);
  m_font.draw(d, screen, m_seekPopup,
              barX + barW - static_cast<int>(durStr.size()) * 9,
              kSeekPopupH - 10, durStr);

  XFreeGC(d, gc);
}

static PanelWidget* createMusic(XConnection& xconn, FontRenderer& font)
{
  return new MusicPlayerWidget(xconn, font);
}

static PanelWidgetRegistrar s_musicRegistrar("music", createMusic);
