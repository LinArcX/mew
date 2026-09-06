#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>


// ============================================================
// Configuration
// ============================================================

constexpr int TITLE_HEIGHT = 28;
constexpr int BORDER_WIDTH = 2;
constexpr int BUTTON_WIDTH = 28;

constexpr unsigned long COLOR_BORDER  = 0x444444;
constexpr unsigned long COLOR_TITLE   = 0x222222;
constexpr unsigned long COLOR_ACTIVE  = 0x285577;
constexpr unsigned long COLOR_BUTTON  = 0x333333;
constexpr unsigned long COLOR_TEXT    = 0xffffff;
constexpr unsigned long COLOR_CLIENT  = 0xdddddd;


// ============================================================
// Client
// ============================================================

struct Client {
    Window window;       // Application window
    Window frame;        // Outer frame

    int x;
    int y;
    int width;
    int height;

    bool maximized = false;
    bool minimized = false;

    int old_x = 0;
    int old_y = 0;
    int old_width = 0;
    int old_height = 0;
};


// ============================================================
// Globals
// ============================================================

Display* display = nullptr;
Window root = 0;

std::vector<Client*> clients;

Client* focused_client = nullptr;

Atom WM_DELETE_WINDOW;
Atom WM_PROTOCOLS;
Atom WM_STATE;


// ============================================================
// Graphics
// ============================================================

GC gc;

unsigned long color(unsigned long value)
{
    return value;
}


// ============================================================
// Find client
// ============================================================

Client* find_client(Window window)
{
    for (Client* client : clients) {

        if (client->window == window ||
            client->frame == window)
        {
            return client;
        }
    }

    return nullptr;
}


// ============================================================
// Draw decoration
// ============================================================

void draw_frame(Client* client)
{
    if (!client)
        return;

    Window frame = client->frame;

    XWindowAttributes attr;

    if (!XGetWindowAttributes(display, frame, &attr))
        return;

    int width = attr.width;

    // --------------------------------------------------------
    // Background
    // --------------------------------------------------------

    XSetForeground(
        display,
        gc,
        color(COLOR_TITLE)
    );

    XFillRectangle(
        display,
        frame,
        gc,
        0,
        0,
        width,
        TITLE_HEIGHT
    );


    // --------------------------------------------------------
    // Border
    // --------------------------------------------------------

    XSetForeground(
        display,
        gc,
        color(
            client == focused_client
                ? COLOR_ACTIVE
                : COLOR_BORDER
        )
    );

    XDrawRectangle(
        display,
        frame,
        gc,
        0,
        0,
        width - 1,
        attr.height - 1
    );


    // --------------------------------------------------------
    // Title
    // --------------------------------------------------------

    XSetForeground(
        display,
        gc,
        color(COLOR_TEXT)
    );

    const char* title = "miniwm";

    XDrawString(
        display,
        frame,
        gc,
        8,
        19,
        title,
        std::strlen(title)
    );


    // --------------------------------------------------------
    // Buttons
    // --------------------------------------------------------

    int button_y = 0;

    int close_x =
        width - BUTTON_WIDTH;

    int max_x =
        width - BUTTON_WIDTH * 2;

    int min_x =
        width - BUTTON_WIDTH * 3;


    // Minimize
    XSetForeground(
        display,
        gc,
        color(COLOR_BUTTON)
    );

    XFillRectangle(
        display,
        frame,
        gc,
        min_x,
        button_y,
        BUTTON_WIDTH,
        TITLE_HEIGHT
    );

    XSetForeground(
        display,
        gc,
        color(COLOR_TEXT)
    );

    XDrawLine(
        display,
        frame,
        gc,
        min_x + 8,
        17,
        min_x + BUTTON_WIDTH - 8,
        17
    );


    // Maximize
    XSetForeground(
        display,
        gc,
        color(COLOR_BUTTON)
    );

    XFillRectangle(
        display,
        frame,
        gc,
        max_x,
        button_y,
        BUTTON_WIDTH,
        TITLE_HEIGHT
    );

    XSetForeground(
        display,
        gc,
        color(COLOR_TEXT)
    );

    XDrawRectangle(
        display,
        frame,
        gc,
        max_x + 8,
        7,
        BUTTON_WIDTH - 16,
        TITLE_HEIGHT - 14
    );


    // Close
    XSetForeground(
        display,
        gc,
        color(COLOR_BUTTON)
    );

    XFillRectangle(
        display,
        frame,
        gc,
        close_x,
        button_y,
        BUTTON_WIDTH,
        TITLE_HEIGHT
    );

    XSetForeground(
        display,
        gc,
        color(COLOR_TEXT)
    );

    XDrawLine(
        display,
        frame,
        gc,
        close_x + 8,
        7,
        close_x + BUTTON_WIDTH - 8,
        21
    );

    XDrawLine(
        display,
        frame,
        gc,
        close_x + BUTTON_WIDTH - 8,
        7,
        close_x + 8,
        21
    );
}


// ============================================================
// Focus
// ============================================================

void focus_client(Client* client)
{
    if (!client)
        return;

    if (client->minimized)
        return;

    focused_client = client;

    XRaiseWindow(
        display,
        client->frame
    );

    XSetInputFocus(
        display,
        client->window,
        RevertToPointerRoot,
        CurrentTime
    );

    for (Client* c : clients)
        draw_frame(c);

    XFlush(display);
}


// ============================================================
// Close
// ============================================================

void close_client(Client* client)
{
    if (!client)
        return;

    Window window = client->window;

    Atom* protocols = nullptr;
    int count = 0;

    if (XGetWMProtocols(
            display,
            window,
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

            event.xclient.type =
                ClientMessage;

            event.xclient.window =
                window;

            event.xclient.message_type =
                WM_PROTOCOLS;

            event.xclient.format = 32;

            event.xclient.data.l[0] =
                WM_DELETE_WINDOW;

            event.xclient.data.l[1] =
                CurrentTime;

            XSendEvent(
                display,
                window,
                False,
                NoEventMask,
                &event
            );

            return;
        }
    }

    XKillClient(
        display,
        window
    );
}


// ============================================================
// Remove client
// ============================================================

void remove_client(Client* client)
{
    if (!client)
        return;

    clients.erase(
        std::remove(
            clients.begin(),
            clients.end(),
            client
        ),
        clients.end()
    );

    if (focused_client == client)
        focused_client = nullptr;

    XDestroyWindow(
        display,
        client->frame
    );

    delete client;
}


// ============================================================
// Minimize
// ============================================================

void minimize_client(Client* client)
{
    if (!client)
        return;

    if (client->minimized)
        return;

    client->minimized = true;

    XUnmapWindow(
        display,
        client->frame
    );

    if (focused_client == client)
        focused_client = nullptr;

    for (Client* c : clients) {

        if (!c->minimized) {
            focus_client(c);
            break;
        }
    }
}


// ============================================================
// Maximize / restore
// ============================================================

void maximize_client(Client* client)
{
    if (!client)
        return;

    XWindowAttributes root_attr;

    XGetWindowAttributes(
        display,
        root,
        &root_attr
    );

    if (!client->maximized) {

        client->old_x = client->x;
        client->old_y = client->y;
        client->old_width = client->width;
        client->old_height = client->height;

        client->maximized = true;

        client->x = 0;
        client->y = 0;

        client->width =
            root_attr.width;

        client->height =
            root_attr.height;

    } else {

        client->x = client->old_x;
        client->y = client->old_y;

        client->width =
            client->old_width;

        client->height =
            client->old_height;

        client->maximized = false;
    }

    XMoveResizeWindow(
        display,
        client->frame,
        client->x,
        client->y,
        client->width,
        client->height
    );

    XMoveResizeWindow(
        display,
        client->window,
        BORDER_WIDTH,
        TITLE_HEIGHT,
        client->width - BORDER_WIDTH * 2,
        client->height - TITLE_HEIGHT - BORDER_WIDTH
    );

    draw_frame(client);
}


// ============================================================
// Manage window
// ============================================================

void manage(Window window)
{
    if (find_client(window))
        return;

    XWindowAttributes attr;

    if (!XGetWindowAttributes(
            display,
            window,
            &attr))
    {
        return;
    }

    if (attr.override_redirect)
        return;


    Client* client =
        new Client{};

    client->window = window;

    client->x = attr.x;
    client->y = attr.y;

    client->width =
        std::max(attr.width, 100);

    client->height =
        std::max(attr.height, 50);


    // --------------------------------------------------------
    // Create frame
    // --------------------------------------------------------

    client->frame =
        XCreateSimpleWindow(
            display,
            root,
            client->x,
            client->y,
            client->width,
            client->height,
            BORDER_WIDTH,
            COLOR_BORDER,
            COLOR_TITLE
        );


    // --------------------------------------------------------
    // Select events on frame
    // --------------------------------------------------------

    XSelectInput(
        display,
        client->frame,
        ExposureMask |
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask |
        EnterWindowMask
    );


    // --------------------------------------------------------
    // Reparent application into frame
    // --------------------------------------------------------

    XReparentWindow(
        display,
        window,
        client->frame,
        BORDER_WIDTH,
        TITLE_HEIGHT
    );


    // --------------------------------------------------------
    // Resize client
    // --------------------------------------------------------

    XResizeWindow(
        display,
        window,
        client->width - BORDER_WIDTH * 2,
        client->height - TITLE_HEIGHT - BORDER_WIDTH
    );


    clients.push_back(client);


    // --------------------------------------------------------
    // Map
    // --------------------------------------------------------

    XMapWindow(
        display,
        client->frame
    );

    XMapWindow(
        display,
        window
    );


    draw_frame(client);

    focus_client(client);

    XFlush(display);
}


// ============================================================
// Arrange
// ============================================================

void arrange()
{
    if (clients.empty())
        return;

    XWindowAttributes root_attr;

    XGetWindowAttributes(
        display,
        root,
        &root_attr
    );

    int screen_width =
        root_attr.width;

    int screen_height =
        root_attr.height;


    std::vector<Client*> visible;

    for (Client* client : clients) {

        if (!client->minimized &&
            !client->maximized)
        {
            visible.push_back(client);
        }
    }

    if (visible.empty())
        return;


    int master_width =
        screen_width * 60 / 100;


    // --------------------------------------------------------
    // One window
    // --------------------------------------------------------

    if (visible.size() == 1) {

        Client* c = visible[0];

        c->x = 0;
        c->y = 0;

        c->width =
            screen_width;

        c->height =
            screen_height;

        XMoveResizeWindow(
            display,
            c->frame,
            c->x,
            c->y,
            c->width,
            c->height
        );

        XMoveResizeWindow(
            display,
            c->window,
            BORDER_WIDTH,
            TITLE_HEIGHT,
            c->width - BORDER_WIDTH * 2,
            c->height - TITLE_HEIGHT - BORDER_WIDTH
        );

        draw_frame(c);

        return;
    }


    // --------------------------------------------------------
    // Master
    // --------------------------------------------------------

    Client* master =
        visible[0];

    master->x = 0;
    master->y = 0;

    master->width =
        master_width;

    master->height =
        screen_height;


    XMoveResizeWindow(
        display,
        master->frame,
        master->x,
        master->y,
        master->width,
        master->height
    );

    XMoveResizeWindow(
        display,
        master->window,
        BORDER_WIDTH,
        TITLE_HEIGHT,
        master->width - BORDER_WIDTH * 2,
        master->height - TITLE_HEIGHT - BORDER_WIDTH
    );


    // --------------------------------------------------------
    // Stack
    // --------------------------------------------------------

    int stack_width =
        screen_width - master_width;

    int stack_count =
        visible.size() - 1;

    int height =
        screen_height / stack_count;


    for (std::size_t i = 1;
         i < visible.size();
         ++i)
    {
        Client* c =
            visible[i];

        int y =
            (i - 1) * height;

        int h = height;

        if (i == visible.size() - 1)
            h = screen_height - y;


        c->x = master_width;
        c->y = y;

        c->width =
            stack_width;

        c->height =
            h;


        XMoveResizeWindow(
            display,
            c->frame,
            c->x,
            c->y,
            c->width,
            c->height
        );

        XMoveResizeWindow(
            display,
            c->window,
            BORDER_WIDTH,
            TITLE_HEIGHT,
            c->width - BORDER_WIDTH * 2,
            c->height - TITLE_HEIGHT - BORDER_WIDTH
        );

        draw_frame(c);
    }

    draw_frame(master);

    XFlush(display);
}


// ============================================================
// Move window
// ============================================================

void move_client(
    Client* client,
    int root_x,
    int root_y,
    int click_x,
    int click_y)
{
    if (!client)
        return;

    if (client->maximized)
        return;

    client->x =
        root_x - click_x;

    client->y =
        root_y - click_y;

    XMoveWindow(
        display,
        client->frame,
        client->x,
        client->y
    );
}


// ============================================================
// Button handling
// ============================================================

void handle_button_press(
    XButtonEvent* event)
{
    Client* client =
        find_client(event->window);

    if (!client)
        return;

    focus_client(client);


    int width =
        client->width;


    int close_x =
        width - BUTTON_WIDTH;

    int max_x =
        width - BUTTON_WIDTH * 2;

    int min_x =
        width - BUTTON_WIDTH * 3;


    // --------------------------------------------------------
    // Close
    // --------------------------------------------------------

    if (event->y < TITLE_HEIGHT &&
        event->x >= close_x)
    {
        close_client(client);
        return;
    }


    // --------------------------------------------------------
    // Maximize
    // --------------------------------------------------------

    if (event->y < TITLE_HEIGHT &&
        event->x >= max_x &&
        event->x < close_x)
    {
        maximize_client(client);
        return;
    }


    // --------------------------------------------------------
    // Minimize
    // --------------------------------------------------------

    if (event->y < TITLE_HEIGHT &&
        event->x >= min_x &&
        event->x < max_x)
    {
        minimize_client(client);
        return;
    }


    // --------------------------------------------------------
    // Title bar drag
    // --------------------------------------------------------

    if (event->y < TITLE_HEIGHT) {

        int start_x =
            event->x_root;

        int start_y =
            event->y_root;

        int offset_x =
            event->x;

        int offset_y =
            event->y;


        XEvent motion;


        while (true) {

            XMaskEvent(
                display,
                ButtonReleaseMask |
                PointerMotionMask,
                &motion
            );


            if (motion.type ==
                MotionNotify)
            {
                move_client(
                    client,
                    motion.xmotion.x_root,
                    motion.xmotion.y_root,
                    offset_x,
                    offset_y
                );

                draw_frame(client);

                XFlush(display);
            }


            if (motion.type ==
                ButtonRelease)
            {
                break;
            }
        }

        return;
    }
}


// ============================================================
// Keyboard
// ============================================================

void focus_next()
{
    if (clients.empty())
        return;

    if (!focused_client) {

        for (Client* c : clients) {

            if (!c->minimized) {
                focus_client(c);
                return;
            }
        }

        return;
    }


    auto it =
        std::find(
            clients.begin(),
            clients.end(),
            focused_client
        );

    if (it == clients.end())
        return;


    std::size_t index =
        std::distance(
            clients.begin(),
            it
        );


    for (std::size_t i = 1;
         i <= clients.size();
         ++i)
    {
        std::size_t next =
            (index + i) % clients.size();

        Client* c =
            clients[next];

        if (!c->minimized) {

            focus_client(c);
            return;
        }
    }
}


void spawn_terminal()
{
    std::system(
        "wezterm start >/dev/null 2>&1 &"
    );
}


void handle_key(XKeyEvent* event)
{
    KeySym key =
        XLookupKeysym(event, 0);


    // Alt
    if (!(event->state & Mod1Mask))
        return;


    // Alt + Tab
    if (key == XK_Tab) {

        focus_next();
        return;
    }


    // Alt + F4
    if (key == XK_F4) {

        if (focused_client)
            close_client(
                focused_client
            );

        return;
    }


    // Alt + F1
    if (key == XK_F1) {

        spawn_terminal();
        return;
    }
}


// ============================================================
// Main
// ============================================================

int main()
{
    // --------------------------------------------------------
    // Open display
    // --------------------------------------------------------

    display =
        XOpenDisplay(nullptr);

    if (!display) {

        std::fprintf(
            stderr,
            "miniwm: cannot open display\n"
        );

        return 1;
    }


    root =
        DefaultRootWindow(display);


    // --------------------------------------------------------
    // Error handler
    // --------------------------------------------------------

    XSetErrorHandler(
        [](Display*, XErrorEvent* event)
        -> int
        {
            if (event->error_code ==
                BadAccess)
            {
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


    // --------------------------------------------------------
    // Become window manager
    // --------------------------------------------------------

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

    WM_STATE =
        XInternAtom(
            display,
            "WM_STATE",
            False
        );


    // --------------------------------------------------------
    // Graphics context
    // --------------------------------------------------------

    gc =
        XCreateGC(
            display,
            root,
            0,
            nullptr
        );


    XSetLineAttributes(
        display,
        gc,
        1,
        LineSolid,
        CapButt,
        JoinMiter
    );


    // --------------------------------------------------------
    // Keyboard grabs
    // --------------------------------------------------------

    KeyCode tab =
        XKeysymToKeycode(
            display,
            XK_Tab
        );

    KeyCode f4 =
        XKeysymToKeycode(
            display,
            XK_F4
        );

    KeyCode f1 =
        XKeysymToKeycode(
            display,
            XK_F1
        );


    XGrabKey(
        display,
        tab,
        Mod1Mask,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );


    XGrabKey(
        display,
        f4,
        Mod1Mask,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );


    XGrabKey(
        display,
        f1,
        Mod1Mask,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );


    XSync(display, False);


    // --------------------------------------------------------
    // Event loop
    // --------------------------------------------------------

    while (true) {

        XEvent event;

        XNextEvent(
            display,
            &event
        );


        switch (event.type) {


        // ====================================================
        // New window
        // ====================================================

        case MapRequest:

            manage(
                event.xmaprequest.window
            );

            break;


        // ====================================================
        // Destroyed
        // ====================================================

        case DestroyNotify:
        {
            Client* client =
                find_client(
                    event.xdestroywindow.window
                );

            if (client)
                remove_client(client);

            break;
        }


        // ====================================================
        // Unmapped
        // ====================================================

        case UnmapNotify:
        {
            Client* client =
                find_client(
                    event.xunmap.window
                );

            if (client &&
                event.xunmap.window ==
                    client->window)
            {
                remove_client(client);
            }

            break;
        }


        // ====================================================
        // Button
        // ====================================================

        case ButtonPress:

            handle_button_press(
                &event.xbutton
            );

            break;


        // ====================================================
        // Keyboard
        // ====================================================

        case KeyPress:

            handle_key(
                &event.xkey
            );

            break;


        // ====================================================
        // Frame exposed
        // ====================================================

        case Expose:
        {
            Client* client =
                find_client(
                    event.xexpose.window
                );

            if (client)
                draw_frame(client);

            break;
        }


        // ====================================================
        // Mouse enters frame
        // ====================================================

        case EnterNotify:
        {
            Client* client =
                find_client(
                    event.xcrossing.window
                );

            if (client)
                focus_client(client);

            break;
        }
        }
    }


    XFreeGC(
        display,
        gc
    );

    XCloseDisplay(
        display
    );

    return 0;
}

//#include <X11/Xlib.h>
//#include <X11/Xatom.h>
//#include <X11/keysym.h>
//
//#include <cstdlib>
//#include <cstdio>
//#include <vector>
//#include <algorithm>
//
//struct Client {
//    Window window;
//};
//
//static Display* display = nullptr;
//static Window root;
//
//static std::vector<Client> clients;
//static std::size_t focused = 0;
//
//static Atom WM_DELETE_WINDOW;
//static Atom WM_PROTOCOLS;
//
//static bool running = true;
//
//
//// ------------------------------------------------------------
//// Utility
//// ------------------------------------------------------------
//
//Client* find_client(Window w)
//{
//    for (auto& c : clients)
//        if (c.window == w)
//            return &c;
//
//    return nullptr;
//}
//
//void focus(Window w)
//{
//    XSetInputFocus(
//        display,
//        w,
//        RevertToPointerRoot,
//        CurrentTime
//    );
//
//    XRaiseWindow(display, w);
//}
//
//void remove_client(Window w)
//{
//    clients.erase(
//        std::remove_if(
//            clients.begin(),
//            clients.end(),
//            [w](const Client& c) {
//                return c.window == w;
//            }
//        ),
//        clients.end()
//    );
//
//    if (focused >= clients.size() && !clients.empty())
//        focused = clients.size() - 1;
//}
//
//
//// ------------------------------------------------------------
//// Window placement
//// ------------------------------------------------------------
//
//void arrange()
//{
//    if (clients.empty())
//        return;
//
//    XWindowAttributes attr;
//    XGetWindowAttributes(display, root, &attr);
//
//    int screen_width  = attr.width;
//    int screen_height = attr.height;
//
//    const int master_width = screen_width * 60 / 100;
//
//    Window master = clients[0].window;
//
//    XMoveResizeWindow(
//        display,
//        master,
//        0,
//        0,
//        master_width,
//        screen_height
//    );
//
//    if (clients.size() == 1)
//        return;
//
//    int stack_width = screen_width - master_width;
//    int stack_count = clients.size() - 1;
//
//    int height = screen_height / stack_count;
//
//    for (std::size_t i = 1; i < clients.size(); ++i) {
//
//        int y = (i - 1) * height;
//
//        int h = height;
//
//        if (i == clients.size() - 1)
//            h = screen_height - y;
//
//        XMoveResizeWindow(
//            display,
//            clients[i].window,
//            master_width,
//            y,
//            stack_width,
//            h
//        );
//    }
//}
//
//
//// ------------------------------------------------------------
//// Manage window
//// ------------------------------------------------------------
//
//void manage(Window w)
//{
//    if (find_client(w))
//        return;
//
//    XWindowAttributes attr;
//
//    if (!XGetWindowAttributes(display, w, &attr))
//        return;
//
//    if (attr.override_redirect)
//        return;
//
//    XSelectInput(
//        display,
//        w,
//        StructureNotifyMask |
//        PropertyChangeMask |
//        FocusChangeMask
//    );
//
//    XMapWindow(display, w);
//
//    clients.push_back({w});
//
//    focused = clients.size() - 1;
//
//    arrange();
//    focus(w);
//}
//
//
//// ------------------------------------------------------------
//// Unmanage
//// ------------------------------------------------------------
//
//void unmanage(Window w)
//{
//    if (!find_client(w))
//        return;
//
//    remove_client(w);
//
//    arrange();
//
//    if (!clients.empty())
//        focus(clients[focused].window);
//}
//
//
//// ------------------------------------------------------------
//// Close window
//// ------------------------------------------------------------
//
//void close_window(Window w)
//{
//    Atom* protocols = nullptr;
//    int count = 0;
//
//    if (XGetWMProtocols(
//            display,
//            w,
//            &protocols,
//            &count))
//    {
//        bool supports_delete = false;
//
//        for (int i = 0; i < count; ++i) {
//            if (protocols[i] == WM_DELETE_WINDOW)
//                supports_delete = true;
//        }
//
//        XFree(protocols);
//
//        if (supports_delete) {
//
//            XEvent event{};
//
//            event.xclient.type = ClientMessage;
//            event.xclient.window = w;
//            event.xclient.message_type = WM_PROTOCOLS;
//            event.xclient.format = 32;
//            event.xclient.data.l[0] =
//                WM_DELETE_WINDOW;
//            event.xclient.data.l[1] =
//                CurrentTime;
//
//            XSendEvent(
//                display,
//                w,
//                False,
//                NoEventMask,
//                &event
//            );
//
//            return;
//        }
//    }
//
//    XKillClient(display, w);
//}
//
//
//// ------------------------------------------------------------
//// Key handling
//// ------------------------------------------------------------
//
//void spawn_terminal()
//{
//    std::system(
//        "wezterm start >/dev/null 2>&1 &"
//    );
//}
//
//void focus_next()
//{
//    if (clients.empty())
//        return;
//
//    focused++;
//
//    if (focused >= clients.size())
//        focused = 0;
//
//    focus(clients[focused].window);
//}
//
//
//// ------------------------------------------------------------
//// Main
//// ------------------------------------------------------------
//
//int main()
//{
//    display = XOpenDisplay(nullptr);
//
//    if (!display) {
//        std::fprintf(
//            stderr,
//            "miniwm: cannot open X display\n"
//        );
//
//        return 1;
//    }
//
//    root = DefaultRootWindow(display);
//
//    // --------------------------------------------------------
//    // Become window manager
//    // --------------------------------------------------------
//
//    XSelectInput(
//        display,
//        root,
//        SubstructureRedirectMask |
//        SubstructureNotifyMask |
//        ButtonPressMask |
//        PointerMotionMask
//    );
//
//    XSync(display, False);
//
//    // --------------------------------------------------------
//    // Check if another WM owns the display
//    // --------------------------------------------------------
//
//    XSetErrorHandler(
//        [](Display*, XErrorEvent* e) -> int {
//
//            if (e->error_code == BadAccess) {
//
//                std::fprintf(
//                    stderr,
//                    "miniwm: another window manager "
//                    "is already running\n"
//                );
//
//                std::exit(1);
//            }
//
//            return 0;
//        }
//    );
//
//    XSelectInput(
//        display,
//        root,
//        SubstructureRedirectMask |
//        SubstructureNotifyMask |
//        ButtonPressMask
//    );
//
//    XSync(display, False);
//
//    // --------------------------------------------------------
//    // Atoms
//    // --------------------------------------------------------
//
//    WM_DELETE_WINDOW =
//        XInternAtom(
//            display,
//            "WM_DELETE_WINDOW",
//            False
//        );
//
//    WM_PROTOCOLS =
//        XInternAtom(
//            display,
//            "WM_PROTOCOLS",
//            False
//        );
//
//    // --------------------------------------------------------
//    // Keyboard shortcuts
//    // --------------------------------------------------------
//
//    unsigned int mod = Mod1Mask; // Alt
//
//    KeyCode tab =
//        XKeysymToKeycode(display, XK_Tab);
//
//    KeyCode f4 =
//        XKeysymToKeycode(display, XK_F4);
//
//    KeyCode f1 =
//        XKeysymToKeycode(display, XK_F1);
//
//    XGrabKey(
//        display,
//        tab,
//        mod,
//        root,
//        True,
//        GrabModeAsync,
//        GrabModeAsync
//    );
//
//    XGrabKey(
//        display,
//        f4,
//        mod,
//        root,
//        True,
//        GrabModeAsync,
//        GrabModeAsync
//    );
//
//    XGrabKey(
//        display,
//        f1,
//        mod,
//        root,
//        True,
//        GrabModeAsync,
//        GrabModeAsync
//    );
//
//    // --------------------------------------------------------
//    // Mouse
//    // --------------------------------------------------------
//
//    XGrabButton(
//        display,
//        1,
//        mod,
//        root,
//        True,
//        ButtonPressMask |
//        ButtonReleaseMask |
//        PointerMotionMask,
//        GrabModeAsync,
//        GrabModeAsync,
//        None,
//        None
//    );
//
//    XGrabButton(
//        display,
//        3,
//        mod,
//        root,
//        True,
//        ButtonPressMask |
//        ButtonReleaseMask |
//        PointerMotionMask,
//        GrabModeAsync,
//        GrabModeAsync,
//        None,
//        None
//    );
//
//    XSync(display, False);
//
//    // --------------------------------------------------------
//    // Event loop
//    // --------------------------------------------------------
//
//    while (running) {
//
//        XEvent event;
//
//        XNextEvent(display, &event);
//
//        switch (event.type) {
//
//        case MapRequest:
//        {
//            manage(
//                event.xmaprequest.window
//            );
//
//            break;
//        }
//
//        case DestroyNotify:
//        {
//            unmanage(
//                event.xdestroywindow.window
//            );
//
//            break;
//        }
//
//        case UnmapNotify:
//        {
//            unmanage(
//                event.xunmap.window
//            );
//
//            break;
//        }
//
//        case ButtonPress:
//        {
//            Window w =
//                event.xbutton.subwindow;
//
//            if (!w)
//                break;
//
//            Client* c = find_client(w);
//
//            if (!c)
//                break;
//
//            focus(w);
//
//            if (event.xbutton.button == 1) {
//
//                XRaiseWindow(display, w);
//
//                XMoveWindow(
//                    display,
//                    w,
//                    event.xbutton.x_root -
//                        event.xbutton.x,
//                    event.xbutton.y_root -
//                        event.xbutton.y
//                );
//            }
//
//            break;
//        }
//
//        case KeyPress:
//        {
//            KeySym key =
//                XLookupKeysym(
//                    &event.xkey,
//                    0
//                );
//
//            if (!(event.xkey.state & mod))
//                break;
//
//            if (key == XK_Tab) {
//
//                focus_next();
//
//            } else if (key == XK_F4) {
//
//                if (!clients.empty())
//                    close_window(
//                        clients[focused].window
//                    );
//
//            } else if (key == XK_F1) {
//
//                spawn_terminal();
//            }
//
//            break;
//        }
//
//        case ButtonRelease:
//        {
//            break;
//        }
//        }
//    }
//
//    XCloseDisplay(display);
//
//    return 0;
//}
