#include "GlTest.hpp"
#include "GLContext.hpp"

#include <GL/gl.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <cstdio>
#include <ctime>

int runGlTest()
{
  Display* d = XOpenDisplay(nullptr);
  if (!d)
  {
    fprintf(stderr, "gl-test: cannot open display\n");
    return 1;
  }

  int screen = DefaultScreen(d);
  int width = 400;
  int height = 300;
  Window root = RootWindow(d, screen);

  XSetWindowAttributes attrs{};
  attrs.background_pixel = BlackPixel(d, screen);
  attrs.event_mask = ExposureMask | KeyPressMask;
  Window win = XCreateWindow(
    d, root,
    (DisplayWidth(d, screen) - width) / 2,
    (DisplayHeight(d, screen) - height) / 2,
    width, height, 0,
    CopyFromParent, InputOutput, CopyFromParent,
    CWBackPixel | CWEventMask, &attrs);
  XStoreName(d, win, "mew --gl-test");

  Atom wmDelete = XInternAtom(d, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(d, win, &wmDelete, 1);

  XMapWindow(d, win);
  XFlush(d);

  GLContext ctx;
  if (!ctx.create(d, win, 3, 3))
  {
    fprintf(stderr, "gl-test: GLContext::create failed\n");
    XDestroyWindow(d, win);
    XCloseDisplay(d);
    return 1;
  }
  ctx.makeCurrent();

  const GLubyte* version = glGetString(GL_VERSION);
  const GLubyte* renderer = glGetString(GL_RENDERER);
  const GLubyte* glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
  fprintf(stderr, "gl-test: GL_VERSION   = %s\n",
          version ? reinterpret_cast<const char*>(version) : "?");
  fprintf(stderr, "gl-test: GL_RENDERER  = %s\n",
          renderer ? reinterpret_cast<const char*>(renderer) : "?");
  fprintf(stderr, "gl-test: GLSL_VERSION = %s\n",
          glslVersion ? reinterpret_cast<const char*>(glslVersion) : "?");
  fprintf(stderr, "gl-test: press ESC or q in the window to exit\n");

  bool running = true;
  while (running)
  {
    while (XPending(d))
    {
      XEvent ev;
      XNextEvent(d, &ev);
      if (ev.type == KeyPress)
      {
        KeySym ks = XLookupKeysym(&ev.xkey, 0);
        if (ks == XK_Escape || ks == XK_q)
        {
          running = false;
        }
      }
      else if (ev.type == ClientMessage &&
               static_cast<Atom>(ev.xclient.data.l[0]) == wmDelete)
      {
        running = false;
      }
    }

    glViewport(0, 0, width, height);
    glClearColor(0.85f, 0.15f, 0.55f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ctx.swapBuffers();

    struct timespec ts = {0, 16L * 1000 * 1000};
    nanosleep(&ts, nullptr);
  }

  ctx.destroy();
  XDestroyWindow(d, win);
  XCloseDisplay(d);
  fprintf(stderr, "gl-test: done\n");
  return 0;
}
