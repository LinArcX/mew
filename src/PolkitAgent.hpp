#pragma once

#include <X11/Xlib.h>

/**
 * @brief Polkit authentication agent that draws an X11 password dialog.
 *
 * Registers itself with the system polkit daemon for the current Unix
 * session. When a privileged action requires authentication, polkit calls
 * the listener, which maps a small modal window, collects the password,
 * and hands it to a PolkitAgentSession that verifies it through PAM and
 * reports the result back to polkit.
 *
 * The password is never stored on disk, never logged, and is cleared from
 * memory as soon as verification completes.
 *
 * Polkit and GObject types are hidden behind void* so that callers do not
 * need to pull in polkit/glib headers.
 */
class PolkitAgent
{
public:
  /**
   * @brief Construct an unregistered agent bound to an X display.
   * @param pDisplay Open X display (must outlive this object).
   * @param screen   Default screen number.
   */
  PolkitAgent(Display* pDisplay, int screen);

  /** @brief Unregister from polkit and destroy internal state. */
  ~PolkitAgent();

  /**
   * @brief Register as the session authentication agent.
   * @return true on success; false if polkit is unavailable or another
   *         agent is already registered for this session.
   */
  bool start();

  /** @brief Unregister and stop listening for requests. */
  void stop();

  /** @brief True if registration succeeded. */
  bool isRegistered() const { return m_registered; }

private:
  Display* m_pDisplay = nullptr;   ///< X display used to draw the dialog.
  int m_screen = 0;                ///< Default screen number.
  bool m_registered = false;       ///< True after a successful registration.
  void* m_pListener = nullptr;     ///< PolkitAgentListener* (opaque).
  void* m_pRegistration = nullptr; ///< Registration handle returned by polkit.
};
