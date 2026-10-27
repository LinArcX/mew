#include "PolkitAgent.hpp"

// libpolkitagent is marked unstable by upstream; defining this symbol
// acknowledges that the ABI may change between polkit releases.
#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE

#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>
#include <gio/gio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

/* Cast helper for the GObject subclass instances. */
#define POLKIT_MEW_AGENT(obj) ((PolkitMewAgent*)(obj))

namespace
{
  /**
   * @brief Minimal X11 password dialog used by the polkit agent.
   */
  class PasswordDialog
  {
  public:
    PasswordDialog(Display* d, int screen)
      : m_pDisplay(d)
      , m_screen(screen)
    {
    }

    ~PasswordDialog()
    {
      if (m_window != None && m_pDisplay)
      {
        XDestroyWindow(m_pDisplay, m_window);
      }
    }

    /**
     * @brief Show the dialog and block until the user submits or cancels.
     * @param actionId Polkit action id (shown as a hint).
     * @param message  Polkit message (shown as a hint).
     * @param outPassword Receives the entered password on success.
     * @return true if the user submitted, false on cancel/close.
     */
    bool run(const char* actionId, const char* message, std::string& outPassword)
    {
      (void)actionId;
      (void)message;

      const int screenW = DisplayWidth(m_pDisplay, m_screen);
      const int screenH = DisplayHeight(m_pDisplay, m_screen);
      const int w = 380;
      const int h = 150;
      const int x = (screenW - w) / 2;
      const int y = (screenH - h) / 2;

      XSetWindowAttributes attrs{};
      attrs.override_redirect = True;
      attrs.background_pixel = 0x1e1e1e;
      attrs.border_pixel = 0x555555;
      attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

      m_window = XCreateWindow(
        m_pDisplay, DefaultRootWindow(m_pDisplay),
        x, y, w, h, 1,
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
        &attrs);
      if (m_window == None)
      {
        return false;
      }

      XStoreName(m_pDisplay, m_window, "Authentication Required");
      XMapRaised(m_pDisplay, m_window);
      XGrabKeyboard(m_pDisplay, m_window, True,
                    GrabModeAsync, GrabModeAsync, CurrentTime);
      XGrabPointer(m_pDisplay, m_window, False,
                   0, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

      m_password.clear();
      bool done = false;
      bool accepted = false;

      while (!done)
      {
        XEvent ev;
        XNextEvent(m_pDisplay, &ev);
        switch (ev.type)
        {
          case Expose:
            draw();
            break;

          case KeyPress:
          {
            KeySym sym = XLookupKeysym(&ev.xkey, 0);
            char buf[8] = {};
            XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &sym, nullptr);

            if (sym == XK_Escape)
            {
              done = true;
              accepted = false;
            }
            else if (sym == XK_Return || sym == XK_KP_Enter)
            {
              done = true;
              accepted = true;
            }
            else if (sym == XK_BackSpace)
            {
              if (!m_password.empty())
              {
                m_password.pop_back();
              }
              draw();
            }
            else if (buf[0] >= 32 && buf[0] < 127)
            {
              m_password.push_back(buf[0]);
              draw();
            }
            break;
          }

          case ButtonPress:
            done = true;
            accepted = false;
            break;

          default:
            break;
        }
      }

      XUngrabPointer(m_pDisplay, CurrentTime);
      XUngrabKeyboard(m_pDisplay, CurrentTime);
      XDestroyWindow(m_pDisplay, m_window);
      m_window = None;

      if (accepted)
      {
        outPassword = m_password;
      }
      if (!m_password.empty())
      {
        std::memset(&m_password[0], 0, m_password.size());
      }
      m_password.clear();

      return accepted;
    }

  private:
    void draw()
    {
      GC gc = XCreateGC(m_pDisplay, m_window, 0, nullptr);

      XSetForeground(m_pDisplay, gc, 0x1e1e1e);
      XFillRectangle(m_pDisplay, m_window, gc, 0, 0, 380, 150);
      XSetForeground(m_pDisplay, gc, 0x555555);
      XDrawRectangle(m_pDisplay, m_window, gc, 0, 0, 379, 149);

      XSetForeground(m_pDisplay, gc, 0xffffff);
      XDrawString(m_pDisplay, m_window, gc, 16, 28,
                  "Authentication Required", 23);
      XDrawString(m_pDisplay, m_window, gc, 16, 52,
                  "Enter your password:", 20);

      XSetForeground(m_pDisplay, gc, 0x2a2a2a);
      XFillRectangle(m_pDisplay, m_window, gc, 16, 66, 348, 28);

      XSetForeground(m_pDisplay, gc, 0xffffff);
      std::string masked(m_password.size(), '*');
      XDrawString(m_pDisplay, m_window, gc, 24, 86,
                  masked.c_str(), static_cast<int>(masked.size()));

      XSetForeground(m_pDisplay, gc, 0x888888);
      XDrawString(m_pDisplay, m_window, gc, 16, 128,
                  "ESC: cancel    Enter: authenticate", 33);

      XFreeGC(m_pDisplay, gc);
    }

    Display* m_pDisplay;
    int m_screen;
    Window m_window = None;
    std::string m_password;
  };

  struct AgentData
  {
    PasswordDialog* pDialog;
  };

  struct SessionData
  {
    std::string password;
    bool success;
    bool completed;
    GMainLoop* pLoop;
  };

}

extern "C" void initiate_authentication(
  PolkitAgentListener* listener,
  const gchar* action_id,
  const gchar* message,
  const gchar* icon_name,
  PolkitDetails* details,
  const gchar* cookie,
  GList* identities,
  GCancellable* cancellable,
  GAsyncReadyCallback callback,
  gpointer user_data);

extern "C" gboolean initiate_authentication_finish(
  PolkitAgentListener* listener,
  GAsyncResult* res,
  GError** error);

struct _PolkitMewAgent
{
  PolkitAgentListener parent_instance;
};

struct _PolkitMewAgentClass
{
  PolkitAgentListenerClass parent_class;
};

typedef struct _PolkitMewAgent PolkitMewAgent;
typedef struct _PolkitMewAgentClass PolkitMewAgentClass;

static void polkit_mew_agent_class_init(gpointer g_class, gpointer class_data)
{
  (void)class_data;
  PolkitAgentListenerClass* lc = POLKIT_AGENT_LISTENER_CLASS(g_class);
  lc->initiate_authentication = initiate_authentication;
  lc->initiate_authentication_finish = initiate_authentication_finish;
}

static void polkit_mew_agent_init(GTypeInstance* instance, gpointer g_class)
{
  (void)instance;
  (void)g_class;
}

static GType polkit_mew_agent_get_type(void)
{
  static volatile gsize type = 0;
  if (g_once_init_enter(const_cast<gsize*>(&type)))
  {
    GTypeInfo info = {
      sizeof(PolkitMewAgentClass),
      nullptr, nullptr,
      polkit_mew_agent_class_init,
      nullptr, nullptr,
      sizeof(PolkitMewAgent),
      0,
      polkit_mew_agent_init,
      nullptr,
    };
    GType t = g_type_register_static(
      POLKIT_AGENT_TYPE_LISTENER, "PolkitMewAgent", &info, (GTypeFlags)0);
    g_once_init_leave(const_cast<gsize*>(&type), t);
  }
  return static_cast<GType>(type);
}

namespace
{
  void on_session_request(
    PolkitAgentSession* session,
    const gchar* request,
    gboolean echo_on,
    gpointer user_data)
  {
    (void)request;
    (void)echo_on;
    SessionData* data = static_cast<SessionData*>(user_data);
    if (data)
    {
      polkit_agent_session_response(session, data->password.c_str());
    }
  }

  void on_session_completed(
    PolkitAgentSession* session,
    gboolean gained_authorization,
    gpointer user_data)
  {
    (void)session;
    SessionData* data = static_cast<SessionData*>(user_data);
    if (!data)
    {
      return;
    }
    data->success = gained_authorization;
    data->completed = true;
    if (data->pLoop)
    {
      g_main_loop_quit(data->pLoop);
    }
  }

  void agent_data_destroy(gpointer p)
  {
    AgentData* ad = static_cast<AgentData*>(p);
    if (ad)
    {
      delete ad->pDialog;
      delete ad;
    }
  }

}

extern "C" void initiate_authentication(
  PolkitAgentListener* listener,
  const gchar* action_id,
  const gchar* message,
  const gchar* icon_name,
  PolkitDetails* details,
  const gchar* cookie,
  GList* identities,
  GCancellable* cancellable,
  GAsyncReadyCallback callback,
  gpointer user_data)
{
  fprintf(stderr,
          "mew: polkit initiate_authentication: action=%s msg=%s\n",
          action_id ? action_id : "(null)",
          message ? message : "(null)");

  (void)icon_name;
  (void)details;
  (void)cancellable;

  GTask* task = g_task_new(listener, nullptr, callback, user_data);

  AgentData* agentData = static_cast<AgentData*>(
    g_object_get_data(G_OBJECT(listener), "mew-data"));
  if (!agentData || !agentData->pDialog)
  {
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "no dialog available");
    g_object_unref(task);
    return;
  }

  if (!identities)
  {
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "no identities");
    g_object_unref(task);
    return;
  }

  PolkitIdentity* identity = nullptr;
  for (GList* l = identities; l != nullptr; l = l->next)
  {
    if (POLKIT_IS_UNIX_USER(l->data))
    {
      identity = POLKIT_IDENTITY(l->data);
      break;
    }
  }
  if (!identity)
  {
    identity = POLKIT_IDENTITY(identities->data);
  }

  std::string password;
  if (!agentData->pDialog->run(action_id, message, password))
  {
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                            "cancelled by user");
    g_object_unref(task);
    return;
  }

  PolkitAgentSession* session = polkit_agent_session_new(identity, cookie);
  if (!session)
  {
    std::memset(&password[0], 0, password.size());
    password.clear();
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "cannot create session");
    g_object_unref(task);
    return;
  }

  SessionData sd{};
  sd.password = password;
  sd.success = false;
  sd.completed = false;
  sd.pLoop = g_main_loop_new(nullptr, FALSE);

  g_signal_connect(session, "request",
                   G_CALLBACK(on_session_request), &sd);
  g_signal_connect(session, "completed",
                   G_CALLBACK(on_session_completed), &sd);

  polkit_agent_session_initiate(session);

  if (!sd.completed)
  {
    g_main_loop_run(sd.pLoop);
  }
  g_main_loop_unref(sd.pLoop);
  g_object_unref(session);

  if (!sd.password.empty())
  {
    std::memset(&sd.password[0], 0, sd.password.size());
  }
  sd.password.clear();

  if (sd.success)
  {
    g_task_return_boolean(task, TRUE);
  }
  else
  {
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "authentication failed");
  }
  g_object_unref(task);
}

extern "C" gboolean initiate_authentication_finish(
  PolkitAgentListener* listener,
  GAsyncResult* res,
  GError** error)
{
  (void)listener;
  return g_task_propagate_boolean(G_TASK(res), error);
}

PolkitAgent::PolkitAgent(Display* pDisplay, int screen)
  : m_pDisplay(pDisplay)
  , m_screen(screen)
{
}

PolkitAgent::~PolkitAgent()
{
  stop();
}

bool PolkitAgent::start()
{
  if (m_registered)
  {
    return true;
  }

  fprintf(stderr, "mew: polkit agent: starting (pid=%d)\n", getpid());

  AgentData* agentData = new AgentData{};
  agentData->pDialog = new PasswordDialog(m_pDisplay, m_screen);

  PolkitAgentListener* listener = POLKIT_AGENT_LISTENER(
    g_object_new(polkit_mew_agent_get_type(), nullptr));
  g_object_set_data_full(
    G_OBJECT(listener), "mew-data", agentData, agent_data_destroy);

  fprintf(stderr, "mew: polkit agent: resolving unix session for pid %d\n",
          getpid());
  PolkitSubject* subject = polkit_unix_session_new_for_process_sync(
    getpid(), nullptr, nullptr);
  if (!subject)
  {
    fprintf(stderr,
            "mew: polkit agent: polkit_unix_session_new_for_process_sync "
            "returned NULL (is mew running inside an active logind session?)\n");
    g_object_unref(listener);
    return false;
  }

  {
    gchar* s = polkit_subject_to_string(subject);
    fprintf(stderr, "mew: polkit agent: session subject: %s\n",
            s ? s : "(null)");
    g_free(s);
  }

  GError* error = nullptr;
  gpointer handle = polkit_agent_listener_register(
    listener,
    POLKIT_AGENT_REGISTER_FLAGS_NONE,
    subject,
    nullptr,
    nullptr,
    &error);

  g_object_unref(subject);

  if (!handle)
  {
    fprintf(stderr, "mew: polkit agent: register failed: %s\n",
            error ? error->message : "unknown");
    g_clear_error(&error);
    g_object_unref(listener);
    return false;
  }

  fprintf(stderr, "mew: polkit agent: registered OK\n");
  m_pListener = listener;
  m_pRegistration = handle;
  m_registered = true;
  return true;
}

void PolkitAgent::stop()
{
  if (m_pRegistration)
  {
    polkit_agent_listener_unregister(m_pRegistration);
    m_pRegistration = nullptr;
  }
  if (m_pListener)
  {
    g_object_unref(m_pListener);
    m_pListener = nullptr;
  }
  m_registered = false;
}
