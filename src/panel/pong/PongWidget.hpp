#pragma once

#include "../PanelWidget.hpp"
#include "../../XConnection.hpp"
#include "../../FontRenderer.hpp"
#include "../../Config.hpp"
#include "../../Types.hpp"

class PongWidget : public PanelWidget
{
public:
  PongWidget(XConnection& xconn, FontRenderer& font);
  ~PongWidget() override;

  const char* id() const override { return "pong"; }
  int width() const override { return m_xconn.width() / 6; }
  void draw(Display* display, Window panel, int x, int baseline) override;
  bool onClick(int screenX) override;
  std::string tooltip() const override;
  bool tick() override;
  void configure(const Config& config) override;
  bool handleEscape() override;
  bool hasFocusedPopup() const override { return m_settingsActive; }
  Window popupWindow() const override;
  void drawPopup() override;
  bool handlePopupKey(XKeyEvent* pEvent) override;
  bool handlePopupClick(XButtonEvent* pEvent) override;

  void handleLocalClick(int localX, int screenX);

  static constexpr int kBtnW = 20;

private:
  enum class Difficulty { Easy, Medium, Hard };

  void resetBall(int dir);
  void updateGame(double dt);
  void handlePlayPauseButton();
  void grabKeyboard();
  void releaseKeyboard();
  void loadDifficulty();
  void saveDifficulty();
  void showSettingsPopup(int screenX);
  void hideSettingsPopup();

  double ballSpeedForDifficulty() const;
  double aiSpeedForDifficulty() const;

  void initSounds();
  void loadSoundEnabled();
  void saveSoundEnabled();
  void playSound(const std::string& path);
  void finishGame(bool playerWon);

  XConnection& m_xconn;
  FontRenderer& m_font;

  bool m_gameActive = false;
  bool m_paused = false;

  double m_ballX = 100;
  double m_ballY = 10;
  double m_ballVX = 70;
  double m_ballVY = 30;
  double m_paddleL = 8;
  double m_paddleR = 8;
  int m_scoreL = 0;
  int m_scoreR = 0;

  long long m_lastMs = 0;

  Difficulty m_difficulty = Difficulty::Medium;

  Window m_settingsPopup = None;
  bool m_settingsActive = false;
  int m_settingsW = 130;
  int m_settingsH = 0;

  // Win condition
  static constexpr int kWinScore = 5;
  bool m_gameOver = false;
  time_t m_gameOverAt = 0;

  // Sound
  bool m_soundEnabled = true;
  std::string m_soundDir;
  std::string m_hitSound;
  std::string m_wonRoundSound;
  std::string m_lostRoundSound;
  std::string m_wonGameSound;
  std::string m_lostGameSound;

  unsigned long m_bgColor = 0x0a0a1a;
  unsigned long m_playerColor = 0x00d0ff;
  unsigned long m_aiColor = 0xff00cc;
  unsigned long m_ballColor = 0xffcc00;

  static constexpr int kPaddleW = 2;
  static constexpr int kPaddleH = 7;
  static constexpr int kBallSize = 2;
  static constexpr int kSettingsRowH = 24;
  static constexpr int kSettingsPad = 8;
  static constexpr int kSettingsRows = 4;  // Easy / Medium / Hard / Sound

};
