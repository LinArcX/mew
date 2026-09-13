#pragma once

#include "../PanelWidget.hpp"
#include "../../XConnection.hpp"
#include "../../FontRenderer.hpp"
#include "../../Config.hpp"
#include "../../EmbeddedFont.hpp"

#include <sys/un.h>
#include <sys/socket.h>
#include <ctime>
#include <string>
#include <vector>
#include <sys/types.h>

class MusicPlayerWidget : public PanelWidget
{
public:
  MusicPlayerWidget(XConnection& xconn, FontRenderer& font);
  ~MusicPlayerWidget() override;

  const char* id() const override { return "music"; }
  int width() const override { return m_computedWidth; }
  void draw(Display* display, Window panel, int x, int baseline) override;
  bool onClick(int screenX) override;
  std::string tooltip() const override;
  bool tick() override;
  void configure(const Config& config) override;
  bool handleEscape() override;
  Window popupWindow() const override
  {
    if (m_seekPopupActive) return m_seekPopup;
    return m_popup;
  }

  void drawPopup() override;
  bool handlePopupMotion(XMotionEvent* pEvent) override;
  bool handlePopupClick(XButtonEvent* pEvent) override;
  bool handlePopupKey(XKeyEvent* pEvent) override;
  bool hasFocusedPopup() const override { return m_popupActive; }
  int eqWidth() const { return m_eqBars * (kEqBarW + kEqBarGap) + 8; }
  void playPrev();
  void playNext();
  void togglePause();
  void stopPlayback();
  void showPopup(int screenX);
  void showSeekPopup(int screenX);
  void hideSeekPopup();
  void commitSeek();

private:
  enum class Btn { NoBtn, Note, Prev, Play, Stop, Next, Label };

  Btn buttonAt(int localX) const;
  void loadDirs();
  void saveDirs();
  void scanFiles();
  void playIndex(size_t idx);
  void hidePopup();
  double queryMpv(const char* property) const;
  bool sendMpvSeek(double seconds) const;
  void drawSeekPopup();

  XConnection& m_xconn;
  FontRenderer& m_font;
  EmbeddedFont m_iconFont;

  std::vector<std::string> m_dirs;
  std::vector<std::string> m_files;
  size_t m_current = 0;

  pid_t m_playerPid = -1;
  bool m_paused = false;
  std::string m_trackName;

  unsigned long m_noteColor = 0xffffff;
  unsigned long m_buttonColor = 0xffffff;
  int m_eqBars = 4;
  std::vector<unsigned long> m_eqColors;
  int m_computedWidth = 160;

  int m_eqFrame = 0;
  long long m_lastEqMs = 0;

  Window m_popup = None;
  Window m_seekPopup = None;
  bool m_seekPopupActive = false;
  bool m_seekDragging = false;
  double m_position = -1.0;
  double m_duration = -1.0;
  long long m_lastQueryMs = 0;
  std::string m_mpvSocket;

  bool m_popupActive = false;
  int m_popupW = 0;
  int m_popupH = 0;
  bool m_escGrabbed = false;
  std::string m_inputBuffer;
  int m_popupDirCount = 0;
  pid_t m_pendingKill = -1;
  time_t m_playStart = 0;

  static constexpr int kSeekPopupW = 320;
  static constexpr int kSeekPopupH = 60;

  static constexpr int kMusicIconW = 24;
  static constexpr int kBtnW = 24;
  static constexpr int kEqBarW = 4;
  static constexpr int kEqBarGap = 3;

  static constexpr int kPopupWidth = 420;
  static constexpr int kPopupRowH = 22;
  static constexpr int kPopupPad = 8;
  static constexpr int kPopupMaxRows = 8;
};
