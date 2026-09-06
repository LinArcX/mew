#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <algorithm>

struct Client {
    Window window;
};

static Display* display = nullptr;
static Window root;

static std::vector<Client> clients;
static std::size_t focused = 0;

static Atom WM_DELETE_WINDOW;
static Atom WM_PROTOCOLS;

static bool running = true;


// ------------------------------------------------------------
// Utility
// ------------------------------------------------------------

Client* find_client(Window w)
{
    for (auto& c : clients)
        if (c.window == w)
            return &c;

    return nullptr;
}

void focus(Window w)
{
    XSetInputFocus(
        display,
        w,
        RevertToPointerRoot,
        CurrentTime
    );

    XRaiseWindow(display, w);
}

void remove_client(Window w)
{
    clients.erase(
        std::remove_if(
            clients.begin(),
            clients.end(),
            [w](const Client& c) {
                return c.window == w;
            }
        ),
        clients.end()
    );

    if (focused >= clients.size() && !clients.empty())
        focused = clients.size() - 1;
}


// ------------------------------------------------------------
// Window placement
// ------------------------------------------------------------

void arrange()
{
    if (clients.empty())
        return;

    XWindowAttributes attr;
    XGetWindowAttributes(display, root, &attr);

    int screen_width  = attr.width;
    int screen_height = attr.height;

    const int master_width = screen_width * 60 / 100;

    Window master = clients[0].window;

    XMoveResizeWindow(
        display,
        master,
        0,
        0,
        master_width,
        screen_height
    );

    if (clients.size() == 1)
        return;

    int stack_width = screen_width - master_width;
    int stack_count = clients.size() - 1;

    int height = screen_height / stack_count;

    for (std::size_t i = 1; i < clients.size(); ++i) {

        int y = (i - 1) * height;

        int h = height;

        if (i == clients.size() - 1)
            h = screen_height - y;

        XMoveResizeWindow(
            display,
            clients[i].window,
            master_width,
            y,
            stack_width,
            h
        );
    }
}


// ------------------------------------------------------------
// Manage window
// ------------------------------------------------------------

void manage(Window w)
{
    if (find_client(w))
        return;

    XWindowAttributes attr;

    if (!XGetWindowAttributes(display, w, &attr))
        return;

    if (attr.override_redirect)
        return;

    XSelectInput(
        display,
        w,
        StructureNotifyMask |
        PropertyChangeMask |
        FocusChangeMask
    );

    XMapWindow(display, w);

    clients.push_back({w});

    focused = clients.size() - 1;

    arrange();
    focus(w);
}


// ------------------------------------------------------------
// Unmanage
// ------------------------------------------------------------

void unmanage(Window w)
{
    if (!find_client(w))
        return;

    remove_client(w);

    arrange();

    if (!clients.empty())
        focus(clients[focused].window);
}


// ------------------------------------------------------------
// Close window
// ------------------------------------------------------------

void close_window(Window w)
{
    Atom* protocols = nullptr;
    int count = 0;

    if (XGetWMProtocols(
            display,
            w,
            &protocols,
            &count))
    {
        bool supports_delete = false;

        for (int i = 0; i < count; ++i) {
            if (protocols[i] == WM_DELETE_WINDOW)
                supports_delete = true;
        }

        XFree(protocols);

        if (supports_delete) {

            XEvent event{};

            event.xclient.type = ClientMessage;
            event.xclient.window = w;
            event.xclient.message_type = WM_PROTOCOLS;
            event.xclient.format = 32;
            event.xclient.data.l[0] =
                WM_DELETE_WINDOW;
            event.xclient.data.l[1] =
                CurrentTime;

            XSendEvent(
                display,
                w,
                False,
                NoEventMask,
                &event
            );

            return;
        }
    }

    XKillClient(display, w);
}


// ------------------------------------------------------------
// Key handling
// ------------------------------------------------------------

void spawn_terminal()
{
    std::system(
        "wezterm start >/dev/null 2>&1 &"
    );
}

void focus_next()
{
    if (clients.empty())
        return;

    focused++;

    if (focused >= clients.size())
        focused = 0;

    focus(clients[focused].window);
}


// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main()
{
    display = XOpenDisplay(nullptr);

    if (!display) {
        std::fprintf(
            stderr,
            "miniwm: cannot open X display\n"
        );

        return 1;
    }

    root = DefaultRootWindow(display);

    // --------------------------------------------------------
    // Become window manager
    // --------------------------------------------------------

    XSelectInput(
        display,
        root,
        SubstructureRedirectMask |
        SubstructureNotifyMask |
        ButtonPressMask |
        PointerMotionMask
    );

    XSync(display, False);

    // --------------------------------------------------------
    // Check if another WM owns the display
    // --------------------------------------------------------

    XSetErrorHandler(
        [](Display*, XErrorEvent* e) -> int {

            if (e->error_code == BadAccess) {

                std::fprintf(
                    stderr,
                    "miniwm: another window manager "
                    "is already running\n"
                );

                std::exit(1);
            }

            return 0;
        }
    );

    XSelectInput(
        display,
        root,
        SubstructureRedirectMask |
        SubstructureNotifyMask |
        ButtonPressMask
    );

    XSync(display, False);

    // --------------------------------------------------------
    // Atoms
    // --------------------------------------------------------

    WM_DELETE_WINDOW =
        XInternAtom(
            display,
            "WM_DELETE_WINDOW",
            False
        );

    WM_PROTOCOLS =
        XInternAtom(
            display,
            "WM_PROTOCOLS",
            False
        );

    // --------------------------------------------------------
    // Keyboard shortcuts
    // --------------------------------------------------------

    unsigned int mod = Mod1Mask; // Alt

    KeyCode tab =
        XKeysymToKeycode(display, XK_Tab);

    KeyCode f4 =
        XKeysymToKeycode(display, XK_F4);

    KeyCode f1 =
        XKeysymToKeycode(display, XK_F1);

    XGrabKey(
        display,
        tab,
        mod,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    XGrabKey(
        display,
        f4,
        mod,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    XGrabKey(
        display,
        f1,
        mod,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    // --------------------------------------------------------
    // Mouse
    // --------------------------------------------------------

    XGrabButton(
        display,
        1,
        mod,
        root,
        True,
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask,
        GrabModeAsync,
        GrabModeAsync,
        None,
        None
    );

    XGrabButton(
        display,
        3,
        mod,
        root,
        True,
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask,
        GrabModeAsync,
        GrabModeAsync,
        None,
        None
    );

    XSync(display, False);

    // --------------------------------------------------------
    // Event loop
    // --------------------------------------------------------

    while (running) {

        XEvent event;

        XNextEvent(display, &event);

        switch (event.type) {

        case MapRequest:
        {
            manage(
                event.xmaprequest.window
            );

            break;
        }

        case DestroyNotify:
        {
            unmanage(
                event.xdestroywindow.window
            );

            break;
        }

        case UnmapNotify:
        {
            unmanage(
                event.xunmap.window
            );

            break;
        }

        case ButtonPress:
        {
            Window w =
                event.xbutton.subwindow;

            if (!w)
                break;

            Client* c = find_client(w);

            if (!c)
                break;

            focus(w);

            if (event.xbutton.button == 1) {

                XRaiseWindow(display, w);

                XMoveWindow(
                    display,
                    w,
                    event.xbutton.x_root -
                        event.xbutton.x,
                    event.xbutton.y_root -
                        event.xbutton.y
                );
            }

            break;
        }

        case KeyPress:
        {
            KeySym key =
                XLookupKeysym(
                    &event.xkey,
                    0
                );

            if (!(event.xkey.state & mod))
                break;

            if (key == XK_Tab) {

                focus_next();

            } else if (key == XK_F4) {

                if (!clients.empty())
                    close_window(
                        clients[focused].window
                    );

            } else if (key == XK_F1) {

                spawn_terminal();
            }

            break;
        }

        case ButtonRelease:
        {
            break;
        }
        }
    }

    XCloseDisplay(display);

    return 0;
}
