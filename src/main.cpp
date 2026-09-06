#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/stat.h>

struct Client {
  Window window;
  Window frame;

  int x, y;
  int width, height;

  int old_x, old_y;
  int old_width, old_height;

  bool maximized;
  bool minimized;

  Time last_title_click;
};

struct KeyBinding {
  KeyCode keycode;
  unsigned int modifiers;
  std::string command;
};

enum ResizeDirection {
  RESIZE_NONE,
  RESIZE_LEFT,
  RESIZE_RIGHT,
  RESIZE_TOP,
  RESIZE_BOTTOM,
  RESIZE_TOP_LEFT,
  RESIZE_TOP_RIGHT,
  RESIZE_BOTTOM_LEFT,
  RESIZE_BOTTOM_RIGHT
};

static Display* display = nullptr;
static Window root;
static int screen;

static std::vector<Client*> clients;
static std::vector<KeyBinding> keybindings;

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

static void focus_next();

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static Client* find_client(Window window)
{
  for (Client* client : clients) {
    if (client->window == window ||
        client->frame == window) {
        return client;
    }
  }
  return nullptr;
}

static Client* get_focused_client()
{
  Window focused;
  int revert;

  XGetInputFocus(display, &focused, &revert);

  return find_client(focused);
}

static std::string trim(const std::string& str)
{
  size_t start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }

  size_t end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

static std::string expand_home(const std::string& path)
{
  if (path == "~") {
    return std::string(getenv("HOME"));
  }

  if (path.rfind("~/", 0) == 0) {
    const char* home = getenv("HOME");
    if (!home) {
      return path;
    }
    return std::string(home) + path.substr(1);
  }
  return path;
}

// ------------------------------------------------------------
// Config
// ------------------------------------------------------------
static std::string get_config_directory()
{
  const char* home = getenv("HOME");
  if (!home) {
    return "";
  }
  return std::string(home) + "/.config/mew";
}

static void create_config_directory()
{
  const char* home = getenv("HOME");
  if (!home) {
    return;
  }

  std::string config = std::string(home) + "/.config";
  std::string mew = config + "/mew";

  mkdir(config.c_str(), 0755);
  mkdir(mew.c_str(), 0755);
}

static bool parse_keybinding(
  const std::string& line,
  std::string& key,
  std::string& command)
{
  /*
    Expected format:
    key="W-q", command="~/power_manager.sh"
  */

  size_t key_pos = line.find("key=");
  if (key_pos == std::string::npos) {
    return false;
  }

  size_t key_start = line.find('"', key_pos);
  if (key_start == std::string::npos) {
    return false;
  }

  size_t key_end = line.find('"', key_start + 1);
  if (key_end == std::string::npos) {
    return false;
  }

  key = line.substr(key_start + 1, key_end - key_start - 1);

  size_t command_pos = line.find("command=", key_end);
  if (command_pos == std::string::npos) {
    return false;
  }

  size_t command_start = line.find('"', command_pos);
  if (command_start == std::string::npos) {
    return false;
  }

  size_t command_end = line.find('"', command_start + 1);
  if (command_end == std::string::npos) {
    return false;
  }

  command = line.substr(command_start + 1, command_end - command_start - 1);
  return true;
}

static bool parse_key(
  const std::string& key_string,
  unsigned int& modifiers,
  std::string& key_name)
{
  modifiers = 0;

  std::stringstream ss(key_string);
  std::string part;
  std::vector<std::string> parts;
  while (std::getline(ss, part, '-')) {
    parts.push_back(part);
  }

  if (parts.empty()) {
    return false;
  }

  /*
    Everything except the last component is considered
    a modifier.

    Examples:

    W-q
    A-F4
    C-S-q
    W-S-q
  */

  for (size_t i = 0; i + 1 < parts.size(); ++i) {
    const std::string& modifier = parts[i];

    if (modifier == "W") {
        modifiers |= Mod4Mask;
    }
    else if (modifier == "A") {
        modifiers |= Mod1Mask;
    }
    else if (modifier == "C") {
        modifiers |= ControlMask;
    }
    else if (modifier == "S") {
        modifiers |= ShiftMask;
    }
    else {
      fprintf(stderr, "mew: unknown modifier '%s'\n", modifier.c_str());
      return false;
    }
  }

  key_name = parts.back();
  return true;
}

static void load_keybindings()
{
  keybindings.clear();

  std::string path = get_config_directory() + "/keybindings";
  std::ifstream file(path);
  if (!file.is_open()) {
    fprintf(stderr, "mew: no keybindings file: %s\n", path.c_str());
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    line = trim(line);

    if (line.empty()) {
      continue;
    }

    if (line[0] == '#') {
      continue;
    }

    std::string key_string;
    std::string command;

    if (!parse_keybinding(line, key_string, command)) {
      fprintf(stderr, "mew: invalid keybinding: %s\n", line.c_str());
      continue;
    }

    unsigned int modifiers;
    std::string key_name;

    if (!parse_key(key_string, modifiers, key_name)) {
      continue;
    }

    KeySym keysym = XStringToKeysym(key_name.c_str());

    if (keysym == NoSymbol) {
      fprintf(stderr, "mew: unknown key: %s\n", key_name.c_str());
      continue;
    }

    KeyCode keycode = XKeysymToKeycode(display, keysym);
    if (keycode == 0) {
      fprintf(stderr, "mew: cannot find keycode: %s\n", key_name.c_str());
      continue;
    }

    KeyBinding binding;
    binding.keycode = keycode;
    binding.modifiers = modifiers;
    binding.command = expand_home(command);

    keybindings.push_back(binding);

    printf("mew: keybinding %s -> %s\n", key_string.c_str(), binding.command.c_str());
  }
}

static void run_autostart()
{
  std::string path = get_config_directory() + "/autostart";

  std::ifstream file(path);

  if (!file.is_open()) {
    fprintf(stderr, "mew: no autostart file: %s\n", path.c_str());
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
      line = trim(line);

      if (line.empty()) {
        continue;
      }

      if (line[0] == '#') {
        continue;
      }

      printf("mew: autostart: %s\n", line.c_str());

      /*
        Run asynchronously so one program doesn't block
        the window manager.
      */

      std::string command = line + " >/dev/null 2>&1 &";
      std::system(command.c_str());
  }
}

// ------------------------------------------------------------
// Drawing
// ------------------------------------------------------------
static void draw_frame(Client* client)
{
    XSetWindowAttributes attrs;

    GC gc = XCreateGC(
        display,
        client->frame,
        0,
        nullptr
    );

    XSetForeground(
        display,
        gc,
        COLOR_BORDER
    );

    XFillRectangle(
        display,
        client->frame,
        gc,
        0,
        0,
        client->width + BORDER_WIDTH * 2,
        client->height + TITLE_HEIGHT + BORDER_WIDTH
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
        BORDER_WIDTH,
        BORDER_WIDTH,
        client->width,
        TITLE_HEIGHT - BORDER_WIDTH
    );

    int button_y = BORDER_WIDTH;

    int close_x =
        BORDER_WIDTH +
        client->width -
        BUTTON_WIDTH;

    int max_x =
        close_x -
        BUTTON_WIDTH;

    int min_x =
        max_x -
        BUTTON_WIDTH;


    /*
        Buttons
    */

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
        button_y,
        BUTTON_WIDTH,
        TITLE_HEIGHT - BORDER_WIDTH
    );
    
    XFillRectangle(
        display,
        client->frame,
        gc,
        max_x,
        button_y,
        BUTTON_WIDTH,
        TITLE_HEIGHT - BORDER_WIDTH
    );
    
    XFillRectangle(
        display,
        client->frame,
        gc,
        close_x,
        button_y,
        BUTTON_WIDTH,
        TITLE_HEIGHT - BORDER_WIDTH
    );

    /*
        Icons
    */

    XSetForeground(
        display,
        gc,
        COLOR_TEXT
    );

    /*
        Minimize icon
    */

    XDrawLine(
        display,
        client->frame,
        gc,
        min_x + 9,
        TITLE_HEIGHT / 2 + 4,
        min_x + BUTTON_WIDTH - 9,
        TITLE_HEIGHT / 2 + 4
    );


    /*
        Maximize / restore icon
    */

    if (client->maximized) {

        XDrawRectangle(
            display,
            client->frame,
            gc,
            max_x + 9,
            8,
            10,
            9
        );

        XDrawRectangle(
            display,
            client->frame,
            gc,
            max_x + 12,
            11,
            10,
            9
        );

    } else {

        XDrawRectangle(
            display,
            client->frame,
            gc,
            max_x + 9,
            8,
            11,
            10
        );
    }


    /*
        Close icon
    */

    XDrawLine(
        display,
        client->frame,
        gc,
        close_x + 9,
        8,
        close_x + BUTTON_WIDTH - 9,
        TITLE_HEIGHT - 9
    );

    XDrawLine(
        display,
        client->frame,
        gc,
        close_x + BUTTON_WIDTH - 9,
        8,
        close_x + 9,
        TITLE_HEIGHT - 9
    );


    /*
        Title
    */

    XSetForeground(
        display,
        gc,
        COLOR_TEXT
    );

    XDrawString(
        display,
        client->frame,
        gc,
        BORDER_WIDTH + 8,
        TITLE_HEIGHT - 9,
        "mew",
        3
    );

    XFreeGC(
        display,
        gc
    );
}


// ------------------------------------------------------------
// Client management
// ------------------------------------------------------------

static void resize_client(Client* client)
{
    XMoveResizeWindow(
        display,
        client->window,
        BORDER_WIDTH,
        TITLE_HEIGHT,
        client->width,
        client->height
    );

    XMoveResizeWindow(
        display,
        client->frame,
        client->x,
        client->y,
        client->width + BORDER_WIDTH * 2,
        client->height + TITLE_HEIGHT + BORDER_WIDTH
    );

    draw_frame(client);
}


static void focus_client(Client* client)
{
    if (!client || client->minimized)
        return;

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

    draw_frame(client);
}

static void close_client(Client* client)
{
    if (!client)
        return;

    Window window = client->window;

    Atom* protocols = nullptr;
    int count = 0;
    bool supports_delete = false;

    if (XGetWMProtocols(display, window, &protocols, &count)) {
        for (int i = 0; i < count; ++i) {
            if (protocols[i] == WM_DELETE_WINDOW) {
                supports_delete = true;
                break;
            }
        }
        if (protocols)
            XFree(protocols);
    }

    if (supports_delete) {
        XEvent event{};
        event.xclient.type         = ClientMessage;
        event.xclient.window       = window;
        event.xclient.message_type = WM_PROTOCOLS;
        event.xclient.format       = 32;
        event.xclient.data.l[0]    = WM_DELETE_WINDOW;
        event.xclient.data.l[1]    = CurrentTime;

        XSendEvent(display, window, False, NoEventMask, &event);
        XFlush(display);
    } else {
        XKillClient(display, window);
    }
}

static void minimize_client(Client* client)
{
    if (!client)
        return;

    client->minimized = true;

    XUnmapWindow(
        display,
        client->frame
    );

    focus_next();
}


static void maximize_client(Client* client)
{
    if (!client)
        return;

    if (!client->maximized) {

        client->old_x = client->x;
        client->old_y = client->y;

        client->old_width = client->width;
        client->old_height = client->height;

        client->x = 0;
        client->y = 0;

        client->width =
            DisplayWidth(display, screen)
            - BORDER_WIDTH * 2;

        client->height =
            DisplayHeight(display, screen)
            - TITLE_HEIGHT
            - BORDER_WIDTH;

        client->maximized = true;

    } else {

        client->x = client->old_x;
        client->y = client->old_y;

        client->width = client->old_width;
        client->height = client->old_height;

        client->maximized = false;
    }

    resize_client(client);
    focus_client(client);
}


// ------------------------------------------------------------
// Resize
// ------------------------------------------------------------

static ResizeDirection get_resize_direction(
    Client* client,
    int x,
    int y)
{
    int frame_width =
        client->width + BORDER_WIDTH * 2;

    int frame_height =
        client->height +
        TITLE_HEIGHT +
        BORDER_WIDTH;

    bool left =
        x <= RESIZE_BORDER;

    bool right =
        x >= frame_width - RESIZE_BORDER;

    bool top =
        y <= RESIZE_BORDER;

    bool bottom =
        y >= frame_height - RESIZE_BORDER;

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


static unsigned int cursor_for_direction(
    ResizeDirection direction)
{
    switch (direction) {

        case RESIZE_LEFT:
        case RESIZE_RIGHT:
            return XC_sb_h_double_arrow;

        case RESIZE_TOP:
        case RESIZE_BOTTOM:
            return XC_sb_v_double_arrow;

        case RESIZE_TOP_LEFT:
        case RESIZE_BOTTOM_RIGHT:
            return XC_top_left_corner;

        case RESIZE_TOP_RIGHT:
        case RESIZE_BOTTOM_LEFT:
            return XC_top_right_corner;

        default:
            return XC_left_ptr;
    }
}


static void update_cursor(
    Client* client,
    int x,
    int y)
{
    ResizeDirection direction =
        get_resize_direction(client, x, y);

    Cursor cursor = XCreateFontCursor(
        display,
        cursor_for_direction(direction)
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
    if (client->maximized)
        return;

    int start_x = client->x;
    int start_y = client->y;

    int start_width = client->width;
    int start_height = client->height;

    Window dummy;
    int root_x;
    int root_y;
    int win_x;
    int win_y;
    unsigned int mask;

    XQueryPointer(
        display,
        root,
        &dummy,
        &dummy,
        &root_x,
        &root_y,
        &win_x,
        &win_y,
        &mask
    );

    XGrabPointer(
        display,
        client->frame,
        False,
        ButtonMotionMask |
        ButtonReleaseMask,
        GrabModeAsync,
        GrabModeAsync,
        None,
        None,
        CurrentTime
    );

    XEvent event;

    while (true) {

        XMaskEvent(
            display,
            ButtonMotionMask |
            ButtonReleaseMask,
            &event
        );

        if (event.type == MotionNotify) {

            int dx =
                event.xmotion.x_root - root_x;

            int dy =
                event.xmotion.y_root - root_y;

            int new_x = start_x;
            int new_y = start_y;

            int new_width = start_width;
            int new_height = start_height;


            if (direction == RESIZE_LEFT ||
                direction == RESIZE_TOP_LEFT ||
                direction == RESIZE_BOTTOM_LEFT) {

                new_x = start_x + dx;
                new_width = start_width - dx;
            }


            if (direction == RESIZE_RIGHT ||
                direction == RESIZE_TOP_RIGHT ||
                direction == RESIZE_BOTTOM_RIGHT) {

                new_width = start_width + dx;
            }


            if (direction == RESIZE_TOP ||
                direction == RESIZE_TOP_LEFT ||
                direction == RESIZE_TOP_RIGHT) {

                new_y = start_y + dy;
                new_height = start_height - dy;
            }


            if (direction == RESIZE_BOTTOM ||
                direction == RESIZE_BOTTOM_LEFT ||
                direction == RESIZE_BOTTOM_RIGHT) {

                new_height = start_height + dy;
            }


            if (new_width < MIN_WIDTH) {

                if (direction == RESIZE_LEFT ||
                    direction == RESIZE_TOP_LEFT ||
                    direction == RESIZE_BOTTOM_LEFT) {

                    new_x -=
                        MIN_WIDTH - new_width;
                }

                new_width = MIN_WIDTH;
            }


            if (new_height < MIN_HEIGHT) {

                if (direction == RESIZE_TOP ||
                    direction == RESIZE_TOP_LEFT ||
                    direction == RESIZE_TOP_RIGHT) {

                    new_y -=
                        MIN_HEIGHT - new_height;
                }

                new_height = MIN_HEIGHT;
            }


            client->x = new_x;
            client->y = new_y;

            client->width = new_width;
            client->height = new_height;

            resize_client(client);
        }

        if (event.type == ButtonRelease)
            break;
    }

    XUngrabPointer(
        display,
        CurrentTime
    );
}


// ------------------------------------------------------------
// Move
// ------------------------------------------------------------

static void move_client(Client* client)
{
    if (client->maximized)
        return;

    Window dummy;

    int root_x;
    int root_y;

    int win_x;
    int win_y;

    unsigned int mask;

    XQueryPointer(
        display,
        root,
        &dummy,
        &dummy,
        &root_x,
        &root_y,
        &win_x,
        &win_y,
        &mask
    );

    int start_x = client->x;
    int start_y = client->y;

    XGrabPointer(
        display,
        client->frame,
        False,
        ButtonMotionMask |
        ButtonReleaseMask,
        GrabModeAsync,
        GrabModeAsync,
        None,
        None,
        CurrentTime
    );

    XEvent event;

    while (true) {

        XMaskEvent(
            display,
            ButtonMotionMask |
            ButtonReleaseMask,
            &event
        );

        if (event.type == MotionNotify) {

            int dx =
                event.xmotion.x_root - root_x;

            int dy =
                event.xmotion.y_root - root_y;

            client->x = start_x + dx;
            client->y = start_y + dy;

            resize_client(client);
        }

        if (event.type == ButtonRelease)
            break;
    }

    XUngrabPointer(
        display,
        CurrentTime
    );
}


// ------------------------------------------------------------
// Button handling
// ------------------------------------------------------------

static void handle_button_press(XButtonEvent* event)
{
    Client* client =
        find_client(event->window);

    if (!client)
        return;

    focus_client(client);

    ResizeDirection direction =
        get_resize_direction(
            client,
            event->x,
            event->y
        );

    if (event->button == Button1 &&
        direction != RESIZE_NONE) {

        resize_window(
            client,
            direction
        );

        return;
    }


    /*
        Titlebar
    */

    if (event->y >= RESIZE_BORDER &&
        event->y < TITLE_HEIGHT) {

        int frame_width =
            client->width +
            BORDER_WIDTH * 2;

        int close_x =
            frame_width -
            BUTTON_WIDTH -
            BORDER_WIDTH;

        int max_x =
            close_x -
            BUTTON_WIDTH;

        int min_x =
            max_x -
            BUTTON_WIDTH;


        /*
            Buttons
        */

        if (event->button == Button1) {

            if (event->x >= min_x &&
                event->x < max_x) {

                minimize_client(client);
                return;
            }

            if (event->x >= max_x &&
                event->x < close_x) {

                maximize_client(client);
                return;
            }

            if (event->x >= close_x) {
                close_client(client);
                return;
            }
        }

        /*
            Double-click titlebar to maximize/restore.
        */

        if (event->button == Button1 &&
            event->x < min_x) {

            Time now = event->time;

            if (client->last_title_click != 0 &&
                now - client->last_title_click < 400) {

                client->last_title_click = 0;

                maximize_client(client);

                return;
            }

            client->last_title_click = now;

            move_client(client);

            return;
        }
    }
}


// ------------------------------------------------------------
// Motion
// ------------------------------------------------------------

static void handle_motion(XMotionEvent* event)
{
    Client* client =
        find_client(event->window);

    if (!client)
        return;

    update_cursor(
        client,
        event->x,
        event->y
    );
}


// ------------------------------------------------------------
// Manage windows
// ------------------------------------------------------------

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

    client->window = window;
    client->frame = XCreateSimpleWindow(
        display,
        root,
        attr.x,
        attr.y,
        attr.width + BORDER_WIDTH * 2,
        attr.height + TITLE_HEIGHT + BORDER_WIDTH,
        0,
        COLOR_BORDER,
        COLOR_TITLE
    );

    client->x = attr.x;
    client->y = attr.y;

    client->width = attr.width;
    client->height = attr.height;

    client->old_x = attr.x;
    client->old_y = attr.y;

    client->old_width = attr.width;
    client->old_height = attr.height;

    client->maximized = false;
    client->minimized = false;

    client->last_title_click = 0;


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


    XSelectInput(display, window, StructureNotifyMask);
    XReparentWindow(
        display,
        window,
        client->frame,
        BORDER_WIDTH,
        TITLE_HEIGHT
    );


    XMapWindow(
        display,
        client->frame
    );

    XMapWindow(
        display,
        window
    );


    clients.push_back(client);

    resize_client(client);

    focus_client(client);
}


static void unmanage(Client* client)
{
    if (!client)
        return;

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

    clients.erase(
        std::remove(
            clients.begin(),
            clients.end(),
            client
        ),
        clients.end()
    );

    delete client;
}


// ------------------------------------------------------------
// Alt+Tab
// ------------------------------------------------------------

static void focus_next()
{
    if (clients.empty())
        return;

    Client* current =
        get_focused_client();

    size_t start = 0;

    if (current) {

        auto it =
            std::find(
                clients.begin(),
                clients.end(),
                current
            );

        if (it != clients.end()) {

            start =
                (it - clients.begin() + 1)
                % clients.size();
        }
    }

    for (size_t i = 0; i < clients.size(); ++i) {

        size_t index =
            (start + i) % clients.size();

        Client* client =
            clients[index];

        if (!client->minimized) {

            focus_client(client);

            return;
        }
    }
}


// ------------------------------------------------------------
// Keybindings
// ------------------------------------------------------------

static void grab_key(
    KeyCode keycode,
    unsigned int modifiers)
{
    unsigned int lock_masks[] = {
        0,
        LockMask,
        Mod2Mask,
        LockMask | Mod2Mask
    };

    for (unsigned int lock : lock_masks) {

        XGrabKey(
            display,
            keycode,
            modifiers | lock,
            root,
            True,
            GrabModeAsync,
            GrabModeAsync
        );
    }
}


static void grab_keys()
{
    /*
        Built-in shortcuts
    */

    grab_key(
        XKeysymToKeycode(
            display,
            XK_Tab
        ),
        Mod1Mask
    );

    grab_key(
        XKeysymToKeycode(
            display,
            XK_F4
        ),
        Mod1Mask
    );

    grab_key(
        XKeysymToKeycode(
            display,
            XK_F1
        ),
        Mod1Mask
    );

    grab_key(
        XKeysymToKeycode(
            display,
            XK_q
        ),
        Mod1Mask | ShiftMask
    );


    /*
        User-configured shortcuts
    */

    for (const KeyBinding& binding : keybindings) {

        grab_key(
            binding.keycode,
            binding.modifiers
        );
    }
}


static bool handle_custom_keybinding(
    XKeyEvent* event)
{
    unsigned int state =
        event->state &
        ~(LockMask | Mod2Mask);

    for (const KeyBinding& binding : keybindings) {

        if (event->keycode ==
                binding.keycode &&
            state ==
                binding.modifiers) {

            std::string command =
                binding.command +
                " >/dev/null 2>&1 &";

            printf(
                "mew: running: %s\n",
                binding.command.c_str()
            );

            std::system(
                command.c_str()
            );

            return true;
        }
    }

    return false;
}


// ------------------------------------------------------------
// X error handler
// ------------------------------------------------------------

static int error_handler(
    Display*,
    XErrorEvent*)
{
    return 0;
}


// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main()
{
    display = XOpenDisplay(nullptr);

    if (!display) {

        fprintf(
            stderr,
            "mew: cannot open display\n"
        );

        return 1;
    }

    screen =
        DefaultScreen(display);

    root =
        RootWindow(display, screen);


    XSetErrorHandler(
        error_handler
    );


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


    /*
        Create ~/.config/mew
    */

    create_config_directory();


    /*
        Load configuration
    */

    load_keybindings();


    /*
        Become the window manager
    */

    XSelectInput(
        display,
        root,
        SubstructureRedirectMask |
        SubstructureNotifyMask |
        ButtonPressMask |
        PropertyChangeMask
    );


    /*
        Grab keyboard shortcuts
    */

    grab_keys();


    /*
        Manage existing windows
    */

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

            if (attr.map_state ==
                    IsViewable &&
                !attr.override_redirect) {

                manage(children[i]);
            }
        }

        if (children)
            XFree(children);
    }


    /*
        Start autostart programs AFTER
        the window manager is initialized.
    */

    run_autostart();


    XSync(
        display,
        False
    );


    /*
        Main event loop
    */

    while (true) {

        XEvent event;

        XNextEvent(
            display,
            &event
        );


        switch (event.type) {

            case MapRequest:
            {
                manage(
                    event.xmaprequest.window
                );

                break;
            }


            case ConfigureRequest:
            {
                Client* client =
                    find_client(
                        event.xconfigurerequest.window
                    );

                if (!client) {

                    XWindowChanges changes;

                    changes.x =
                        event.xconfigurerequest.x;

                    changes.y =
                        event.xconfigurerequest.y;

                    changes.width =
                        event.xconfigurerequest.width;

                    changes.height =
                        event.xconfigurerequest.height;

                    changes.border_width =
                        event.xconfigurerequest.border_width;

                    changes.sibling =
                        event.xconfigurerequest.above;

                    changes.stack_mode =
                        event.xconfigurerequest.detail;

                    XConfigureWindow(
                        display,
                        event.xconfigurerequest.window,
                        event.xconfigurerequest.value_mask,
                        &changes
                    );

                    break;
                }


                if (client->maximized)
                    break;


                if (event.xconfigurerequest.value_mask &
                    CWX)

                    client->x =
                        event.xconfigurerequest.x;

                if (event.xconfigurerequest.value_mask &
                    CWY)

                    client->y =
                        event.xconfigurerequest.y;

                if (event.xconfigurerequest.value_mask &
                    CWWidth)

                    client->width =
                        std::max(
                            MIN_WIDTH,
                            event.xconfigurerequest.width
                        );

                if (event.xconfigurerequest.value_mask &
                    CWHeight)

                    client->height =
                        std::max(
                            MIN_HEIGHT,
                            event.xconfigurerequest.height
                        );


                resize_client(client);

                break;
            }


            case ButtonPress:
            {
                handle_button_press(
                    &event.xbutton
                );

                break;
            }


            case MotionNotify:
            {
                handle_motion(
                    &event.xmotion
                );

                break;
            }


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
              Client* client = find_client(event.xdestroywindow.window);
              if (client) {
                  // Window is already gone – only destroy the frame
                  XDestroyWindow(display, client->frame);
                  clients.erase(
                      std::remove(clients.begin(), clients.end(), client),
                      clients.end()
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

                    unmanage(client);
                }

                break;
            }


            case KeyPress:
            {
                XKeyEvent* key =
                    &event.xkey;


                /*
                    First check user configuration.
                */

                if (handle_custom_keybinding(key))
                    break;


                unsigned int state =
                    key->state &
                    ~(LockMask | Mod2Mask);


                KeySym keysym =
                    XLookupKeysym(
                        key,
                        0
                    );


                /*
                    Alt+Tab
                */

                if (state == Mod1Mask &&
                    keysym == XK_Tab) {

                    focus_next();
                    break;
                }


                /*
                    Alt+F4
                */

                if (state == Mod1Mask &&
                    keysym == XK_F4) {

                    Client* client =
                        get_focused_client();

                    if (client)
                        close_client(client);

                    break;
                }


                /*
                    Alt+F1
                */

                if (state == Mod1Mask &&
                    keysym == XK_F1) {

                    std::system(
                        "wezterm start >/dev/null 2>&1 &"
                    );

                    break;
                }


                /*
                    Alt+Shift+Q
                    Quit mew
                */

                if (state ==
                        (Mod1Mask | ShiftMask) &&
                    keysym == XK_q) {

                    XCloseDisplay(display);

                    return 0;
                }

        break;
      }
    }
  }

  XCloseDisplay(display);
  return 0;
}
