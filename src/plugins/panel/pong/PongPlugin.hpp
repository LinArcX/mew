#pragma once

#include "../PanelPlugin.hpp"
#include "XConnection.hpp"
#include "FontRenderer.hpp"
#include "Config.hpp"
#include "Types.hpp"

/**
 * @brief Minimal Pong game embedded in the panel.
 *
 * The player controls the left paddle with arrow / WASD keys, the AI
 * plays on the right. The plugin strip contains a play/pause button,
 * the playing field, and a settings button. First to 5 points wins.
 * Sound effects are played from ~/.config/mew/pong_sounds/.
 */
class PongPlugin : public PanelPlugin
{
public:
  /**
   * @brief Construct a Pong widget.
   * @param xconn Open X connection.
   * @param font  Font renderer for score digits.
   */
  PongPlugin(XConnection& xconn, FontRenderer& font);

  /** @brief Destroy the settings popup and release the keyboard grab. */
  ~PongPlugin() override;

  /** @brief Plugin id used in config ("pong"). */
  const char* id() const override { return "pong"; }

  /** @brief Panel width reserved for the game (1/6 of the screen). */
  int width() const override { return m_xconn.width() / 6; }

  /**
   * @brief Draw the play button, field, paddles, ball, and scores.
   * @param display  X display.
   * @param panel    Panel pixmap/window.
   * @param x        Left edge of the plugin.
   * @param baseline Text baseline y (unused).
   */
  void draw(Display* display, Window panel, int x, int baseline) override;

  /** @brief Screen-coordinate click handler is unused. */
  bool onClick(int screenX) override;

  /** @brief Tooltip: difficulty and current score. */
  std::string tooltip() const override;

  /** @brief Advance the physics simulation while the game is running. */
  bool tick() override;

  /** @brief Apply config (currently unused). */
  void configure(const Config& config) override;

  /** @brief ESC closes the settings popup or stops the game. */
  bool handleEscape() override;

  /** @brief True while the settings popup is mapped. */
  bool hasFocusedPopup() const override { return m_settingsActive; }

  /** @brief Return the settings popup window, or None. */
  Window popupWindow() const override;

  /** @brief Redraw the settings popup. */
  void drawPopup() override;

  /**
   * @brief Handle keys while the settings popup has focus (ESC to close).
   * @param pEvent Key event.
   * @return true if consumed.
   */
  bool handlePopupKey(XKeyEvent* pEvent) override;

  /**
   * @brief Select difficulty or toggle sound from the popup.
   * @param pEvent Button event positioned over a row.
   * @return true if consumed.
   */
  bool handlePopupClick(XButtonEvent* pEvent) override;

  /**
   * @brief Local click: play/pause on the left, settings on the right.
   * @param localX  Local x inside the plugin.
   * @param screenX Absolute screen x.
   * @return true (always consumes the click).
   */
  bool handleLocalClick(int localX, int screenX) override;

  static constexpr int kBtnW = 20;  ///< Width of the play / settings buttons.

private:
  /** @brief AI and ball speed preset. */
  enum class Difficulty { Easy, Medium, Hard };

  /**
   * @brief Center the ball and launch it in the given direction.
   * @param dir +1 = to the right, -1 = to the left.
   */
  void resetBall(int dir);

  /**
   * @brief Advance one physics substep.
   * @param dt Delta time in seconds.
   */
  void updateGame(double dt);

  /** @brief Handle the play/pause button (grab/release keyboard). */
  void handlePlayPauseButton();

  /** @brief Grab the keyboard on the root window. */
  void grabKeyboard();

  /** @brief Release the keyboard grab. */
  void releaseKeyboard();

  /** @brief Read persisted difficulty from ~/.config/mew/pong_difficulty. */
  void loadDifficulty();

  /** @brief Persist the current difficulty. */
  void saveDifficulty();

  /**
   * @brief Map the settings popup anchored to the plugin.
   * @param screenX Absolute screen x of the plugin.
   */
  void showSettingsPopup(int screenX);

  /** @brief Unmap the settings popup and release the grab. */
  void hideSettingsPopup();

  /** @brief Ball speed in pixels per second for the current difficulty. */
  double ballSpeedForDifficulty() const;

  /** @brief AI paddle speed in pixels per second for the current difficulty. */
  double aiSpeedForDifficulty() const;

  /** @brief Discover sound files in ~/.config/mew/pong_sounds/. */
  void initSounds();

  /** @brief Read the persisted sound-enabled flag. */
  void loadSoundEnabled();

  /** @brief Persist the sound-enabled flag. */
  void saveSoundEnabled();

  /**
   * @brief Play a sound file asynchronously (fork + aplay/paplay/mpv).
   * @param path Path to the audio file.
   */
  void playSound(const std::string& path);

  /**
   * @brief End the game and play the win/lose sound.
   * @param playerWon True if the player won.
   */
  void finishGame(bool playerWon);

  XConnection& m_xconn;         ///< Open X connection.
  FontRenderer& m_font;         ///< Font renderer for scores.

  bool m_gameActive = false;    ///< True once the game has been started.
  bool m_paused = false;        ///< True while paused.

  double m_ballX = 100;         ///< Ball x in field coordinates.
  double m_ballY = 10;          ///< Ball y in field coordinates.
  double m_ballVX = 70;         ///< Ball x velocity (px/s).
  double m_ballVY = 30;         ///< Ball y velocity (px/s).
  double m_paddleL = 8;         ///< Player paddle y in field coordinates.
  double m_paddleR = 8;         ///< AI paddle y in field coordinates.
  int m_scoreL = 0;             ///< Player score.
  int m_scoreR = 0;             ///< AI score.

  long long m_lastMs = 0;       ///< Timestamp of the last physics step.

  Difficulty m_difficulty = Difficulty::Medium;  ///< Current difficulty preset.

  Window m_settingsPopup = None;  ///< Settings popup window.
  bool m_settingsActive = false;  ///< True while settings popup is mapped.
  int m_settingsW = 130;          ///< Settings popup width.
  int m_settingsH = 0;            ///< Settings popup height.

  static constexpr int kWinScore = 5;  ///< First to this score wins.
  bool m_gameOver = false;             ///< True after win/lose until reset.
  time_t m_gameOverAt = 0;             ///< Timestamp of game end.

  bool m_soundEnabled = true;      ///< Master toggle for sound effects.
  std::string m_soundDir;          ///< Directory scanned for sounds.
  std::string m_hitSound;          ///< Player paddle hit the ball.
  std::string m_wonRoundSound;     ///< Player won a round.
  std::string m_lostRoundSound;    ///< Player lost a round.
  std::string m_wonGameSound;      ///< Player won the game.
  std::string m_lostGameSound;     ///< Player lost the game.

  unsigned long m_bgColor = 0x0a0a1a;      ///< Field background.
  unsigned long m_playerColor = 0x00d0ff;  ///< Player paddle color.
  unsigned long m_aiColor = 0xff00cc;      ///< AI paddle color.
  unsigned long m_ballColor = 0xffcc00;    ///< Ball color.

  static constexpr int kPaddleW = 2;       ///< Paddle width.
  static constexpr int kPaddleH = 7;       ///< Paddle height.
  static constexpr int kBallSize = 2;      ///< Ball side length.
  static constexpr int kSettingsRowH = 24; ///< Settings popup row height.
  static constexpr int kSettingsPad = 8;   ///< Settings popup inner pad.
  static constexpr int kSettingsRows = 4;  ///< Easy / Medium / Hard / Sound

};
