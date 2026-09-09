#pragma once

#include "Config.hpp"
#include "FontRenderer.hpp"
#include "Types.hpp"
#include "XConnection.hpp"

#include <X11/Xlib.h>
#include <string>
#include <vector>

/**
 * @brief Owns and manipulates all managed client windows.
 *
 * Handles framing, focus, move/resize, maximize/minimize/fullscreen/snap,
 * and titlebar button interaction. Composition only (no inheritance).
 */
class ClientManager
{
public:
  /**
   * @brief Construct a manager bound to an X connection and font renderer.
   * @param xconn Open X connection (must outlive this object).
   * @param font Font renderer for titlebar text.
   */
  ClientManager(XConnection& xconn, FontRenderer& font);

  /**
   * @brief Set panel height used when computing usable screen area.
   * @param height Panel height in pixels.
   */
  void setPanelHeight(int height) { m_panelHeight = height; }

  /**
   * @brief Optional config used for per-app geometry on manage().
   * @param pConfig Config pointer (not owned), or nullptr.
   */
  void setConfig(const Config* pConfig) { m_pConfig = pConfig; }

  /**
   * @brief Optional callback so overlays stay on top after focus/map.
   * @param fn Function pointer, or nullptr to clear.
   */
  void setRaiseOverlay(void (*fn)()) { m_raiseOverlay = fn; }

  /** @brief Adopt a top-level window into a decorated frame. */
  void manage(Window window);

  /** @brief Release client, reparent to root, destroy frame, delete pointer. */
  void unmanage(Client* pClient);

  /** @brief Find client by window or frame id; nullptr if unknown. */
  Client* findClient(Window window);

  /** @brief Managed client with input focus, or nullptr. */
  Client* focusedClient();

  /** @brief Raise, unminimize if needed, set input focus. */
  void focus(Client* pClient);

  /** @brief Focus next non-minimized client. */
  void focusNext();

  /** @brief Close via WM_DELETE_WINDOW or XKillClient. */
  void close(Client* pClient);

  /** @brief Minimize (unmap frame) and focus another. */
  void minimize(Client* pClient);

  /** @brief Toggle maximized geometry within usable area. */
  void maximize(Client* pClient);

  /**
   * @brief Enter or leave borderless fullscreen.
   * @param enable true = fullscreen, false = restore.
   */
  void setFullscreen(Client* pClient, bool enable);

  /**
   * @brief Snap to screen edge half.
   * @param edge "left", "right", "top", or "bottom".
   */
  void snap(Client* pClient, const std::string& edge);

  /** @brief Resize to 2/3 screen and center. */
  void center(Client* pClient);

  /** @brief Interactive titlebar drag until release. */
  void move(Client* pClient);

  /** @brief Interactive border resize until release. */
  void resizeInteractive(Client* pClient, ResizeDirection direction);

  /** @brief Apply geometry to frame/client and redraw decorations. */
  void resize(Client* pClient);

  /** @brief Paint frame decorations and title. */
  void drawFrame(Client* pClient);

  /** @brief Handle frame ButtonPress (buttons, drag, resize). */
  void handleButtonPress(XButtonEvent* pEvent);

  /** @brief Handle frame MotionNotify (resize cursor). */
  void handleMotion(XMotionEvent* pEvent);

  /** @brief Mutable client list. */
  std::vector<Client*>& clients() { return m_clients; }

  /** @brief Const client list. */
  const std::vector<Client*>& clients() const { return m_clients; }

  /** @brief Screen height minus panel height. */
  int usableHeight() const;

private:
  /** @brief Read window title from X properties. */
  std::string windowTitle(Window window);

  /** @brief Which resize edge contains (x,y) in frame coords. */
  ResizeDirection resizeDirection(Client* pClient, int x, int y);

  /** @brief Cursor for a resize direction. */
  Cursor cursorFor(ResizeDirection direction);

  XConnection& m_xconn;
  FontRenderer& m_font;
  std::vector<Client*> m_clients;
  int m_panelHeight = MewConst::panelHeight;
  void (*m_raiseOverlay)() = nullptr;
  const Config* m_pConfig = nullptr;
};
