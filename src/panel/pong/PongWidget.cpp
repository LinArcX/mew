#include "PongWidget.hpp"
#include "../PanelWidgetRegistry.hpp"
#include "../../Util.hpp"

#include "player_wav_data.h"
#include "opponent_wav_data.h"
#include "win_wav_data.h"
#include "game_over_wav_data.h"

#include <sys/wait.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <unistd.h>

std::string findSoundInDir(const std::string& dir, const std::string& base)
{
  const char* exts[] = { ".wav", ".ogg", ".mp3", ".flac", nullptr };
  for (int i = 0; exts[i]; ++i)
  {
    std::string p = dir + "/" + base + exts[i];
    if (access(p.c_str(), R_OK) == 0)
    {
      return p;
    }
  }
  return "";
}

PongWidget::PongWidget(XConnection& xconn, FontRenderer& font)
  : m_xconn(xconn)
  , m_font(font)
{
  loadDifficulty();
  resetBall(1);
  initSounds();
  loadSoundEnabled();
}

PongWidget::~PongWidget()
{
  releaseKeyboard();
  if (m_settingsPopup != None && m_xconn.display())
  {
    XDestroyWindow(m_xconn.display(), m_settingsPopup);
    m_settingsPopup = None;
  }
}

void PongWidget::configure(const Config& config)
{
  (void)config;
}

void PongWidget::initSounds()
{
  m_soundDir = Util::getConfigDirectory() + "/pong_sounds";
  m_hitSound       = findSoundInDir(m_soundDir, "hit");
  m_wonRoundSound  = findSoundInDir(m_soundDir, "won_round");
  m_lostRoundSound = findSoundInDir(m_soundDir, "lost_round");
  m_wonGameSound   = findSoundInDir(m_soundDir, "won_game");
  m_lostGameSound  = findSoundInDir(m_soundDir, "lost_game");
}

void PongWidget::loadSoundEnabled()
{
  std::string path = Util::getConfigDirectory() + "/pong_sound_enabled";
  std::ifstream f(path);
  int v = 1;
  if (f >> v)
  {
    m_soundEnabled = (v != 0);
  }
}

void PongWidget::saveSoundEnabled()
{
  std::string path = Util::getConfigDirectory() + "/pong_sound_enabled";
  std::ofstream f(path);
  if (f)
  {
    f << (m_soundEnabled ? 1 : 0) << '\n';
  }
}

void PongWidget::playSound(const std::string& path)
{
  if (!m_soundEnabled || path.empty())
  {
    return;
  }

  pid_t pid = fork();
  if (pid != 0)
  {
    return;
  }

  setsid();
  freopen("/dev/null", "r", stdin);
  freopen("/dev/null", "w", stdout);
  freopen("/dev/null", "w", stderr);

  // Low-latency ALSA first, then PulseAudio, then mpv as a fallback.
  execlp("aplay", "aplay", "-q", path.c_str(),
         static_cast<char*>(nullptr));
  execlp("paplay", "paplay", path.c_str(),
         static_cast<char*>(nullptr));
  execlp("mpv", "mpv",
         "--no-video", "--vo=null", "--no-terminal",
         "--really-quiet", "--force-window=no",
         "--no-resume-playback",
         path.c_str(), static_cast<char*>(nullptr));
  _exit(1);
}

void PongWidget::finishGame(bool playerWon)
{
  m_gameOver = true;
  m_gameOverAt = time(nullptr);
  playSound(playerWon ? m_wonGameSound : m_lostGameSound);
}

void PongWidget::loadDifficulty()
{
  std::string path = Util::getConfigDirectory() + "/pong_difficulty";
  std::ifstream f(path);
  int d = 1;
  if (f >> d && d >= 0 && d <= 2)
  {
    m_difficulty = static_cast<Difficulty>(d);
  }
}

void PongWidget::saveDifficulty()
{
  std::string path = Util::getConfigDirectory() + "/pong_difficulty";
  std::ofstream f(path);
  if (f)
  {
    f << static_cast<int>(m_difficulty) << '\n';
  }
}

double PongWidget::ballSpeedForDifficulty() const
{
  switch (m_difficulty)
  {
    case Difficulty::Easy:   return 45.0;
    case Difficulty::Medium: return 70.0;
    case Difficulty::Hard:   return 100.0;
  }
  return 70.0;
}

double PongWidget::aiSpeedForDifficulty() const
{
  switch (m_difficulty)
  {
    case Difficulty::Easy:   return 30.0;
    case Difficulty::Medium: return 60.0;
    case Difficulty::Hard:   return 110.0;
  }
  return 60.0;
}

void PongWidget::grabKeyboard()
{
  XGrabKeyboard(m_xconn.display(), m_xconn.root(), False,
                GrabModeAsync, GrabModeAsync, CurrentTime);
}

void PongWidget::releaseKeyboard()
{
  XUngrabKeyboard(m_xconn.display(), CurrentTime);
}

void PongWidget::handlePlayPauseButton()
{
  if (!m_gameActive)
  {
    m_gameActive = true;
    m_paused = false;
    m_lastMs = 0;
    grabKeyboard();
    return;
  }

  if (!m_paused)
  {
    m_paused = true;
    m_lastMs = 0;
    releaseKeyboard();
  }
  else
  {
    m_paused = false;
    m_lastMs = 0;
    grabKeyboard();
  }
}

void PongWidget::resetBall(int dir)
{
  int fieldW = width() - 2 * kBtnW - 2;
  m_ballX = fieldW / 2;
  m_ballY = 10;
  m_ballVX = ballSpeedForDifficulty() * dir;
  m_ballVY = ballSpeedForDifficulty() * 0.4 * ((rand() % 2) ? 1 : -1);
}

bool PongWidget::handleEscape()
{
  if (m_settingsActive)
  {
    hideSettingsPopup();
    return true;
  }
  if (m_gameActive)
  {
    m_gameActive = false;
    m_paused = false;
    m_gameOver = false;
    m_scoreL = 0;
    m_scoreR = 0;
    releaseKeyboard();
    return true;
  }
  return false;
}

//bool PongWidget::handleEscape()
//{
//  if (m_settingsActive)
//  {
//    hideSettingsPopup();
//    return true;
//  }
//  if (m_gameActive)
//  {
//    m_gameActive = false;
//    m_paused = false;
//    releaseKeyboard();
//    return true;
//  }
//  return false;
//}

bool PongWidget::handlePopupKey(XKeyEvent* pEvent)
{
  if (!pEvent || !m_settingsActive)
  {
    return false;
  }
  KeySym sym = XLookupKeysym(pEvent, 0);
  if (sym == XK_Escape)
  {
    hideSettingsPopup();
    return true;
  }
  return true;
}

bool PongWidget::handlePopupClick(XButtonEvent* pEvent)
{
  if (!pEvent || !m_settingsActive)
  {
    return false;
  }

  XftFont* pFont = m_font.font();
  int ascent = pFont ? pFont->ascent : 10;
  int rel = pEvent->y - kSettingsPad - ascent;
  int row = rel / kSettingsRowH;

  if (row >= 0 && row < 3)
  {
    m_difficulty = static_cast<Difficulty>(row);
    saveDifficulty();
    resetBall(1);
  }
  else if (row == 3)
  {
    m_soundEnabled = !m_soundEnabled;
    saveSoundEnabled();
  }

  drawPopup();
  return true;
}

//bool PongWidget::handlePopupClick(XButtonEvent* pEvent)
//{
//  if (!pEvent || !m_settingsActive)
//  {
//    return false;
//  }
//
//  XftFont* pFont = m_font.font();
//  int ascent = pFont ? pFont->ascent : 10;
//  int rel = pEvent->y - kSettingsPad - ascent;
//  int row = rel / kSettingsRowH;
//
//  if (row >= 0 && row < 3)
//  {
//    m_difficulty = static_cast<Difficulty>(row);
//    saveDifficulty();
//    resetBall(1);
//    drawPopup();
//  }
//  return true;
//}

Window PongWidget::popupWindow() const
{
  return m_settingsActive ? m_settingsPopup : None;
}

void PongWidget::showSettingsPopup(int screenX)
{
  Display* d = m_xconn.display();
  m_settingsH = kSettingsPad * 2 + kSettingsRows * kSettingsRowH + 4;

  int screenW = m_xconn.width();
  int screenH = m_xconn.height();
  int widgetW = width();
  int px = screenX + widgetW - m_settingsW;
  int py = screenH - MewConst::panelHeight - m_settingsH - 4;
  if (px < 0) px = 0;
  if (px + m_settingsW > screenW) px = screenW - m_settingsW;

  if (m_settingsPopup == None)
  {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0x1e1e1e;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    m_settingsPopup = XCreateWindow(
      d, m_xconn.root(),
      px, py, m_settingsW, m_settingsH, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs);
  }
  else
  {
    XMoveResizeWindow(d, m_settingsPopup, px, py, m_settingsW, m_settingsH);
  }

  XMapRaised(d, m_settingsPopup);
  m_settingsActive = true;
  XGrabKeyboard(d, m_settingsPopup, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  drawPopup();
}

void PongWidget::hideSettingsPopup()
{
  if (m_settingsPopup != None && m_settingsActive)
  {
    XUnmapWindow(m_xconn.display(), m_settingsPopup);
    XUngrabKeyboard(m_xconn.display(), CurrentTime);
  }
  m_settingsActive = false;
}

void PongWidget::drawPopup()
{
  if (m_settingsPopup == None || !m_settingsActive)
  {
    return;
  }

  Display* d = m_xconn.display();
  int screen = m_xconn.screen();
  GC gc = XCreateGC(d, m_settingsPopup, 0, nullptr);

  XSetForeground(d, gc, 0x1e1e1e);
  XFillRectangle(d, m_settingsPopup, gc, 0, 0, m_settingsW, m_settingsH);
  XSetForeground(d, gc, 0x555555);
  XDrawRectangle(d, m_settingsPopup, gc, 0, 0, m_settingsW - 1, m_settingsH - 1);

  const char* names[3] = { "Easy", "Medium", "Hard" };
  int selected = static_cast<int>(m_difficulty);

  XftFont* pFont = m_font.font();
  int ascent = pFont ? pFont->ascent : 10;

  // Rows 0..2 — difficulty
  for (int i = 0; i < 3; ++i)
  {
    int rowY = kSettingsPad + i * kSettingsRowH;
    int baseline = rowY + ascent;

    if (i == selected)
    {
      XSetForeground(d, gc, 0x0a64c8);
      XFillRectangle(d, m_settingsPopup, gc, 2, rowY,
                     m_settingsW - 4, kSettingsRowH);
    }

    m_font.setColor(0xffffff);
    m_font.draw(d, screen, m_settingsPopup,
                kSettingsPad + 4, baseline, names[i]);

    if (i == selected)
    {
      m_font.draw(d, screen, m_settingsPopup,
                  m_settingsW - 18, baseline, "\xe2\x9c\x93");
    }
  }

  // Divider
  int divY = kSettingsPad + 3 * kSettingsRowH;
  XSetForeground(d, gc, 0x444444);
  XFillRectangle(d, m_settingsPopup, gc, 4, divY, m_settingsW - 8, 1);

  // Row 3 — sound toggle
  int rowY = kSettingsPad + 3 * kSettingsRowH;
  int baseline = rowY + ascent;

  if (m_soundEnabled)
  {
    XSetForeground(d, gc, 0x0a64c8);
    XFillRectangle(d, m_settingsPopup, gc, 2, rowY,
                   m_settingsW - 4, kSettingsRowH);
  }

  m_font.setColor(0xffffff);
  std::string soundLabel = std::string("Sound: ") + (m_soundEnabled ? "On" : "Off");
  m_font.draw(d, screen, m_settingsPopup,
              kSettingsPad + 4, baseline, soundLabel);

  if (m_soundEnabled)
  {
    m_font.draw(d, screen, m_settingsPopup,
                m_settingsW - 18, baseline, "\xe2\x9c\x93");
  }

  m_font.setColor(0xffffff);
  XFreeGC(d, gc);
}
//void PongWidget::drawPopup()
//{
//  if (m_settingsPopup == None || !m_settingsActive)
//  {
//    return;
//  }
//
//  Display* d = m_xconn.display();
//  int screen = m_xconn.screen();
//  GC gc = XCreateGC(d, m_settingsPopup, 0, nullptr);
//
//  XSetForeground(d, gc, 0x1e1e1e);
//  XFillRectangle(d, m_settingsPopup, gc, 0, 0, m_settingsW, m_settingsH);
//  XSetForeground(d, gc, 0x555555);
//  XDrawRectangle(d, m_settingsPopup, gc, 0, 0, m_settingsW - 1, m_settingsH - 1);
//
//  const char* names[3] = { "Easy", "Medium", "Hard" };
//  int selected = static_cast<int>(m_difficulty);
//
//  XftFont* pFont = m_font.font();
//  int ascent = pFont ? pFont->ascent : 10;
//
//  for (int i = 0; i < 3; ++i)
//  {
//    int rowY = kSettingsPad + i * kSettingsRowH;
//    int baseline = rowY + ascent;
//
//    if (i == selected)
//    {
//      XSetForeground(d, gc, 0x0a64c8);
//      XFillRectangle(d, m_settingsPopup, gc, 2, rowY,
//                     m_settingsW - 4, kSettingsRowH);
//    }
//
//    m_font.setColor(0xffffff);
//    m_font.draw(d, screen, m_settingsPopup,
//                kSettingsPad + 4, baseline, names[i]);
//
//    if (i == selected)
//    {
//      m_font.setColor(0xffffff);
//      m_font.draw(d, screen, m_settingsPopup,
//                  m_settingsW - 18, baseline, "\xe2\x9c\x93");
//    }
//  }
//
//  m_font.setColor(0xffffff);
//  XFreeGC(d, gc);
//}

bool PongWidget::tick()
{
  if (!m_gameActive || m_paused)
  {
    return false;
  }

  if (m_gameOver)
  {
    if (time(nullptr) - m_gameOverAt >= 2)
    {
      m_gameOver = false;
      m_scoreL = 0;
      m_scoreR = 0;
      resetBall(1);
      return true;
    }
    return false;
  }

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long now = static_cast<long long>(ts.tv_sec) * 1000
                + ts.tv_nsec / 1000000;

  if (m_lastMs == 0)
  {
    m_lastMs = now;
    return false;
  }

  double dt = (now - m_lastMs) / 1000.0;
  m_lastMs = now;
  if (dt > 0.1)
  {
    dt = 0.1;
  }

  const double subDt = 0.004;
  while (dt > 0)
  {
    double step = (dt < subDt) ? dt : subDt;
    updateGame(step);
    dt -= step;
    if (m_gameOver) break;
  }

  return true;
}

//bool PongWidget::tick()
//{
//  if (!m_gameActive || m_paused)
//  {
//    return false;
//  }
//
//  struct timespec ts;
//  clock_gettime(CLOCK_MONOTONIC, &ts);
//  long long now = static_cast<long long>(ts.tv_sec) * 1000
//                + ts.tv_nsec / 1000000;
//
//  if (m_lastMs == 0)
//  {
//    m_lastMs = now;
//    return false;
//  }
//
//  double dt = (now - m_lastMs) / 1000.0;
//  m_lastMs = now;
//  if (dt > 0.1)
//  {
//    dt = 0.1;
//  }
//
//  // Sub-step for smooth motion + pixel-accurate collisions.
//  const double subDt = 0.004;
//  while (dt > 0)
//  {
//    double step = (dt < subDt) ? dt : subDt;
//    updateGame(step);
//    dt -= step;
//  }
//
//  return true;
//}

void PongWidget::updateGame(double dt)
{
  int widgetW = width();
  int fieldW = widgetW - 2 * kBtnW - 2;
  int fieldH = MewConst::panelHeight - 4;

  char keys[32];
  XQueryKeymap(m_xconn.display(), keys);
  auto isDown = [&](KeySym s) -> bool {
    KeyCode c = XKeysymToKeycode(m_xconn.display(), s);
    if (!c) return false;
    return (keys[c / 8] & (1 << (c % 8))) != 0;
  };

  bool up = isDown(XK_Up) || isDown(XK_w) || isDown(XK_W);
  bool down = isDown(XK_Down) || isDown(XK_s) || isDown(XK_S);

  const double playerSpeed = 90.0;
  if (up)   m_paddleL -= playerSpeed * dt;
  if (down) m_paddleL += playerSpeed * dt;
  if (m_paddleL < 0) m_paddleL = 0;
  if (m_paddleL > fieldH - kPaddleH) m_paddleL = fieldH - kPaddleH;

  double target = m_ballY - kPaddleH / 2.0 + kBallSize / 2.0;
  double aiSpeed = aiSpeedForDifficulty();
  if (target < m_paddleR - 1.0)      m_paddleR -= aiSpeed * dt;
  else if (target > m_paddleR + 1.0) m_paddleR += aiSpeed * dt;
  if (m_paddleR < 0) m_paddleR = 0;
  if (m_paddleR > fieldH - kPaddleH) m_paddleR = fieldH - kPaddleH;

  m_ballX += m_ballVX * dt;
  m_ballY += m_ballVY * dt;

  if (m_ballY < 0) { m_ballY = 0; m_ballVY = -m_ballVY; }
  if (m_ballY > fieldH - kBallSize)
  {
    m_ballY = fieldH - kBallSize;
    m_ballVY = -m_ballVY;
  }

  // Player paddle hit — plays the hit sound.
  const double playerPaddleRight = 1.0 + kPaddleW;
  if (m_ballVX < 0 && m_ballX <= playerPaddleRight)
  {
    if (m_ballY + kBallSize >= m_paddleL &&
        m_ballY <= m_paddleL + kPaddleH)
    {
      m_ballX = playerPaddleRight;
      m_ballVX = -m_ballVX;
      double hit = (m_ballY + kBallSize / 2.0) - (m_paddleL + kPaddleH / 2.0);
      m_ballVY += hit * 4.0;
      if (m_ballVY > 90) m_ballVY = 90;
      if (m_ballVY < -90) m_ballVY = -90;
      playSound(m_hitSound);
    }
  }

  // AI paddle.
  const double aiPaddleLeft = static_cast<double>(fieldW) - 1.0 - kPaddleW;
  if (m_ballVX > 0 && m_ballX + kBallSize >= aiPaddleLeft)
  {
    if (m_ballY + kBallSize >= m_paddleR &&
        m_ballY <= m_paddleR + kPaddleH)
    {
      m_ballX = aiPaddleLeft - kBallSize;
      m_ballVX = -m_ballVX;
    }
  }

  // Scoring + win condition.
  if (m_ballX < -2)
  {
    // Player missed → AI scores.
    m_scoreR++;
    if (m_scoreR >= kWinScore)
    {
      finishGame(false);
    }
    else
    {
      playSound(m_lostRoundSound);
      resetBall(1);
    }
  }
  else if (m_ballX > fieldW + 2)
  {
    // AI missed → player scores.
    m_scoreL++;
    if (m_scoreL >= kWinScore)
    {
      finishGame(true);
    }
    else
    {
      playSound(m_wonRoundSound);
      resetBall(-1);
    }
  }
}

//void PongWidget::updateGame(double dt)
//{
//  int widgetW = width();
//  int fieldW = widgetW - 2 * kBtnW - 2;
//  int fieldH = MewConst::panelHeight - 4;
//
//  // Read held keys.
//  char keys[32];
//  XQueryKeymap(m_xconn.display(), keys);
//  auto isDown = [&](KeySym s) -> bool {
//    KeyCode c = XKeysymToKeycode(m_xconn.display(), s);
//    if (!c) return false;
//    return (keys[c / 8] & (1 << (c % 8))) != 0;
//  };
//
//  bool up = isDown(XK_Up) || isDown(XK_w) || isDown(XK_W);
//  bool down = isDown(XK_Down) || isDown(XK_s) || isDown(XK_S);
//
//  const double playerSpeed = 90.0;
//  if (up)   m_paddleL -= playerSpeed * dt;
//  if (down) m_paddleL += playerSpeed * dt;
//  if (m_paddleL < 0) m_paddleL = 0;
//  if (m_paddleL > fieldH - kPaddleH) m_paddleL = fieldH - kPaddleH;
//
//  // AI
//  double target = m_ballY - kPaddleH / 2.0 + kBallSize / 2.0;
//  double aiSpeed = aiSpeedForDifficulty();
//  if (target < m_paddleR - 1.0)      m_paddleR -= aiSpeed * dt;
//  else if (target > m_paddleR + 1.0) m_paddleR += aiSpeed * dt;
//  if (m_paddleR < 0) m_paddleR = 0;
//  if (m_paddleR > fieldH - kPaddleH) m_paddleR = fieldH - kPaddleH;
//
//  // Move ball.
//  m_ballX += m_ballVX * dt;
//  m_ballY += m_ballVY * dt;
//
//  // Wall bounce.
//  if (m_ballY < 0) { m_ballY = 0; m_ballVY = -m_ballVY; }
//  if (m_ballY > fieldH - kBallSize)
//  {
//    m_ballY = fieldH - kBallSize;
//    m_ballVY = -m_ballVY;
//  }
//
//  // Player paddle (left side). Paddle occupies pixels 1..kPaddleW.
//  const double playerPaddleRight = 1.0 + kPaddleW;
//  if (m_ballVX < 0 && m_ballX <= playerPaddleRight)
//  {
//    if (m_ballY + kBallSize >= m_paddleL &&
//        m_ballY <= m_paddleL + kPaddleH)
//    {
//      m_ballX = playerPaddleRight;
//      m_ballVX = -m_ballVX;
//      double hit = (m_ballY + kBallSize / 2.0) - (m_paddleL + kPaddleH / 2.0);
//      m_ballVY += hit * 4.0;
//      if (m_ballVY > 90) m_ballVY = 90;
//      if (m_ballVY < -90) m_ballVY = -90;
//    }
//  }
//
//  // AI paddle (right side). Paddle occupies pixels fieldW-kPaddleW-1 .. fieldW-2.
//  const double aiPaddleLeft = static_cast<double>(fieldW) - 1.0 - kPaddleW;
//  if (m_ballVX > 0 && m_ballX + kBallSize >= aiPaddleLeft)
//  {
//    if (m_ballY + kBallSize >= m_paddleR &&
//        m_ballY <= m_paddleR + kPaddleH)
//    {
//      m_ballX = aiPaddleLeft - kBallSize;
//      m_ballVX = -m_ballVX;
//    }
//  }
//
//  // Score.
//  if (m_ballX < -2)
//  {
//    m_scoreR++;
//    resetBall(1);
//  }
//  else if (m_ballX > fieldW + 2)
//  {
//    m_scoreL++;
//    resetBall(-1);
//  }
//}

void PongWidget::handleLocalClick(int localX, int screenX)
{
  int widgetW = width();

  if (localX < kBtnW)
  {
    handlePlayPauseButton();
    return;
  }

  if (localX >= widgetW - kBtnW)
  {
    if (m_settingsActive)
    {
      hideSettingsPopup();
    }
    else
    {
      showSettingsPopup(screenX);
    }
    return;
  }
}

bool PongWidget::onClick(int screenX)
{
  (void)screenX;
  return false;
}

std::string PongWidget::tooltip() const
{
  char buf[80];
  const char* diff =
    (m_difficulty == Difficulty::Easy)   ? "Easy" :
    (m_difficulty == Difficulty::Hard)   ? "Hard" : "Medium";
  snprintf(buf, sizeof(buf), "Pong [%s]  %d : %d", diff, m_scoreL, m_scoreR);
  return buf;
}

void PongWidget::draw(Display* display, Window panel, int x, int baseline)
{
  (void)baseline;

  GC gc = XCreateGC(display, panel, 0, nullptr);
  int widgetW = width();
  int widgetH = MewConst::panelHeight;

  // Outer.
  XSetForeground(display, gc, m_bgColor);
  XFillRectangle(display, panel, gc, x, 0, widgetW, widgetH);
  XSetForeground(display, gc, 0x2a2a3a);
  XDrawRectangle(display, panel, gc, x, 0, widgetW - 1, widgetH - 1);

  // --- Left section: play/pause button ---
  XSetForeground(display, gc, 0x14142a);
  XFillRectangle(display, panel, gc, x + 1, 1, kBtnW - 1, widgetH - 2);

  int btnY = 5;
  int btnH = widgetH - 10;
  if (btnH < 6) btnH = 6;
  int iconW = kBtnW - 10;
  int iconX = x + (kBtnW - iconW) / 2;

  if (!m_gameActive || m_paused)
  {
    XSetForeground(display, gc, m_gameActive ? 0xffcc00 : 0x44dd66);
    XPoint tri[3];
    tri[0].x = static_cast<short>(iconX);
    tri[0].y = static_cast<short>(btnY);
    tri[1].x = static_cast<short>(iconX);
    tri[1].y = static_cast<short>(btnY + btnH);
    tri[2].x = static_cast<short>(iconX + iconW);
    tri[2].y = static_cast<short>(btnY + btnH / 2);
    XFillPolygon(display, panel, gc, tri, 3, Convex, CoordModeOrigin);
  }
  else
  {
    XSetForeground(display, gc, 0x00d0ff);
    int barW = (iconW - 3) / 2;
    if (barW < 2) barW = 2;
    XFillRectangle(display, panel, gc, iconX, btnY, barW, btnH);
    XFillRectangle(display, panel, gc, iconX + barW + 3, btnY, barW, btnH);
  }

  // Separator between button and field.
  XSetForeground(display, gc, 0x3a3a5a);
  XFillRectangle(display, panel, gc, x + kBtnW, 1, 1, widgetH - 2);

  // --- Field ---
  int fieldX = x + kBtnW + 1;
  int fieldY = 2;
  int fieldW = widgetW - 2 * kBtnW - 2;
  int fieldH = widgetH - 4;

  XSetForeground(display, gc, 0x050510);
  XFillRectangle(display, panel, gc, fieldX, fieldY, fieldW, fieldH);

  // Dashed center line.
  XSetForeground(display, gc, 0x222240);
  for (int cy = fieldY + 1; cy < fieldY + fieldH - 1; cy += 3)
  {
    XFillRectangle(display, panel, gc, fieldX + fieldW / 2, cy, 1, 2);
  }

  // Player paddle.
  XSetForeground(display, gc, m_playerColor);
  XFillRectangle(display, panel, gc,
    fieldX + 1, fieldY + static_cast<int>(m_paddleL),
    kPaddleW, kPaddleH);

  // AI paddle.
  XSetForeground(display, gc, m_aiColor);
  XFillRectangle(display, panel, gc,
    fieldX + fieldW - 1 - kPaddleW,
    fieldY + static_cast<int>(m_paddleR),
    kPaddleW, kPaddleH);

  // Ball.
  XSetForeground(display, gc, m_ballColor);
  XFillRectangle(display, panel, gc,
    fieldX + static_cast<int>(m_ballX + 0.5),
    fieldY + static_cast<int>(m_ballY + 0.5),
    kBallSize, kBallSize);

  // Scores near center line.
  char buf[16];
  m_font.setColor(0x666688);

  snprintf(buf, sizeof(buf), "%d", m_scoreL);
  m_font.draw(display, m_xconn.screen(), panel,
              fieldX + fieldW / 2 - 14, fieldY + 11, buf);

  snprintf(buf, sizeof(buf), "%d", m_scoreR);
  m_font.draw(display, m_xconn.screen(), panel,
              fieldX + fieldW / 2 + 6, fieldY + 11, buf);

  // --- Right section: settings button ---
  XSetForeground(display, gc, 0x14142a);
  XFillRectangle(display, panel, gc, x + widgetW - kBtnW, 1,
                 kBtnW - 1, widgetH - 2);

  int setBtnX = x + widgetW - kBtnW;
  int lineW = 12;
  int lineX = setBtnX + (kBtnW - lineW) / 2;
  int lineY1 = 6;
  int lineY2 = btnY + btnH / 2 - 1;
  int lineY3 = widgetH - 8;

  XSetForeground(display, gc, m_gameActive ? 0x8888cc : 0x666688);
  XFillRectangle(display, panel, gc, lineX, lineY1, lineW, 2);
  XFillRectangle(display, panel, gc, lineX, lineY2, lineW, 2);
  XFillRectangle(display, panel, gc, lineX, lineY3, lineW, 2);

  // Restore default font color so other widgets aren't affected.
  m_font.setColor(0xffffff);

  XFreeGC(display, gc);
}

static PanelWidget* createPong(XConnection& xconn, FontRenderer& font)
{
  return new PongWidget(xconn, font);
}

static PanelWidgetRegistrar s_pongRegistrar("pong", createPong);
