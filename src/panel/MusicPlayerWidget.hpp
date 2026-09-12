#pragma once

#include "PanelWidget.hpp"
#include "../XConnection.hpp"
#include "../FontRenderer.hpp"
#include "../Config.hpp"
#include "../EmbeddedFont.hpp"

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
  int width() const override { return kWidth; }
  void draw(Display* display, Window panel, int x, int baseline) override;
  bool onClick(int screenX) override;
  std::string tooltip() const override;
  void tick() override;
  void configure(const Config& config) override;
  bool handleEscape() override;
  Window popupWindow() const override { return m_popup; }
  void drawPopup() override;
  bool handlePopupKey(XKeyEvent* pEvent) override;
  bool hasFocusedPopup() const override { return m_popupActive; }
  void playPrev();
  void playNext();
  void togglePause();
  void stopPlayback();
  void showPopup(int screenX);

private:
  //enum class Btn { None, Prev, Play, Stop, Next, Label };
  enum class Btn { NoBtn, Prev, Play, Stop, Next, Label };

  Btn buttonAt(int localX) const;
  void loadDirs();
  void saveDirs();
  void scanFiles();
  void playIndex(size_t idx);
  void hidePopup();

  XConnection& m_xconn;
  FontRenderer& m_font;
  EmbeddedFont m_iconFont;

  std::vector<std::string> m_dirs;
  std::vector<std::string> m_files;
  size_t m_current = 0;

  pid_t m_playerPid = -1;
  bool m_paused = false;
  std::string m_trackName;

  unsigned long m_iconColor = 0xffffff;
  unsigned long m_textColor = 0xffffff;

  Window m_popup = None;
  bool m_popupActive = false;
  int m_popupW = 0;
  int m_popupH = 0;
  bool m_escGrabbed = false;
  std::string m_inputBuffer;
  int m_popupDirCount = 0;
  pid_t m_pendingKill = -1;
  time_t m_playStart = 0;

  static constexpr int kWidth = 120;
  static constexpr int kBtnW = 24;
  static constexpr int kPopupWidth = 420;
  static constexpr int kPopupRowH = 22;
  static constexpr int kPopupPad = 8;
  static constexpr int kPopupMaxRows = 8;
};
