#pragma once

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>

/**
 * @brief Owns the X Display connection, root window, atoms, and cursors.
 */
class XConnection
{
public:
  /** @brief Construct a closed connection. */
  XConnection();

  /** @brief Close the Display if open. */
  ~XConnection();

  /**
   * @brief Open the default Display and intern WM atoms.
   * @return true on success.
   */
  bool open();

  /** @brief Close the Display. */
  void close();

  /** @brief X Display pointer. */
  Display* display() const { return m_pDisplay; }
  /** @brief Root window. */
  Window root() const { return m_root; }
  /** @brief Default screen index. */
  int screen() const { return m_screen; }

  /** @brief WM_DELETE_WINDOW atom. */
  Atom atomDeleteWindow() const { return m_atomDeleteWindow; }
  /** @brief WM_PROTOCOLS atom. */
  Atom atomProtocols() const { return m_atomProtocols; }
  /** @brief _NET_WM_NAME atom. */
  Atom atomNetWmName() const { return m_atomNetWmName; }
  /** @brief _NET_WM_STATE atom. */
  Atom atomNetWmState() const { return m_atomNetWmState; }
  /** @brief _NET_WM_STATE_FULLSCREEN atom. */
  Atom atomNetWmStateFullscreen() const { return m_atomNetWmStateFullscreen; }
  /** @brief _NET_WM_STATE_MAXIMIZED_VERT atom. */
  Atom atomNetWmStateMaxVert() const { return m_atomNetWmStateMaxVert; }
  /** @brief _NET_WM_STATE_MAXIMIZED_HORZ atom. */
  Atom atomNetWmStateMaxHorz() const { return m_atomNetWmStateMaxHorz; }

  /** @brief Default pointer cursor. */
  Cursor cursorDefault() const { return m_cursorDefault; }
  /** @brief Horizontal resize cursor. */
  Cursor cursorResizeH() const { return m_cursorResizeH; }
  /** @brief Vertical resize cursor. */
  Cursor cursorResizeV() const { return m_cursorResizeV; }
  /** @brief Top-left resize cursor. */
  Cursor cursorResizeTl() const { return m_cursorResizeTl; }
  /** @brief Top-right resize cursor. */
  Cursor cursorResizeTr() const { return m_cursorResizeTr; }
  /** @brief Bottom-left resize cursor. */
  Cursor cursorResizeBl() const { return m_cursorResizeBl; }
  /** @brief Bottom-right resize cursor. */
  Cursor cursorResizeBr() const { return m_cursorResizeBr; }

  /**
   * @brief Load cursors from theme (with core fallbacks) and set root cursor.
   * @param themeName Xcursor theme name (may be empty).
   * @param size Cursor size in pixels.
   */
  void loadCursors(const char* themeName, int size);

  /**
   * @brief Advertise EWMH support on the root window.
   * Needed so clients (mpv, etc.) send _NET_WM_STATE instead of internal FS.
   */
  void setupEwmh();

  /** @brief Screen width in pixels. */
  int width() const;
  /** @brief Screen height in pixels. */
  int height() const;

  /** @brief _NET_WM_MOVERESIZE atom. */
  Atom atomNetWmMoveResize() const { return m_atomNetWmMoveResize; }

private:
  Cursor loadCursor(const char* themeName, unsigned int fallbackShape);

  Display* m_pDisplay = nullptr;
  Window m_root = None;
  int m_screen = 0;
  Window m_ewmhWmCheck = None;

  Atom m_atomDeleteWindow = None;
  Atom m_atomProtocols = None;
  Atom m_atomNetWmName = None;
  Atom m_atomNetWmState = None;
  Atom m_atomNetWmStateFullscreen = None;
  Atom m_atomNetWmStateMaxVert = None;
  Atom m_atomNetWmStateMaxHorz = None;
  Atom m_atomNetSupported = None;
  Atom m_atomNetSupportingWmCheck = None;
  Atom m_atomNetWmMoveResize = None;

  Cursor m_cursorDefault = None;
  Cursor m_cursorResizeH = None;
  Cursor m_cursorResizeV = None;
  Cursor m_cursorResizeTl = None;
  Cursor m_cursorResizeTr = None;
  Cursor m_cursorResizeBl = None;
  Cursor m_cursorResizeBr = None;
};
