#pragma once

/**
 * @brief Self-contained GLX/OpenGL smoke test for Phase 0 of the NanoVG migration.
 *
 * Opens a small window, creates a GLContext, prints GL_VERSION/GL_RENDERER to
 * stderr, and clears the window to a magenta color at ~60 Hz until ESC/q is
 * pressed or the window is closed. Returns the process exit code.
 *
 * Intended to be run without a WM (native X, or an Xephyr instance) to confirm
 * that GLX 3.3 core context creation and buffer swap work on this system.
 */
int runGlTest();
