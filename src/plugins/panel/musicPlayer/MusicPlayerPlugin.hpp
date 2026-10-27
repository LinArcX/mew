#pragma once

#include "../PanelPlugin.hpp"
#include "../XConnection.hpp"
#include "../FontRenderer.hpp"
#include "../Config.hpp"
#include "../EmbeddedFont.hpp"
#include "../PluginProcess.hpp"

#include <sys/un.h>
#include <sys/socket.h>
#include <ctime>
#include <string>
#include <vector>
#include <sys/types.h>

/**
 * @brief Panel plugin that plays audio files via mpv.
 *
 * Provides prev/play-pause/stop/next controls and an animated equalizer.
 * Clicking the music note opens a popup for managing the directories that
 * are scanned for audio files. Clicking the equalizer opens a seek slider
 * that talks to mpv over its JSON IPC socket.
 */
class MusicPlayerPlugin : public PanelPlugin
{
public:
  /**
   * @brief Construct a music player panel plugin.
   * @param xconn Open X connection.
   * @param font  Font renderer for the panel text.
   */
  MusicPlayerPlugin(XConnection& xconn, FontRenderer& font);

  /** @brief Stop playback, close popups, and release resources. */
  ~MusicPlayerPlugin() override;

  /** @brief Plugin id used in config ("music"). */
  const char* id() const override { return "music"; }

  /** @brief Cached panel width derived from config (bars, buttons). */
  int width() const override { return m_computedWidth; }

  /**
   * @brief Draw controls and equalizer bars.
   * @param display  X display.
   * @param panel    Panel pixmap/window.
   * @param x        Left edge of the plugin.
   * @param baseline Text baseline y.
   */
  void draw(Display* display, Window panel, int x, int baseline) override;

  /**
   * @brief Screen-coordinate click handler (see handleLocalClick).
   * @param screenX Absolute screen x of the click.
   * @return true if consumed.
   */
  bool onClick(int screenX) override;

  /** @brief Tooltip: current track name or paused state. */
  std::string tooltip() const override;

  /** @brief Advance animation, poll mpv, auto-play the next track. */
  bool tick() override;

  /**
   * @brief Apply colors and equalizer bar count from config.
   * @param config Loaded configuration.
   */
  void configure(const Config& config) override;

  /** @brief Handle ESC to close the seek popup or the main popup. */
  bool handleEscape() override;

  /** @brief Return whichever popup is currently mapped. */
  Window popupWindow() const override
  {
    if (m_seekPopupActive) return m_seekPopup;
    return m_popup;
  }

  /** @brief Redraw whichever popup is active. */
  void drawPopup() override;

  /**
   * @brief Drag the seek slider.
   * @param pEvent Motion event.
   * @return true if consumed.
   */
  bool handlePopupMotion(XMotionEvent* pEvent) override;

  /**
   * @brief Handle popup clicks (rows, trash icon, seek bar).
   * @param pEvent Button event.
   * @return true if consumed.
   */
  bool handlePopupClick(XButtonEvent* pEvent) override;

  /**
   * @brief Route key events to the popup (text input, ESC, Enter).
   * @param pEvent Key event.
   * @return true if consumed.
   */
  bool handlePopupKey(XKeyEvent* pEvent) override;

  /** @brief True while the directory popup has keyboard focus. */
  bool hasFocusedPopup() const override { return m_popupActive; }

  /** @brief Optional out-of-process backend for the plugin. */
  PluginProcess* process() const override
  {
    if (m_playerProcess.isRunning()) return const_cast<PluginProcess*>(&m_playerProcess);
    return const_cast<PluginProcess*>(&m_scanProcess);
  }

  /** @brief Pixel width of the equalizer region. */
  int eqWidth() const { return m_eqBars * (kEqBarW + kEqBarGap) + 8; }

  /** @brief Play the previous track, wrapping around. */
  void playPrev();

  /** @brief Play the next track, wrapping around. */
  void playNext();

  /** @brief Toggle pause/resume, or start playback if idle. */
  void togglePause();

  /** @brief Kill the player process (non-blocking; reap in tick()). */
  void stopPlayback();

  /**
   * @brief Map the directory-management popup.
   * @param screenX Absolute screen x of the plugin (for placement).
   */
  void showPopup(int screenX);

  /**
   * @brief Map the seek-slider popup.
   * @param screenX Absolute screen x of the plugin (for placement).
   */
  void showSeekPopup(int screenX);

  /** @brief Unmap the seek popup. */
  void hideSeekPopup();

  /**
   * @brief Toggle the seek slider popup.
   * @param screenX Absolute screen x for placement.
   */
  void toggleSeekPopup(int screenX);

  /** @brief Send the current slider position to mpv if still dragging. */
  void commitSeek();

  /**
   * @brief Dispatch a local click inside the plugin strip.
   * @param localX Local x inside the plugin.
   * @param screenX Absolute screen x.
   * @return true (always consumes the click).
   */
  bool handleLocalClick(int localX, int screenX) override;

  /**
   * @brief Release handler for the popup (commits seek).
   * @param pEvent Button event.
   */
  void handlePopupRelease(XButtonEvent* pEvent) override;

private:
  /** @brief Hit-test regions inside the plugin strip. */
  enum class Btn { NoBtn, Note, Prev, Play, Stop, Next, Label };

  /**
   * @brief Map a local x coordinate to a control button.
   * @param localX Local x inside the plugin.
   * @return Button under the pointer.
   */
  Btn buttonAt(int localX) const;

  /** @brief Load music directories from ~/.config/mew/music_dirs. */
  void loadDirs();

  /** @brief Persist the current music directory list. */
  void saveDirs();

  /** @brief Recursively rescan all configured directories for audio files. */
  void scanFiles();

  /**
   * @brief Start playback at the given index.
   * @param idx Index into m_files.
   */
  void playIndex(size_t idx);

  /** @brief Unmap the directory popup and release the grab. */
  void hidePopup();

  /**
   * @brief Query a numeric property from mpv over the IPC socket.
   * @param property mpv property name (e.g. "time-pos").
   * @return Parsed value, or -1.0 on error.
   */
  double queryMpv(const char* property) const;

  /**
   * @brief Send an absolute seek command to mpv.
   * @param seconds Target position in seconds.
   * @return true on success.
   */
  bool sendMpvSeek(double seconds) const;

  /** @brief Redraw the seek slider popup. */
  void drawSeekPopup();

  XConnection& m_xconn;          ///< Open X connection.
  FontRenderer& m_font;          ///< Font renderer.
  EmbeddedFont m_iconFont;       ///< Embedded Nerd Font for icon glyphs.

  std::vector<std::string> m_dirs;   ///< Configured music directories.
  std::vector<std::string> m_files;  ///< Discovered audio files (recursive scan).
  size_t m_current = 0;              ///< Index into m_files of the current track.

  pid_t m_playerPid = -1;        ///< Child mpv pid, or -1 when idle.
  bool m_paused = false;         ///< True while paused (SIGSTOP).
  std::string m_trackName;       ///< Basename of the current track.

  unsigned long m_noteColor = 0xffffff;    ///< Color of the music note.
  unsigned long m_buttonColor = 0xffffff;  ///< Color of the control icons.
  int m_eqBars = 4;                        ///< Number of equalizer bars.
  std::vector<unsigned long> m_eqColors;   ///< Per-bar colors, cycles if shorter.
  int m_computedWidth = 160;               ///< Cached total panel width.

  int m_eqFrame = 0;             ///< Equalizer animation frame.
  long long m_lastEqMs = 0;      ///< Timestamp of last equalizer step.

  Window m_popup = None;         ///< Directory popup window.
  Window m_seekPopup = None;     ///< Seek slider popup window.
  bool m_seekPopupActive = false;
  bool m_seekDragging = false;
  double m_position = -1.0;      ///< Last position read from mpv.
  double m_duration = -1.0;      ///< Last duration read from mpv.
  long long m_lastQueryMs = 0;   ///< Timestamp of last mpv query.
  std::string m_mpvSocket;       ///< Path to the mpv IPC socket.

  PluginProcess m_scanProcess;
  PluginProcess m_playerProcess;
  bool m_scanPending = false;

  bool m_popupActive = false;
  int m_popupW = 0;
  int m_popupH = 0;
  bool m_escGrabbed = false;
  std::string m_inputBuffer;     ///< Text typed into the add-directory row.
  int m_popupDirCount = 0;
  pid_t m_pendingKill = -1;      ///< Previous player waiting to be reaped.
  time_t m_playStart = 0;        ///< Wall-clock start of current track.

  static constexpr int kSeekPopupW = 320;  ///< Seek popup width.
  static constexpr int kSeekPopupH = 60;   ///< Seek popup height.

  static constexpr int kMusicIconW = 24;   ///< Music-note button width.
  static constexpr int kBtnW = 24;         ///< Transport button width.
  static constexpr int kEqBarW = 4;        ///< Equalizer bar width.
  static constexpr int kEqBarGap = 3;      ///< Gap between equalizer bars.

  static constexpr int kPopupWidth = 420;    ///< Directory popup width.
  static constexpr int kPopupRowH = 22;      ///< Directory popup row height.
  static constexpr int kPopupPad = 8;        ///< Directory popup inner pad.
  static constexpr int kPopupMaxRows = 8;    ///< Max rows shown at once.
};
