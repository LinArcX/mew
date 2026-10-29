#ifndef GLX_GLXEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#endif

#include "GLContext.hpp"

#include <cstdio>

// Fallback defines for GLX_ARB_create_context constants in case <GL/glx.h>
// on this system is older than the extension. Values fixed by the spec.
#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif
#ifndef GLX_CONTEXT_MINOR_VERSION_ARB
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif
#ifndef GLX_CONTEXT_PROFILE_MASK_ARB
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif
#ifndef GLX_CONTEXT_CORE_PROFILE_BIT_ARB
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

namespace
{
  using PFNCreateContextAttribs = GLXContext (*)(
    Display*, GLXFBConfig, GLXContext, Bool, const int*);
}

GLContext::~GLContext()
{
  destroy();
}

bool GLContext::create(Display* pDisplay, Window window, int major, int minor)
{
  if (!pDisplay || window == None)
  {
    return false;
  }
  if (m_pContext)
  {
    destroy();
  }

  int screen = DefaultScreen(pDisplay);

  int fbAttribs[] = {
    GLX_X_RENDERABLE,  True,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
    GLX_RED_SIZE,      8,
    GLX_GREEN_SIZE,    8,
    GLX_BLUE_SIZE,     8,
    GLX_ALPHA_SIZE,    8,
    GLX_DEPTH_SIZE,    24,
    GLX_STENCIL_SIZE,  8,
    GLX_DOUBLEBUFFER,  True,
    None
  };

  int fbCount = 0;
  GLXFBConfig* pConfigs = glXChooseFBConfig(pDisplay, screen, fbAttribs, &fbCount);
  if (!pConfigs || fbCount == 0)
  {
    fprintf(stderr, "GLContext: no usable GLX FBConfig found\n");
    return false;
  }
  GLXFBConfig config = pConfigs[0];

  XVisualInfo* pVisual = glXGetVisualFromFBConfig(pDisplay, config);
  if (!pVisual)
  {
    fprintf(stderr, "GLContext: glXGetVisualFromFBConfig failed\n");
    XFree(pConfigs);
    return false;
  }

  GLXContext context = nullptr;
  PFNCreateContextAttribs pCreateAttribs =
    reinterpret_cast<PFNCreateContextAttribs>(
      glXGetProcAddressARB(
        reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));

  if (pCreateAttribs)
  {
    int ctxAttribs[] = {
      GLX_CONTEXT_MAJOR_VERSION_ARB, major,
      GLX_CONTEXT_MINOR_VERSION_ARB, minor,
      GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
      None
    };
    context = pCreateAttribs(pDisplay, config, nullptr, True, ctxAttribs);
    if (!context)
    {
      fprintf(stderr,
              "GLContext: glXCreateContextAttribsARB(%d.%d core) failed; "
              "falling back to legacy context\n",
              major, minor);
    }
  }

  if (!context)
  {
    context = glXCreateContext(pDisplay, pVisual, nullptr, True);
  }

  XFree(pVisual);
  XFree(pConfigs);

  if (!context)
  {
    fprintf(stderr, "GLContext: could not create GLX context\n");
    return false;
  }

  m_pDisplay = pDisplay;
  m_window = window;
  m_pContext = context;
  m_ownsContext = true;
  return true;
}

void GLContext::destroy()
{
  if (m_pContext && m_ownsContext && m_pDisplay)
  {
    if (glXGetCurrentContext() == m_pContext)
    {
      glXMakeCurrent(m_pDisplay, None, nullptr);
    }
    glXDestroyContext(m_pDisplay, m_pContext);
  }
  m_pContext = nullptr;
  m_pDisplay = nullptr;
  m_window = None;
  m_ownsContext = false;
}

void GLContext::makeCurrent()
{
  if (m_pContext && m_pDisplay)
  {
    glXMakeCurrent(m_pDisplay, m_window, m_pContext);
  }
}

void GLContext::swapBuffers()
{
  if (m_pDisplay && m_window != None)
  {
    glXSwapBuffers(m_pDisplay, m_window);
  }
}
