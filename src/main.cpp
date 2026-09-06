#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

struct Client {
    Window window;
    Window frame;

    int x;
    int y;
    int width;
    int height;

    int old_x;
    int old_y;
    int old_width;
    int old_height;

    bool maximized;
    bool minimized;

    Time last_title_click;
};

static Display* display = nullptr;
static Window root;
static int screen;

static std::vector<Client*> clients;

static const int TITLE_HEIGHT = 28;
static const int BORDER_WIDTH = 2;
static const int RESIZE_BORDER = 6;
static const int BUTTON_WIDTH = 30;
static const int MIN_WIDTH = 120;
static const int MIN_HEIGHT = 60;

static const unsigned long COLOR_BORDER = 0x333333;
static const unsigned long COLOR_TITLE  = 0x444444;
static const unsigned long COLOR_BUTTON = 0x555555;
static const unsigned long COLOR_TEXT   = 0xffffff;

static Atom WM_DELETE_WINDOW;
static Atom WM_PROTOCOLS;

enum ResizeDirection {
    RESIZE_NONE = 0,
    RESIZE_LEFT,
    RESIZE_RIGHT,
    RESIZE_TOP,
    RESIZE_BOTTOM,
    RESIZE_TOP_LEFT,
    RESIZE_TOP_RIGHT,
    RESIZE_BOTTOM_LEFT,
    RESIZE_BOTTOM_RIGHT
};

static Client* find_client(Window window)
{
    for (Client* client : clients) {
        if (client->window == window ||
            client->frame == window)
            return client;
    }

    return nullptr;
}

static Client* get_focused_client()
{
    Window focused;
    int revert;

    XGetInputFocus(
        display,
        &focused,
        &revert
    );

    return find_client(focused);
}

static void draw_frame(Client* client)
{
    XWindowAttributes attr;

    if (!XGetWindowAttributes(
            display,
            client->frame,
            &attr))
        return;

    GC gc =
        XCreateGC(
            display,
            client->frame,
            0,
            nullptr
        );

    XSetForeground(
        display,
        gc,
        COLOR_TITLE
    );

    XFillRectangle(
        display,
        client->frame,
        gc,
        0,
        0,
        attr.width,
        TITLE_HEIGHT
    );

    XSetForeground(
        display,
        gc,
        COLOR_BORDER
    );

    XDrawRectangle(
        display,
        client->frame,
        gc,
        0,
        0,
        attr.width - 1,
        attr.height - 1
    );

    int close_x =
        attr.width - BUTTON_WIDTH;

    int max_x =
        attr.width - BUTTON_WIDTH * 2;

    int min_x =
        attr.width - BUTTON_WIDTH * 3;

    XSetForeground(
        display,
        gc,
        COLOR_BUTTON
    );

    XFillRectangle(
        display,
        client->frame,
        gc,
        min_x,
        0,
        BUTTON_WIDTH,
        TITLE_HEIGHT
    );

    XFillRectangle(
        display,
        client->frame,
        gc,
        max_x,
        0,
        BUTTON_WIDTH,
        TITLE_HEIGHT
    );

    XFillRectangle(
        display,
        client->frame,
        gc,
        close_x,
        0,
        BUTTON_WIDTH,
        TITLE_HEIGHT
    );

    XSetForeground(
        display,
        gc,
        COLOR_TEXT
    );

    int min_cx =
        min_x + BUTTON_WIDTH / 2;

    int min_cy =
        TITLE_HEIGHT / 2 + 5;

    XDrawLine(
        display,
        client->frame,
        gc,
        min_cx - 6,
        min_cy,
        min_cx + 6,
        min_cy
    );

    int max_cx =
        max_x + BUTTON_WIDTH / 2;

    int max_cy =
        TITLE_HEIGHT / 2;

    if (!client->maximized) {

        XDrawRectangle(
            display,
            client->frame,
            gc,
            max_cx - 6,
            max_cy - 6,
            12,
            12
        );

    } else {

        XDrawRectangle(
            display,
            client->frame,
            gc,
            max_cx - 4,
            max_cy - 6,
            9,
            9
        );

        XDrawLine(
            display,
            client->frame,
            gc,
            max_cx - 7,
            max_cy - 3,
            max_cx - 7,
            max_cy + 6
        );

        XDrawLine(
            display,
            client->frame,
            gc,
            max_cx - 7,
            max_cy + 6,
            max_cx + 3,
            max_cy + 6
        );
    }

    int close_cx =
        close_x + BUTTON_WIDTH / 2;

    int close_cy =
        TITLE_HEIGHT / 2;

    XDrawLine(
        display,
        client->frame,
        gc,
        close_cx - 6,
        close_cy - 6,
        close_cx + 6,
        close_cy + 6
    );

    XDrawLine(
        display,
        client->frame,
        gc,
        close_cx + 6,
        close_cy - 6,
        close_cx - 6,
        close_cy + 6
    );

    XDrawString(
        display,
        client->frame,
        gc,
        8,
        19,
        "miniwm",
        6
    );

    XFreeGC(
        display,
        gc
    );
}

static void resize_client(Client* client)
{
    int frame_width =
        client->width +
        BORDER_WIDTH * 2;

    int frame_height =
        client->height +
        TITLE_HEIGHT +
        BORDER_WIDTH;

    XMoveResizeWindow(
        display,
        client->frame,
        client->x,
        client->y,
        frame_width,
        frame_height
    );

    XMoveResizeWindow(
        display,
        client->window,
        BORDER_WIDTH,
        TITLE_HEIGHT,
        client->width,
        client->height
    );

    draw_frame(client);
}

static void focus_client(Client* client)
{
    if (!client || client->minimized)
        return;

    XSetInputFocus(
        display,
        client->window,
        RevertToPointerRoot,
        CurrentTime
    );

    XRaiseWindow(
        display,
        client->frame
    );
}

static void close_client(Client* client)
{
    if (!client)
        return;

    Atom* protocols = nullptr;
    int count = 0;

    if (XGetWMProtocols(
            display,
            client->window,
            &protocols,
            &count)) {

        for (int i = 0; i < count; ++i) {

            if (protocols[i] ==
                WM_DELETE_WINDOW) {

                XEvent event{};

                event.xclient.type =
                    ClientMessage;

                event.xclient.window =
                    client->window;

                event.xclient.message_type =
                    WM_PROTOCOLS;

                event.xclient.format = 32;

                event.xclient.data.l[0] =
                    WM_DELETE_WINDOW;

                event.xclient.data.l[1] =
                    CurrentTime;

                XSendEvent(
                    display,
                    client->window,
                    False,
                    NoEventMask,
                    &event
                );

                XFree(protocols);
                return;
            }
        }

        XFree(protocols);
    }

    XKillClient(
        display,
        client->window
    );
}

static void minimize_client(Client* client)
{
    if (!client || client->minimized)
        return;

    client->minimized = true;

    XUnmapWindow(
        display,
        client->frame
    );
}

static void maximize_client(Client* client)
{
    if (!client)
        return;

    if (client->maximized) {

        client->x =
            client->old_x;

        client->y =
            client->old_y;

        client->width =
            client->old_width;

        client->height =
            client->old_height;

        client->maximized = false;

        XMapWindow(
            display,
            client->frame
        );

        resize_client(client);
        focus_client(client);

        return;
    }

    client->old_x =
        client->x;

    client->old_y =
        client->y;

    client->old_width =
        client->width;

    client->old_height =
        client->height;

    client->x = 0;
    client->y = 0;

    client->width =
        DisplayWidth(
            display,
            screen
        ) -
        BORDER_WIDTH * 2;

    client->height =
        DisplayHeight(
            display,
            screen
        ) -
        TITLE_HEIGHT -
        BORDER_WIDTH;

    client->maximized = true;

    XMapWindow(
        display,
        client->frame
    );

    resize_client(client);
    focus_client(client);
}

static ResizeDirection get_resize_direction(
    Client* client,
    int x,
    int y)
{
    if (!client ||
        client->maximized)
        return RESIZE_NONE;

    XWindowAttributes attr;

    if (!XGetWindowAttributes(
            display,
            client->frame,
            &attr))
        return RESIZE_NONE;

    int width = attr.width;
    int height = attr.height;

    bool left =
        x <= RESIZE_BORDER;

    bool right =
        x >= width - RESIZE_BORDER;

    bool top =
        y <= RESIZE_BORDER;

    bool bottom =
        y >= height - RESIZE_BORDER;

    if (left && top)
        return RESIZE_TOP_LEFT;

    if (right && top)
        return RESIZE_TOP_RIGHT;

    if (left && bottom)
        return RESIZE_BOTTOM_LEFT;

    if (right && bottom)
        return RESIZE_BOTTOM_RIGHT;

    if (left)
        return RESIZE_LEFT;

    if (right)
        return RESIZE_RIGHT;

    if (top)
        return RESIZE_TOP;

    if (bottom)
        return RESIZE_BOTTOM;

    return RESIZE_NONE;
}

static Cursor cursor_for_direction(
    ResizeDirection direction)
{
    switch (direction) {

    case RESIZE_LEFT:
    case RESIZE_RIGHT:
        return XCreateFontCursor(
            display,
            XC_sb_h_double_arrow
        );

    case RESIZE_TOP:
    case RESIZE_BOTTOM:
        return XCreateFontCursor(
            display,
            XC_sb_v_double_arrow
        );

    case RESIZE_TOP_LEFT:
    case RESIZE_BOTTOM_RIGHT:
        return XCreateFontCursor(
            display,
            XC_top_left_corner
        );

    case RESIZE_TOP_RIGHT:
    case RESIZE_BOTTOM_LEFT:
        return XCreateFontCursor(
            display,
            XC_top_right_corner
        );

    default:
        return XCreateFontCursor(
            display,
            XC_left_ptr
        );
    }
}

static void update_cursor(
    Client* client,
    int x,
    int y)
{
    ResizeDirection direction =
        get_resize_direction(
            client,
            x,
            y
        );

    Cursor cursor =
        cursor_for_direction(
            direction
        );

    XDefineCursor(
        display,
        client->frame,
        cursor
    );

    XFreeCursor(
        display,
        cursor
    );
}

static void resize_window(
    Client* client,
    ResizeDirection direction)
{
    if (!client ||
        direction == RESIZE_NONE ||
        client->maximized)
        return;

    Window child;

    int start_root_x;
    int start_root_y;

    int win_x;
    int win_y;

    unsigned int mask;

    if (!XQueryPointer(
            display,
            root,
            &child,
            &child,
            &start_root_x,
            &start_root_y,
            &win_x,
            &win_y,
            &mask))
        return;

    const int start_x =
        client->x;

    const int start_y =
        client->y;

    const int start_width =
        client->width;

    const int start_height =
        client->height;

    XGrabPointer(
        display,
        client->frame,
        False,
        PointerMotionMask |
        ButtonReleaseMask,
        GrabModeAsync,
        GrabModeAsync,
        None,
        None,
        CurrentTime
    );

    bool resizing = true;

    while (resizing) {

        XEvent event;

        XMaskEvent(
            display,
            PointerMotionMask |
            ButtonReleaseMask |
            ExposureMask,
            &event
        );

        if (event.type ==
            MotionNotify) {

            int dx =
                event.xmotion.x_root -
                start_root_x;

            int dy =
                event.xmotion.y_root -
                start_root_y;

            int new_x = start_x;
            int new_y = start_y;

            int new_width =
                start_width;

            int new_height =
                start_height;

            switch (direction) {

            case RESIZE_LEFT:
                new_x =
                    start_x + dx;
                new_width =
                    start_width - dx;
                break;

            case RESIZE_RIGHT:
                new_width =
                    start_width + dx;
                break;

            case RESIZE_TOP:
                new_y =
                    start_y + dy;
                new_height =
                    start_height - dy;
                break;

            case RESIZE_BOTTOM:
                new_height =
                    start_height + dy;
                break;

            case RESIZE_TOP_LEFT:
                new_x =
                    start_x + dx;
                new_y =
                    start_y + dy;
                new_width =
                    start_width - dx;
                new_height =
                    start_height - dy;
                break;

            case RESIZE_TOP_RIGHT:
                new_y =
                    start_y + dy;
                new_width =
                    start_width + dx;
                new_height =
                    start_height - dy;
                break;

            case RESIZE_BOTTOM_LEFT:
                new_x =
                    start_x + dx;
                new_width =
                    start_width - dx;
                new_height =
                    start_height + dy;
                break;

            case RESIZE_BOTTOM_RIGHT:
                new_width =
                    start_width + dx;
                new_height =
                    start_height + dy;
                break;

            default:
                break;
            }

            if (new_width <
                MIN_WIDTH) {

                if (direction ==
                        RESIZE_LEFT ||
                    direction ==
                        RESIZE_TOP_LEFT ||
                    direction ==
                        RESIZE_BOTTOM_LEFT) {

                    new_x =
                        start_x +
                        start_width -
                        MIN_WIDTH;
                }

                new_width =
                    MIN_WIDTH;
            }

            if (new_height <
                MIN_HEIGHT) {

                if (direction ==
                        RESIZE_TOP ||
                    direction ==
                        RESIZE_TOP_LEFT ||
                    direction ==
                        RESIZE_TOP_RIGHT) {

                    new_y =
                        start_y +
                        start_height -
                        MIN_HEIGHT;
                }

                new_height =
                    MIN_HEIGHT;
            }

            client->x =
                new_x;

            client->y =
                new_y;

            client->width =
                new_width;

            client->height =
                new_height;

            resize_client(client);

        } else if (
            event.type ==
            ButtonRelease) {

            resizing = false;

        } else if (
            event.type ==
            Expose) {

            draw_frame(client);
        }
    }

    XUngrabPointer(
        display,
        CurrentTime
    );
}

static void move_client(Client* client)
{
    if (!client ||
        client->maximized)
        return;

    Window child;

    int start_mouse_x;
    int start_mouse_y;

    int win_x;
    int win_y;

    unsigned int mask;

    if (!XQueryPointer(
            display,
            root,
            &child,
            &child,
            &start_mouse_x,
            &start_mouse_y,
            &win_x,
            &win_y,
            &mask))
        return;

    int start_x =
        client->x;

    int start_y =
        client->y;

    XGrabPointer(
        display,
        client->frame,
        False,
        PointerMotionMask |
        ButtonReleaseMask,
        GrabModeAsync,
        GrabModeAsync,
        None,
        None,
        CurrentTime
    );

    bool moving = true;

    while (moving) {

        XEvent event;

        XMaskEvent(
            display,
            PointerMotionMask |
            ButtonReleaseMask |
            ExposureMask,
            &event
        );

        if (event.type ==
            MotionNotify) {

            int dx =
                event.xmotion.x_root -
                start_mouse_x;

            int dy =
                event.xmotion.y_root -
                start_mouse_y;

            client->x =
                start_x + dx;

            client->y =
                start_y + dy;

            resize_client(client);

        } else if (
            event.type ==
            ButtonRelease) {

            moving = false;

        } else if (
            event.type ==
            Expose) {

            draw_frame(client);
        }
    }

    XUngrabPointer(
        display,
        CurrentTime
    );
}

static void handle_button_press(
    XButtonEvent* event)
{
    Client* client =
        find_client(
            event->window
        );

    if (!client)
        return;

    focus_client(client);

    if (event->window !=
        client->frame)
        return;

    /*
     * Double-click title bar.
     */
    if (event->button == Button1 &&
        event->y >= RESIZE_BORDER &&
        event->y < TITLE_HEIGHT) {

        Time now = event->time;

        if (client->last_title_click != 0 &&
            now -
                client->last_title_click <
                400) {

            client->last_title_click = 0;

            maximize_client(client);
            return;
        }

        client->last_title_click = now;
    }

    int frame_width =
        client->width +
        BORDER_WIDTH * 2;

    /*
     * Resize borders.
     */
    if (event->y >= TITLE_HEIGHT ||
        event->x < RESIZE_BORDER ||
        event->x >=
            frame_width - RESIZE_BORDER) {

        ResizeDirection direction =
            get_resize_direction(
                client,
                event->x,
                event->y
            );

        if (direction !=
            RESIZE_NONE) {

            resize_window(
                client,
                direction
            );

            return;
        }
    }

    if (event->y >= TITLE_HEIGHT)
        return;

    int close_x =
        frame_width -
        BUTTON_WIDTH;

    int max_x =
        frame_width -
        BUTTON_WIDTH * 2;

    int min_x =
        frame_width -
        BUTTON_WIDTH * 3;

    if (event->x >= close_x) {

        close_client(client);
        return;
    }

    if (event->x >= max_x &&
        event->x < close_x) {

        maximize_client(client);
        return;
    }

    if (event->x >= min_x &&
        event->x < max_x) {

        minimize_client(client);
        return;
    }

    if (event->button == Button1)
        move_client(client);
}

static void handle_motion(
    XMotionEvent* event)
{
    Client* client =
        find_client(
            event->window
        );

    if (!client)
        return;

    update_cursor(
        client,
        event->x,
        event->y
    );
}

static void manage(Window window)
{
    if (find_client(window))
        return;

    XWindowAttributes attr;

    if (!XGetWindowAttributes(
            display,
            window,
            &attr))
        return;

    if (attr.override_redirect)
        return;

    Client* client =
        new Client{};

    client->window =
        window;

    client->x =
        attr.x;

    client->y =
        attr.y;

    client->width =
        attr.width;

    client->height =
        attr.height;

    client->old_x =
        attr.x;

    client->old_y =
        attr.y;

    client->old_width =
        attr.width;

    client->old_height =
        attr.height;

    client->maximized =
        false;

    client->minimized =
        false;

    client->last_title_click =
        0;

    client->frame =
        XCreateSimpleWindow(
            display,
            root,
            client->x,
            client->y,
            client->width +
                BORDER_WIDTH * 2,
            client->height +
                TITLE_HEIGHT +
                BORDER_WIDTH,
            BORDER_WIDTH,
            COLOR_BORDER,
            COLOR_TITLE
        );

    XSelectInput(
        display,
        client->frame,
        ExposureMask |
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask
    );

    XAddToSaveSet(
        display,
        window
    );

    XReparentWindow(
        display,
        window,
        client->frame,
        BORDER_WIDTH,
        TITLE_HEIGHT
    );

    XSelectInput(
        display,
        window,
        StructureNotifyMask |
        PropertyChangeMask
    );

    clients.push_back(
        client
    );

    XMapWindow(
        display,
        client->frame
    );

    XMapWindow(
        display,
        client->window
    );

    resize_client(client);
    focus_client(client);
}

static void unmanage(Client* client)
{
    if (!client)
        return;

    Window window =
        client->window;

    XUnmapWindow(
        display,
        window
    );

    XReparentWindow(
        display,
        window,
        root,
        client->x,
        client->y
    );

    XRemoveFromSaveSet(
        display,
        window
    );

    XDestroyWindow(
        display,
        client->frame
    );

    auto it =
        std::find(
            clients.begin(),
            clients.end(),
            client
        );

    if (it != clients.end())
        clients.erase(it);

    delete client;
}

/*
 * Alt+Tab:
 *
 * The current implementation cycles through the
 * managed clients. The next non-minimized client
 * receives focus and is raised.
 */
static void focus_next()
{
    if (clients.empty())
        return;

    Client* current =
        get_focused_client();

    size_t start_index = 0;

    if (current) {

        auto it =
            std::find(
                clients.begin(),
                clients.end(),
                current
            );

        if (it != clients.end()) {

            start_index =
                std::distance(
                    clients.begin(),
                    it
                );

            start_index =
                (start_index + 1) %
                clients.size();
        }
    }

    for (size_t i = 0;
         i < clients.size();
         ++i) {

        Client* client =
            clients[
                (start_index + i) %
                clients.size()
            ];

        if (!client->minimized) {

            focus_client(client);
            return;
        }
    }
}

static void grab_keys()
{
    unsigned int modifiers[] = {
        0,
        LockMask,
        Mod2Mask,
        LockMask | Mod2Mask
    };

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

    KeyCode q =
        XKeysymToKeycode(
            display,
            XK_q
        );

    for (unsigned int extra :
         modifiers) {

        /*
         * Alt+Tab
         */
        XGrabKey(
            display,
            tab,
            Mod1Mask | extra,
            root,
            True,
            GrabModeAsync,
            GrabModeAsync
        );

        /*
         * Alt+F4
         */
        XGrabKey(
            display,
            f4,
            Mod1Mask | extra,
            root,
            True,
            GrabModeAsync,
            GrabModeAsync
        );

        /*
         * Alt+F1
         */
        XGrabKey(
            display,
            f1,
            Mod1Mask | extra,
            root,
            True,
            GrabModeAsync,
            GrabModeAsync
        );

        /*
         * Alt+Shift+Q
         */
        XGrabKey(
            display,
            q,
            Mod1Mask |
            ShiftMask |
            extra,
            root,
            True,
            GrabModeAsync,
            GrabModeAsync
        );
    }
}

static int error_handler(
    Display*,
    XErrorEvent* error)
{
    if (error->error_code ==
        BadAccess) {

        std::fprintf(
            stderr,
            "Another window manager is already running.\n"
        );

        std::exit(1);
    }

    return 0;
}

int main()
{
    display =
        XOpenDisplay(
            nullptr
        );

    if (!display) {

        std::fprintf(
            stderr,
            "Cannot open X display.\n"
        );

        return 1;
    }

    screen =
        DefaultScreen(
            display
        );

    root =
        RootWindow(
            display,
            screen
        );

    XSetErrorHandler(
        error_handler
    );

    WM_PROTOCOLS =
        XInternAtom(
            display,
            "WM_PROTOCOLS",
            False
        );

    WM_DELETE_WINDOW =
        XInternAtom(
            display,
            "WM_DELETE_WINDOW",
            False
        );

    XSelectInput(
        display,
        root,
        SubstructureRedirectMask |
        SubstructureNotifyMask |
        ButtonPressMask |
        PropertyChangeMask
    );

    XSync(
        display,
        False
    );

    grab_keys();

    XSync(
        display,
        False
    );

    Window root_return;
    Window parent_return;

    Window* children = nullptr;

    unsigned int child_count = 0;

    if (XQueryTree(
            display,
            root,
            &root_return,
            &parent_return,
            &children,
            &child_count)) {

        for (unsigned int i = 0;
             i < child_count;
             ++i) {

            XWindowAttributes attr;

            if (!XGetWindowAttributes(
                    display,
                    children[i],
                    &attr))
                continue;

            if (attr.map_state != IsUnmapped &&
                !attr.override_redirect) {

                manage(
                    children[i]
                );
            }
        }

        if (children)
            XFree(children);
    }

    XSync(
        display,
        False
    );

    bool running = true;

    while (running) {

        XEvent event;

        XNextEvent(
            display,
            &event
        );

        switch (event.type) {

        case MapRequest:

            manage(
                event.xmaprequest.window
            );

            break;

        case ConfigureRequest:
        {
            XConfigureRequestEvent* e =
                &event.xconfigurerequest;

            Client* client =
                find_client(
                    e->window
                );

            if (!client) {

                XWindowChanges changes{};

                changes.x =
                    e->x;

                changes.y =
                    e->y;

                changes.width =
                    e->width;

                changes.height =
                    e->height;

                changes.border_width =
                    e->border_width;

                changes.sibling =
                    e->above;

                changes.stack_mode =
                    e->detail;

                XConfigureWindow(
                    display,
                    e->window,
                    e->value_mask,
                    &changes
                );

                break;
            }

            if (!client->maximized) {

                if (e->value_mask &
                    CWWidth)
                    client->width =
                        e->width;

                if (e->value_mask &
                    CWHeight)
                    client->height =
                        e->height;

                if (e->value_mask &
                    CWX)
                    client->x =
                        e->x;

                if (e->value_mask &
                    CWY)
                    client->y =
                        e->y;

                resize_client(client);
            }

            break;
        }

        case ButtonPress:

            handle_button_press(
                &event.xbutton
            );

            break;

        case MotionNotify:

            handle_motion(
                &event.xmotion
            );

            break;

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

        case DestroyNotify:
        {
            Client* client =
                find_client(
                    event.xdestroywindow.window
                );

            if (client) {

                auto it =
                    std::find(
                        clients.begin(),
                        clients.end(),
                        client
                    );

                if (it != clients.end())
                    clients.erase(it);

                if (client->frame)
                    XDestroyWindow(
                        display,
                        client->frame
                    );

                delete client;
            }

            break;
        }

        case UnmapNotify:
        {
            Client* client =
                find_client(
                    event.xunmap.window
                );

            if (client &&
                event.xunmap.window ==
                    client->window) {

                if (!client->minimized)
                    unmanage(client);
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

            bool alt =
                event.xkey.state &
                Mod1Mask;

            bool shift =
                event.xkey.state &
                ShiftMask;

            /*
             * Alt+Tab
             */
            if (alt &&
                key == XK_Tab) {

                focus_next();

            /*
             * Alt+F4
             */
            } else if (
                alt &&
                key == XK_F4) {

                Client* client =
                    get_focused_client();

                if (client)
                    close_client(client);

            /*
             * Alt+F1
             */
            } else if (
                alt &&
                key == XK_F1) {

                std::system(
                    "wezterm start >/dev/null 2>&1 &"
                );

            /*
             * Alt+Shift+Q
             */
            } else if (
                alt &&
                shift &&
                key == XK_q) {

                running = false;
            }

            break;
        }

        default:
            break;
        }
    }

    for (Client* client :
         clients) {

        XUnmapWindow(
            display,
            client->window
        );

        XReparentWindow(
            display,
            client->window,
            root,
            client->x,
            client->y
        );

        XRemoveFromSaveSet(
            display,
            client->window
        );

        XDestroyWindow(
            display,
            client->frame
        );

        delete client;
    }

    clients.clear();

    XSync(
        display,
        False
    );

    XCloseDisplay(
        display
    );

    return 0;
}
