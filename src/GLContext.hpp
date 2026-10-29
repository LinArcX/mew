#pragma once

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>

/**
 * @brief RAII wrapper around a GLX OpenGL 3.3 core-profile context.
 *
 * Creates a context on an existing Xlib window. Tries
 * glXCreateContextAttribsARB for a 3.3 core profile and falls back to a
 * legacy glXCreateContext on failure (Mesa on very old hardware will still
 * report GL 3.3 from a compatibility context, which NanoVG's GL3 backend
 * accepts).
 */
class GLContext
{
public:
  GLContext() = default;
  ~GLContext();

  GLContext(const GLContext&) = delete;
  GLContext& operator=(const GLContext&) = delete;

  /**
   * @brief Create a context for the given window.
   * @param pDisplay X display (must outlive this object).
   * @param window Target X window.
   * @param major Requested GL major (default 3).
   * @param minor Requested GL minor (default 3).
   * @return true on success.
   */
  bool create(Display* pDisplay, Window window, int major = 3, int minor = 3);

  /** @brief Destroy the context if created. */
  void destroy();

  /** @brief True if a context was created. */
  bool isValid() const { return m_pContext != nullptr; }

  /** @brief Make this context current on its display/window. */
  void makeCurrent();

  /** @brief Swap front/back buffers. */
  void swapBuffers();

  /** @brief Underlying GLX context (opaque). */
  GLXContext handle() const { return m_pContext; }

  /** @brief Display associated with this context. */
  Display* display() const { return m_pDisplay; }

  /** @brief Window associated with this context. */
  Window window() const { return m_window; }

private:
  Display* m_pDisplay = nullptr;
  Window m_window = None;
  GLXContext m_pContext = nullptr;
  bool m_ownsContext = false;
};
