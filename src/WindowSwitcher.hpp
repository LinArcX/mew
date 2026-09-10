#pragma once

#include "ClientManager.hpp"
#include "FontRenderer.hpp"
#include "XConnection.hpp"

#include <X11/Xlib.h>
#include <vector>

/**
 * @brief Alt+Tab window switcher popup.
 *
 * Shows a centered list of managed clients. Cycles selection while Alt is held
 * and focuses the selected client when Alt is released.
 */
class WindowSwitcher
{
public:
  /**
   * @brief Construct a switcher bound to X11 and client state.
   * @param xconn Open X connection.
   * @param font Font renderer used for list labels.
   * @param clients Client manager providing the window list and focus API.
   */
  WindowSwitcher(XConnection& xconn, FontRenderer& font, ClientManager& clients);

  /**
   * @brief Destroy the switcher window if it exists.
   */
  ~WindowSwitcher();

  /**
   * @brief Cycle the switcher forward or backward.
   *
   * On the first call while inactive, builds the list and shows the popup.
   * Subsequent calls move the highlight.
   *
   * @param reverse If true, cycle backward (Alt+Shift+Tab).
   */
  void cycle(bool reverse);

  /**
   * @brief Hide the popup, release the keyboard grab, and clear state.
   */
  void hide();

  /**
   * @brief Redraw the switcher contents (e.g. on Expose).
   */
  void draw();

  /**
   * @brief Commit the current selection: focus that client and hide.
   */
  void commit();

  /**
   * @brief Whether the switcher popup is currently mapped.
   */
  bool isActive() const { return m_active; }

  /**
   * @brief X window id of the popup, or None.
   */
  Window window() const { return m_window; }

  /**
   * @brief True if either Alt key is currently pressed.
   */
  bool isAltHeld() const;

private:
  void show();

  std::string titleFor(Client* pClient) const;

  /** @brief Rebuild m_mru from managed clients and move focused client to front. */
  void syncMru();

  XConnection& m_xconn;
  FontRenderer& m_font;
  ClientManager& m_clients;

  Window m_window = None;
  bool m_active = false;
  size_t m_index = 0;
  std::vector<Client*> m_list;

  // most-recently-used order, front = top
  std::vector<Client*> m_mru;   

  static constexpr int kWidth = 420;
  static constexpr int kLineH = 30;
  static constexpr int kPad = 12;
};
