#include <X11/Xft/Xft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include "font_data.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#include <csignal>
#include <cstring>
#include <unistd.h>

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
  std::string display;   // human-readable form, e.g. "Win-q"
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

// Context menu
static Window context_menu = None;
static bool context_menu_active = false;


static FT_Library ft_library = nullptr;
static FT_Face ft_face = nullptr;

static const int MENU_WIDTH = 180;
static const int MENU_ITEM_HEIGHT = 28;
static const unsigned long COLOR_MENU_BG     = 0x1e1e1e;
static const unsigned long COLOR_MENU_BORDER = 0x555555;
static const unsigned long COLOR_MENU_TEXT   = 0xffffff;

static const std::vector<std::string> context_menu_items = {
    "Show Keybindings",
    "Exit",
};

static XftFont* title_font = nullptr;
static XftColor title_text_color;
static bool title_text_color_ready = false;

// Keybindings viewer window
static Window keybindings_window = None;
static bool keybindings_window_active = false;
//static bool keybindings_window_maximized = false;

// Set by the "Exit" menu item, checked in the main loop.
static volatile bool should_quit = false;

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
static Atom NET_WM_NAME;

// Switcher
static Window switcher = None;
static bool switcher_active = false;
static size_t switcher_index = 0;
static std::vector<Client*> switcher_list;

static const int SWITCHER_WIDTH = 420;
static const int SWITCHER_LINE_H = 30;
static const int SWITCHER_PAD = 12;
static const unsigned long COLOR_SWITCHER_BG     = 0x1e1e1e;
static const unsigned long COLOR_SWITCHER_BORDER = 0x555555;
static const unsigned long COLOR_SWITCHER_HL     = 0x0a64c8;
static const unsigned long COLOR_SWITCHER_TEXT   = 0xffffff;

static void focus_next();

// NEW forward declarations
//static int kb_close_button_x();
//static void move_keybindings_window();
//static void keybindings_window_apply_geometry();

static void hide_context_menu()
{
    if (context_menu != None && context_menu_active) {
        XUnmapWindow(display, context_menu);
    }
    context_menu_active = false;
}

static void draw_context_menu()
{
    if (context_menu == None || !context_menu_active)
        return;

    int height = (int)context_menu_items.size() * MENU_ITEM_HEIGHT;

    GC gc = XCreateGC(display, context_menu, 0, nullptr);

    XSetForeground(display, gc, COLOR_MENU_BG);
    XFillRectangle(display, context_menu, gc, 0, 0, MENU_WIDTH, height);

    XSetForeground(display, gc, COLOR_MENU_BORDER);
    XDrawRectangle(display, context_menu, gc, 0, 0, MENU_WIDTH - 1, height - 1);

    for (size_t i = 0; i < context_menu_items.size(); ++i) {
        int y = (int)i * MENU_ITEM_HEIGHT;

        XSetForeground(display, gc, COLOR_MENU_TEXT);
        XDrawString(display, context_menu, gc,
                    12, y + MENU_ITEM_HEIGHT - 9,
                    context_menu_items[i].c_str(),
                    (int)context_menu_items[i].size());
    }

    XFreeGC(display, gc);
}

static void show_context_menu(int x, int y)
{
    int height = (int)context_menu_items.size() * MENU_ITEM_HEIGHT;

    int screen_w = DisplayWidth(display, screen);
    int screen_h = DisplayHeight(display, screen);

    if (x + MENU_WIDTH > screen_w) x = screen_w - MENU_WIDTH;
    if (y + height > screen_h)     y = screen_h - height;

    if (context_menu == None) {
        XSetWindowAttributes attrs{};
        attrs.override_redirect = True;
        attrs.background_pixel  = COLOR_MENU_BG;
        attrs.border_pixel      = COLOR_MENU_BORDER;
        attrs.event_mask        = ExposureMask | ButtonPressMask;

        context_menu = XCreateWindow(
            display, root,
            x, y, MENU_WIDTH, height,
            1,
            CopyFromParent, InputOutput, CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
            &attrs
        );
    } else {
        XMoveResizeWindow(display, context_menu, x, y, MENU_WIDTH, height);
    }

    XMapRaised(display, context_menu);
    context_menu_active = true;
    draw_context_menu();
}

static int kb_win_x, kb_win_y;
static int kb_win_width, kb_win_height;
//static int kb_win_old_x, kb_win_old_y, kb_win_old_width, kb_win_old_height;

static const int KB_LINE_HEIGHT = 20;
static const int KB_PADDING = 12;
static const int KB_DEFAULT_WIDTH = 560;

static void kb_button_geometry(int& close_x, int& max_x, int& min_x)
{
    close_x = kb_win_width - BUTTON_WIDTH;
}

static void load_title_font()
{
  if (FT_Init_FreeType(&ft_library) != 0) {
    fprintf(stderr, "mew: FT_Init_FreeType failed\n");
    return;
  }

  if (FT_New_Memory_Face(
        ft_library,
        mew_font_ttf,
        (FT_Long)mew_font_ttf_len,
        0,
        &ft_face) != 0) {
    fprintf(stderr, "mew: FT_New_Memory_Face failed to parse embedded font\n");
    return;
  }

  fprintf(
    stderr,
    "mew: loaded embedded font: %s %s\n",
    ft_face->family_name ? ft_face->family_name : "?",
    ft_face->style_name  ? ft_face->style_name  : "?"
  );

  FcPattern* pattern = FcPatternCreate();
  FcPatternAddFTFace(pattern, FC_FT_FACE, ft_face);
  FcPatternAddDouble(pattern, FC_PIXEL_SIZE, 17.0);

  // Force good rendering defaults explicitly — don't rely on Xft.*
  // X resources, since a minimal WM setup like this usually has none set.
  FcPatternAddBool(pattern, FC_ANTIALIAS, FcTrue);
  FcPatternAddBool(pattern, FC_AUTOHINT, FcFalse);
  FcPatternAddBool(pattern, FC_HINTING, FcTrue);
  FcPatternAddInteger(pattern, FC_HINT_STYLE, FC_HINT_SLIGHT);
  FcPatternAddInteger(pattern, FC_RGBA, FC_RGBA_RGB);
  FcPatternAddInteger(pattern, FC_LCD_FILTER, FC_LCD_DEFAULT);

  // Fill in sane rendering defaults (antialiasing, hinting, RGBA order)
  // based on this specific display, same as XftFontOpen would do internally.
  FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
  XftDefaultSubstitute(display, screen, pattern);

  // XftFontOpenPattern wraps our FT_Face directly and does NOT run
  // FcFontMatch against the system font database, so it can't get
  // silently substituted the way XftFontOpen did.
  title_font = XftFontOpenPattern(display, pattern);

  // Note: on success or failure, XftFontOpenPattern takes ownership of
  // `pattern` internally (frees it on failure, stores it on success) —
  // do not call FcPatternDestroy(pattern) yourself either way.

  if (!title_font) {
    fprintf(stderr, "mew: XftFontOpenPattern failed for embedded font\n");
  }
}

//static void load_title_font()
//{
//  int fd = memfd_create("mew-font", 0);
//  if (fd < 0) {
//    fprintf(stderr, "mew: memfd_create failed for embedded font\n");
//    return;
//  }
//
//  ssize_t written = write(fd, mew_font_ttf, mew_font_ttf_len);
//  if (written != (ssize_t)mew_font_ttf_len) {
//    fprintf(stderr, "mew: failed to write embedded font to memfd\n");
//    close(fd);
//    return;
//  }
//
//  char path[64];
//  snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
//
//  title_font = XftFontOpen(
//    display, screen,
//    XFT_FILE, XftTypeString, path,
//    XFT_INDEX, XftTypeInteger, 0,
//    XFT_SIZE, XftTypeDouble, 12.0,
//    nullptr
//  );
//
//  // Deliberately leaked: FreeType may lazily re-read the stream for
//  // the font's lifetime, so the memfd must stay alive for as long as
//  // mew runs. Cost is a few KB, reclaimed automatically on exit.
//
//  if (!title_font) {
//    fprintf(stderr, "mew: failed to open embedded font\n");
//  }
//}

static std::string get_config_directory()
{
  const char* home = getenv("HOME");
  if (!home) {
    return "";
  }
  return std::string(home) + "/.config/mew";
}


static void raise_keybindings_window_if_active()
{
  if (keybindings_window != None && keybindings_window_active) {
    XRaiseWindow(display, keybindings_window);
  }
}

static std::vector<std::string> kb_display_lines;

static void build_keybindings_display()
{
    kb_display_lines.clear();
    kb_display_lines.push_back(get_config_directory() + "/keybindings");
    kb_display_lines.push_back(""); // spacer

    int i = 1;
    for (const KeyBinding& b : keybindings) {
        std::ostringstream oss;
        oss << i << ". " << b.display << " --> " << b.command;
        kb_display_lines.push_back(oss.str());
        ++i;
    }

    if (keybindings.empty()) {
        kb_display_lines.push_back("(no keybindings configured)");
    }
}

static void draw_keybindings_window()
{
    if (keybindings_window == None || !keybindings_window_active)
        return;

    GC gc = XCreateGC(display, keybindings_window, 0, nullptr);

    // Whole window background (border color, matches client frame style)
    XSetForeground(display, gc, COLOR_BORDER);
    XFillRectangle(display, keybindings_window, gc, 0, 0, kb_win_width, kb_win_height);

    // Titlebar
    XSetForeground(display, gc, COLOR_TITLE);
    XFillRectangle(display, keybindings_window, gc,
                   BORDER_WIDTH, BORDER_WIDTH,
                   kb_win_width - BORDER_WIDTH * 2, TITLE_HEIGHT - BORDER_WIDTH);

    int close_x, max_x, min_x;
    kb_button_geometry(close_x, max_x, min_x);

    // Buttons
    XSetForeground(display, gc, COLOR_BUTTON);
    XFillRectangle(display, keybindings_window, gc, min_x, BORDER_WIDTH, BUTTON_WIDTH, TITLE_HEIGHT - BORDER_WIDTH);

    // Icons
    XSetForeground(display, gc, COLOR_TEXT);

    //// Minimize
    //XDrawLine(display, keybindings_window, gc,
    //          min_x + 9, TITLE_HEIGHT / 2 + 4,
    //          min_x + BUTTON_WIDTH - 9, TITLE_HEIGHT / 2 + 4);

    // Maximize / restore
    //if (keybindings_window_maximized) {
    //    XDrawRectangle(display, keybindings_window, gc, max_x + 9, 8, 10, 9);
    //    XDrawRectangle(display, keybindings_window, gc, max_x + 12, 11, 10, 9);
    //} else {
    //    XDrawRectangle(display, keybindings_window, gc, max_x + 9, 8, 11, 10);
    //}

    // Close
    XDrawLine(display, keybindings_window, gc, close_x + 9, 8, close_x + BUTTON_WIDTH - 9, TITLE_HEIGHT - 9);
    XDrawLine(display, keybindings_window, gc, close_x + BUTTON_WIDTH - 9, 8, close_x + 9, TITLE_HEIGHT - 9);

    // Title text
    static const char* title_text = "Keybindings";
    XDrawString(display, keybindings_window, gc, BORDER_WIDTH + 8, TITLE_HEIGHT - 9, title_text, (int)strlen(title_text));

    // Content area background
    XSetForeground(display, gc, COLOR_SWITCHER_BG);
    XFillRectangle(display, keybindings_window, gc,
                   BORDER_WIDTH, TITLE_HEIGHT,
                   kb_win_width - BORDER_WIDTH * 2,
                   kb_win_height - TITLE_HEIGHT - BORDER_WIDTH);

    // Content text
    XSetForeground(display, gc, COLOR_SWITCHER_TEXT);
    int y = TITLE_HEIGHT + KB_PADDING + 12;
    for (const std::string& line : kb_display_lines) {
        if (!line.empty()) {
            XDrawString(display, keybindings_window, gc,
                        BORDER_WIDTH + KB_PADDING, y,
                        line.c_str(), (int)line.size());
        }
        y += KB_LINE_HEIGHT;
        if (y > kb_win_height - BORDER_WIDTH - 4)
            break; // don't draw past the window (no scrolling yet)
    }

    XFreeGC(display, gc);
}

static int kb_close_button_x()
{
    return kb_win_width - BUTTON_WIDTH;
}

static void keybindings_window_apply_geometry()
{
    XMoveResizeWindow(display, keybindings_window, kb_win_x, kb_win_y, kb_win_width, kb_win_height);
    draw_keybindings_window();
}

static void ensure_title_text_color()
{
  if (title_text_color_ready)
    return;

  XRenderColor render_color;
  render_color.red   = 0xffff;
  render_color.green = 0xffff;
  render_color.blue  = 0xffff;
  render_color.alpha = 0xffff;

  XftColorAllocValue(
    display, DefaultVisual(display, screen),
    DefaultColormap(display, screen),
    &render_color, &title_text_color
  );

  title_text_color_ready = true;
}

static void draw_title_text(Window window, int x, int y, const std::string& text)
{
  if (!title_font) {
    // Fallback so a load failure doesn't leave titles blank.
    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, COLOR_TEXT);
    XDrawString(display, window, gc, x, y, text.c_str(), (int)text.size());
    XFreeGC(display, gc);
    return;
  }

  ensure_title_text_color();

  XftDraw* draw = XftDrawCreate(
    display, window,
    DefaultVisual(display, screen),
    DefaultColormap(display, screen)
  );

  XftDrawStringUtf8(
    draw, &title_text_color, title_font,
    x, y,
    (const FcChar8*)text.c_str(), (int)text.size()
  );

  XftDrawDestroy(draw);
}

static void move_keybindings_window()
{
    Window dummy;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;

    XQueryPointer(display, root, &dummy, &dummy,
                  &root_x, &root_y, &win_x, &win_y, &mask);

    int start_x = kb_win_x;
    int start_y = kb_win_y;

    XGrabPointer(
        display, keybindings_window, False,
        ButtonMotionMask | ButtonReleaseMask,
        GrabModeAsync, GrabModeAsync,
        None, None, CurrentTime
    );

    XEvent event;
    while (true) {
        XMaskEvent(display, ButtonMotionMask | ButtonReleaseMask, &event);

        if (event.type == MotionNotify) {
            int dx = event.xmotion.x_root - root_x;
            int dy = event.xmotion.y_root - root_y;

            kb_win_x = start_x + dx;
            kb_win_y = start_y + dy;

            keybindings_window_apply_geometry();
        }

        if (event.type == ButtonRelease)
            break;
    }

    XUngrabPointer(display, CurrentTime);
}

//static void draw_keybindings_window()
//{
//    if (keybindings_window == None || !keybindings_window_active)
//        return;
//
//    GC gc = XCreateGC(display, keybindings_window, 0, nullptr);
//
//    XSetForeground(display, gc, COLOR_SWITCHER_BG);
//    XFillRectangle(display, keybindings_window, gc, 0, 0, 400, 300);
//
//    XSetForeground(display, gc, COLOR_SWITCHER_BORDER);
//    XDrawRectangle(display, keybindings_window, gc, 0, 0, 399, 299);
//
//    XSetForeground(display, gc, COLOR_SWITCHER_TEXT);
//    XDrawString(display, keybindings_window, gc, 12, 24, "Keybindings", 11);
//
//    XFreeGC(display, gc);
//}

//static void show_keybindings_window()
//{
//    int width = 400, height = 300;
//    int screen_w = DisplayWidth(display, screen);
//    int screen_h = DisplayHeight(display, screen);
//    int x = (screen_w - width) / 2;
//    int y = (screen_h - height) / 2;
//
//    if (keybindings_window == None) {
//        XSetWindowAttributes attrs{};
//        attrs.override_redirect = True;
//        attrs.background_pixel  = COLOR_SWITCHER_BG;
//        attrs.border_pixel      = COLOR_SWITCHER_BORDER;
//        attrs.event_mask        = ExposureMask;
//
//        keybindings_window = XCreateWindow(
//            display, root,
//            x, y, width, height,
//            1,
//            CopyFromParent, InputOutput, CopyFromParent,
//            CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
//            &attrs
//        );
//    } else {
//        XMoveResizeWindow(display, keybindings_window, x, y, width, height);
//    }
//
//    XMapRaised(display, keybindings_window);
//    keybindings_window_active = true;
//    draw_keybindings_window();
//}

static void show_keybindings_window()
{
    build_keybindings_display();

    int screen_w = DisplayWidth(display, screen);
    int screen_h = DisplayHeight(display, screen);

    if (keybindings_window == None) {
        kb_win_width = KB_DEFAULT_WIDTH;
        kb_win_height = std::min(
            (int)(TITLE_HEIGHT + BORDER_WIDTH + KB_PADDING * 2 + kb_display_lines.size() * KB_LINE_HEIGHT + 12),
            screen_h - 80
        );
        kb_win_x = (screen_w - kb_win_width) / 2;
        kb_win_y = (screen_h - kb_win_height) / 2;

        XSetWindowAttributes attrs{};
        attrs.override_redirect = True;
        attrs.background_pixel  = COLOR_BORDER;
        attrs.border_pixel      = COLOR_SWITCHER_BORDER;
        attrs.event_mask        = ExposureMask | ButtonPressMask;

        keybindings_window = XCreateWindow(
            display, root,
            kb_win_x, kb_win_y, kb_win_width, kb_win_height,
            1,
            CopyFromParent, InputOutput, CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
            &attrs
        );
    } else {
        // Recompute height in case keybindings changed since last open
        kb_win_height = std::min(
            (int)(TITLE_HEIGHT + BORDER_WIDTH + KB_PADDING * 2 + kb_display_lines.size() * KB_LINE_HEIGHT + 12),
            screen_h - 80
        );
        XMoveResizeWindow(display, keybindings_window, kb_win_x, kb_win_y, kb_win_width, kb_win_height);
    }

    XMapRaised(display, keybindings_window);
    keybindings_window_active = true;
    draw_keybindings_window();
}

static void handle_context_menu_selection(int index)
{
    if (index < 0 || index >= (int)context_menu_items.size())
        return;

    const std::string& label = context_menu_items[index];

    if (label == "Exit") {
        should_quit = true;
    }
    else if (label == "Show Keybindings") {
        show_keybindings_window();
    }
}

static bool is_alt_held()
{
    char keys[32];
    XQueryKeymap(display, keys);

    auto key_pressed = [&](KeySym sym) -> bool {
        KeyCode code = XKeysymToKeycode(display, sym);
        if (code == 0) return false;
        return keys[code / 8] & (1 << (code % 8));
    };

    return key_pressed(XK_Alt_L) || key_pressed(XK_Alt_R);
}

static std::string get_window_title(Window window)
{
    char* name = nullptr;
    if (XFetchName(display, window, &name) && name) {
        std::string title(name);
        XFree(name);
        return title.empty() ? "Untitled" : title;
    }

    XTextProperty prop;
    if (XGetWMName(display, window, &prop) && prop.value) {
        std::string title(reinterpret_cast<char*>(prop.value));
        XFree(prop.value);
        return title.empty() ? "Untitled" : title;
    }
    return "Untitled";
}

static void hide_switcher()
{
    if (switcher != None && switcher_active) {
        XUnmapWindow(display, switcher);
    }

    // NEW: release the keyboard grab we took in show_switcher()
    if (switcher_active) {
        XUngrabKeyboard(display, CurrentTime);
    }

    switcher_active = false;
    switcher_list.clear();
}

static void draw_switcher()
{
    if (switcher == None || !switcher_active || switcher_list.empty())
        return;

    int height = SWITCHER_PAD * 2 + (int)switcher_list.size() * SWITCHER_LINE_H;

    GC gc = XCreateGC(display, switcher, 0, nullptr);

    // Background
    XSetForeground(display, gc, COLOR_SWITCHER_BG);
    XFillRectangle(display, switcher, gc, 0, 0, SWITCHER_WIDTH, height);

    // Border
    XSetForeground(display, gc, COLOR_SWITCHER_BORDER);
    XDrawRectangle(display, switcher, gc, 0, 0, SWITCHER_WIDTH - 1, height - 1);

    for (size_t i = 0; i < switcher_list.size(); ++i) {
        int y = SWITCHER_PAD + (int)i * SWITCHER_LINE_H;

        if (i == switcher_index) {
            XSetForeground(display, gc, COLOR_SWITCHER_HL);
            XFillRectangle(display, switcher, gc,
                           4, y,
                           SWITCHER_WIDTH - 8, SWITCHER_LINE_H);
        }

        std::string title = get_window_title(switcher_list[i]->window);
        if (title.size() > 48)
            title = title.substr(0, 45) + "...";

        XSetForeground(display, gc, COLOR_SWITCHER_TEXT);
        XDrawString(display, switcher, gc,
                    SWITCHER_PAD + 6, y + 20,
                    title.c_str(), (int)title.size());
    }

    XFreeGC(display, gc);
}

static void show_switcher()
{
    if (switcher_list.empty())
        return;

    int height = SWITCHER_PAD * 2 + (int)switcher_list.size() * SWITCHER_LINE_H;
    int screen_w = DisplayWidth(display, screen);
    int screen_h = DisplayHeight(display, screen);
    int x = (screen_w - SWITCHER_WIDTH) / 2;
    int y = (screen_h - height) / 2;

    if (switcher == None) {
        XSetWindowAttributes attrs{};
        attrs.override_redirect = True;
        attrs.background_pixel  = COLOR_SWITCHER_BG;
        attrs.border_pixel      = COLOR_SWITCHER_BORDER;
        attrs.event_mask        = ExposureMask;

        switcher = XCreateWindow(
            display, root,
            x, y, SWITCHER_WIDTH, height,
            1,  // border width
            CopyFromParent, InputOutput, CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
            &attrs
        );
    } else {
        XMoveResizeWindow(display, switcher, x, y, SWITCHER_WIDTH, height);
    }

    XMapRaised(display, switcher);

    // NEW: grab the keyboard so we reliably see the Alt release
    // regardless of which window has input focus.
    XGrabKeyboard(display, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);

    switcher_active = true;
    draw_switcher();
}

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

static void cycle_switcher(bool reverse)
{
  // Include ALL clients (minimized ones too)
  switcher_list.clear();
  for (Client* c : clients) {
    switcher_list.push_back(c);
  }

  if (switcher_list.empty()) {
    hide_switcher();
    return;
  }

  if (!switcher_active) {
    // First Alt+Tab: start after the currently focused window
    Client* current = get_focused_client();
    size_t start = 0;
    if (current) {
      auto it = std::find(switcher_list.begin(), switcher_list.end(), current);
      if (it != switcher_list.end()) {
        start = (it - switcher_list.begin() + 1) % switcher_list.size();
      }
    }
    switcher_index = start;
    show_switcher();
  }
  else {
    if (reverse) {
      switcher_index = (switcher_index == 0)
                           ? switcher_list.size() - 1
                           : switcher_index - 1;
    }
    else {
      switcher_index = (switcher_index + 1) % switcher_list.size();
    }
    draw_switcher();
  }

  //  // Build / refresh the list of visible windows
  //  switcher_list.clear();
  //  for (Client* c : clients) {
  //      if (!c->minimized)
  //          switcher_list.push_back(c);
  //  }

  //  if (switcher_list.empty()) {
  //      hide_switcher();
  //      return;
  //  }

  //  if (!switcher_active) {
  //      // First press: start switcher and select the next window
  //      Client* current = get_focused_client();
  //      size_t start = 0;
  //      if (current) {
  //          auto it = std::find(switcher_list.begin(), switcher_list.end(), current);
  //          if (it != switcher_list.end())
  //              start = (it - switcher_list.begin() + 1) % switcher_list.size();
  //      }
  //      switcher_index = start;
  //      show_switcher();
  //  } else {
  //      // Subsequent presses: cycle
  //      if (reverse) {
  //          if (switcher_index == 0)
  //              switcher_index = switcher_list.size() - 1;
  //          else
  //              --switcher_index;
  //      } else {
  //          switcher_index = (switcher_index + 1) % switcher_list.size();
  //      }
  //      draw_switcher();
  //  }
}

static void focus_next();

static volatile sig_atomic_t need_reconfigure = 0;

static void sighup_handler(int)
{
    need_reconfigure = 1;
}

static std::string expand_key_string(const std::string& key_string)
{
    std::stringstream ss(key_string);
    std::string part;
    std::vector<std::string> parts;
    while (std::getline(ss, part, '-')) {
        parts.push_back(part);
    }
    if (parts.empty())
        return key_string;

    std::string result;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        const std::string& m = parts[i];
        std::string full;
        if (m == "W")      full = "Win";
        else if (m == "A") full = "Alt";
        else if (m == "C") full = "Control";
        else if (m == "S") full = "Shift";
        else               full = m; // unknown modifier, keep as-is
        result += full + "-";
    }
    result += parts.back();
    return result;
}








//static void keybindings_window_apply_geometry()
//{
//    XMoveResizeWindow(display, keybindings_window, kb_win_x, kb_win_y, kb_win_width, kb_win_height);
//    draw_keybindings_window();
//}



static void hide_keybindings_window()
{
    if (keybindings_window != None && keybindings_window_active) {
        XUnmapWindow(display, keybindings_window);
    }
    keybindings_window_active = false;
}

//static void maximize_keybindings_window()
//{
//    int screen_w = DisplayWidth(display, screen);
//    int screen_h = DisplayHeight(display, screen);
//
//    if (!keybindings_window_maximized) {
//        kb_win_old_x = kb_win_x;
//        kb_win_old_y = kb_win_y;
//        kb_win_old_width = kb_win_width;
//        kb_win_old_height = kb_win_height;
//
//        kb_win_x = 0;
//        kb_win_y = 0;
//        kb_win_width = screen_w;
//        kb_win_height = screen_h;
//        keybindings_window_maximized = true;
//    } else {
//        kb_win_x = kb_win_old_x;
//        kb_win_y = kb_win_old_y;
//        kb_win_width = kb_win_old_width;
//        kb_win_height = kb_win_old_height;
//        keybindings_window_maximized = false;
//    }
//
//    keybindings_window_apply_geometry();
//}

static void handle_keybindings_window_click(XButtonEvent* event)
{
    if (event->y < BORDER_WIDTH || event->y >= TITLE_HEIGHT) {
      return; // clicks below the titlebar do nothing for now
    }

    int close_x = kb_close_button_x();

    if (event->x >= close_x) {
        hide_keybindings_window();
        return;
    }

    // Anywhere else on the titlebar -> drag to move
    move_keybindings_window();
}

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------


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


static std::string get_pidfile()
{
    return get_config_directory() + "/mew.pid";
}

static void write_pidfile()
{
    std::ofstream f(get_pidfile());
    if (f)
        f << getpid() << '\n';
}

static void remove_pidfile()
{
    unlink(get_pidfile().c_str());
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
    binding.display = expand_key_string(key_string);

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

    //int baseline = BORDER_WIDTH + (TITLE_HEIGHT - BORDER_WIDTH + title_font ? title_font->ascent : 10) / 2;

    std::string title = get_window_title(client->window);

    int text_height = title_font ? (title_font->ascent + title_font->descent) : 10;
    int baseline = BORDER_WIDTH 
      + (TITLE_HEIGHT - BORDER_WIDTH - text_height) / 2 
      + (title_font ? title_font->ascent : 10);
    draw_title_text(client->frame, BORDER_WIDTH + 8, baseline, title);

    //XDrawString(
    //    display,
    //    client->frame,
    //    gc,
    //    BORDER_WIDTH + 8,
    //    TITLE_HEIGHT - 9,
    //    "mew",
    //    3
    //);

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
  if (!client) {
    return;
  }

  if (client->minimized) {
    client->minimized = false;
    XMapWindow(display, client->frame);
  }

  XRaiseWindow(display, client->frame);
  raise_keybindings_window_if_active();
  XSetInputFocus(display, client->window, RevertToPointerRoot, CurrentTime);
  draw_frame(client);

  //if (!client || client->minimized)
  //    return;

  //XRaiseWindow(
  //    display,
  //    client->frame
  //);

  //XSetInputFocus(
  //    display,
  //    client->window,
  //    RevertToPointerRoot,
  //    CurrentTime
  //);

  //draw_frame(client);
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
  if (client->maximized) {
    return;
  }

  Window dummy;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;

  XQueryPointer(
    display, root, &dummy, &dummy,
    &root_x, &root_y, &win_x, &win_y, &mask
  );

  int start_x = client->x;
  int start_y = client->y;

  XGrabPointer(
    display, client->frame, False,
    ButtonMotionMask | ButtonReleaseMask,
    GrabModeAsync, GrabModeAsync,
    None, None, CurrentTime
  );

  XEvent event;
  while (true) {
    XMaskEvent(display, ButtonMotionMask | ButtonReleaseMask, &event);

    if (event.type == MotionNotify) {
      int dx = event.xmotion.x_root - root_x;
      int dy = event.xmotion.y_root - root_y;

      client->x = start_x + dx;
      client->y = start_y + dy;

      resize_client(client);
    }

    if (event.type == ButtonRelease)
      break;
  }

  XUngrabPointer(display, CurrentTime);
}

//static void move_client(Client* client)
//{
//  if (client->maximized) {
//    return;
//  }
//
//  Window dummy;
//
//  int root_x;
//  int root_y;
//
//  int win_x;
//  int win_y;
//
//  unsigned int mask;
//
//  XQueryPointer(
//      display,
//      root,
//      &dummy,
//      &dummy,
//      &root_x,
//      &root_y,
//      &win_x,
//      &win_y,
//      &mask
//  );
//
//  int start_x = kb_win_x;// client->x;
//  int start_y = kb_win_y;// client->y;
//                         //
//  int grab_result = XGrabPointer(
//    display, client->frame, False,
//    ButtonMotionMask | ButtonReleaseMask,
//    GrabModeAsync, GrabModeAsync,
//    None, None, CurrentTime
//  );
//  fprintf(stderr, "mew: grab result = %d (0 = success)\n", grab_result);
//
//
//  //XGrabPointer(
//  //  display, keybindings_window, False,
//  //  ButtonMotionMask | ButtonReleaseMask,
//  //  GrabModeAsync, GrabModeAsync,
//  //  None, None, CurrentTime
//  //);
//
//  XEvent event;
//  while (true) {
//      XMaskEvent(display, ButtonMotionMask | ButtonReleaseMask, &event);
//
//      if (event.type == MotionNotify) {
//          int dx = event.xmotion.x_root - root_x;
//          int dy = event.xmotion.y_root - root_y;
//
//          kb_win_x = start_x + dx;
//          kb_win_y = start_y + dy;
//
//          keybindings_window_apply_geometry();
//      }
//
//      if (event.type == ButtonRelease)
//          break;
//  }
//
//  XUngrabPointer(display, CurrentTime);
//
//  //XGrabPointer(
//  //    display,
//  //    client->frame,
//  //    False,
//  //    ButtonMotionMask |
//  //    ButtonReleaseMask,
//  //    GrabModeAsync,
//  //    GrabModeAsync,
//  //    None,
//  //    None,
//  //    CurrentTime
//  //);
//
//  //XEvent event;
//
//  //while (true) {
//
//  //    XMaskEvent(
//  //        display,
//  //        ButtonMotionMask |
//  //        ButtonReleaseMask,
//  //        &event
//  //    );
//
//  //    if (event.type == MotionNotify) {
//
//  //        int dx =
//  //            event.xmotion.x_root - root_x;
//
//  //        int dy =
//  //            event.xmotion.y_root - root_y;
//
//  //        client->x = start_x + dx;
//  //        client->y = start_y + dy;
//
//  //        resize_client(client);
//  //    }
//
//  //    if (event.type == ButtonRelease)
//  //        break;
//  //}
//
//  //XUngrabPointer(
//  //    display,
//  //    CurrentTime
//  //);
//}


// ------------------------------------------------------------
// Button handling
// ------------------------------------------------------------

static void handle_button_press(XButtonEvent* event)
{
    Client* client = find_client(event->window);

    if (!client) {
      return;
    }

    focus_client(client);
    ResizeDirection direction = get_resize_direction(client, event->x, event->y);

    if (event->button == Button1 && direction != RESIZE_NONE) {
      resize_window(client, direction);
      return;
    }

    // Titlebar
    if (event->y >= RESIZE_BORDER && event->y < TITLE_HEIGHT) {
      int frame_width = client->width + BORDER_WIDTH * 2;
      int close_x = frame_width - BUTTON_WIDTH - BORDER_WIDTH;
      int max_x = close_x - BUTTON_WIDTH;
      int min_x = max_x - BUTTON_WIDTH;

      // Buttons
      if (event->button == Button1) {
        if (event->x >= min_x && event->x < max_x) {
          minimize_client(client);
          return;
        }

        if (event->x >= max_x && event->x < close_x) {
          maximize_client(client);
          return;
        }

        if (event->x >= close_x) {
          close_client(client);
          return;
        }
      }

      // Double-click titlebar to maximize/restore.
      if (event->button == Button1 && event->x < min_x) {
        Time now = event->time;
        if (client->last_title_click != 0 && now - client->last_title_click < 400) {
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
  if (find_client(window)) {
    return;
  }

  XWindowAttributes attr;
  if (!XGetWindowAttributes(display, window, &attr)) {
    return;
  }

  if (attr.override_redirect) {
    return;
  }

  // Center the window on the screen
  int screen_w = DisplayWidth(display, screen);
  int screen_h = DisplayHeight(display, screen);

  int frame_w = attr.width  + BORDER_WIDTH * 2;
  int frame_h = attr.height + TITLE_HEIGHT + BORDER_WIDTH;

  int x = (screen_w - frame_w) / 2;
  int y = (screen_h - frame_h) / 2;

  // Keep it on-screen if the window is larger than the display
  if (x < 0) x = 0;
  if (y < 0) y = 0;

  Client* client = new Client{};
  client->window = window;
  client->frame = XCreateSimpleWindow(
      display,
      root,
      x,
      y,
      frame_w,
      frame_h,
      0,
      COLOR_BORDER,
      COLOR_TITLE
  );

  client->x = x;
  client->y = y;

  client->width = attr.width;
  client->height = attr.height;

  client->old_x = x;
  client->old_y = y;

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


  XAddToSaveSet(display, window);

  //XSelectInput(display, window, StructureNotifyMask);
  XSelectInput(display, window, StructureNotifyMask | PropertyChangeMask);
  XReparentWindow(
    display,
    window,
    client->frame,
    BORDER_WIDTH,
    TITLE_HEIGHT
  );

  XMapWindow(display, client->frame);
  XMapWindow(display, window);

  raise_keybindings_window_if_active();

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
  // Built-in shortcuts
  grab_key(XKeysymToKeycode(display, XK_Tab), Mod1Mask);                  // Alt+Tab
  grab_key(XKeysymToKeycode(display, XK_Tab), Mod1Mask | ShiftMask);      // Alt+Shift+Tab
  grab_key(XKeysymToKeycode(display, XK_F4), Mod1Mask);
  grab_key(XKeysymToKeycode(display, XK_F1), Mod1Mask);
  grab_key(XKeysymToKeycode(display, XK_q), Mod1Mask | ShiftMask);

  // User-configured shortcuts
  for (const KeyBinding& binding : keybindings) {
    grab_key(binding.keycode, binding.modifiers);
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

int main(int argc, char** argv)
{
  bool do_reconfigure = false;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--reconfigure") == 0) {
      do_reconfigure = true;
    }
    else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      printf("Usage: mew [--reconfigure]\n");
      return 0;
    }
  }

  if (do_reconfigure) {
    std::ifstream f(get_pidfile());
    pid_t pid = 0;
    if (f >> pid && pid > 1) {
      if (kill(pid, SIGHUP) == 0) {
        printf("mew: reconfigure sent to pid %d\n", (int)pid);
        return 0;
      }
      fprintf(stderr, "mew: failed to signal pid %d\n", (int)pid);
      return 1;
    }
    fprintf(stderr, "mew: no running instance found\n");
    return 1;
  }

  // ---------- normal startup ----------
  display = XOpenDisplay(nullptr);
  if (!display) {
    fprintf(stderr, "mew: cannot open display\n");
    return 1;
  }

  // install SIGHUP handler
  struct sigaction sa{};
  sa.sa_handler = sighup_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGHUP, &sa, nullptr);

  screen = DefaultScreen(display);
  root   = RootWindow(display, screen);


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

  load_title_font();

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
      KeyReleaseMask |
      PropertyChangeMask
  );


  /*
      Grab keyboard shortcuts
  */

  grab_keys();


  write_pidfile();

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
    if (need_reconfigure) {
      need_reconfigure = 0;
      printf("mew: reconfiguring...\n");

      // drop every previous grab
      XUngrabKey(display, AnyKey, AnyModifier, root);

      // reload keybindings from disk
      load_keybindings();

      // re-grab everything (built-in + new config)
      grab_keys();
    }

    // Safety net: if Alt is no longer held, force-close the switcher
    if (switcher_active && !is_alt_held()) {
      if (!switcher_list.empty() && switcher_index < switcher_list.size()) {
        focus_client(switcher_list[switcher_index]);
      }
      hide_switcher();
    }

    XEvent event;
    XNextEvent(display, &event);

    switch (event.type) {
      case PropertyNotify:
      {
        //Atom name_atom = XInternAtom(display, "_NET_WM_NAME", False);
      
        NET_WM_NAME = XInternAtom(display, "_NET_WM_NAME", False);

        if (event.xproperty.atom == XA_WM_NAME || event.xproperty.atom == NET_WM_NAME) {
      
          Client* client = find_client(event.xproperty.window);
          if (client) {
            draw_frame(client);
          }
        }
        break;
      }

        case MapRequest:
        {
            manage(event.xmaprequest.window);
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
          Window w = event.xbutton.window;

          // Click on the keybindings window -> handle its titlebar buttons
          if (keybindings_window_active && w == keybindings_window) {
              handle_keybindings_window_click(&event.xbutton);
              break;
          }

          // Click landed on an open context menu -> select the item.
          if (context_menu_active && w == context_menu) {
              int index = event.xbutton.y / MENU_ITEM_HEIGHT;
              hide_context_menu();
              handle_context_menu_selection(index);
              break;
          }

          // Any other click dismisses an open menu.
          if (context_menu_active) {
              hide_context_menu();
          }

          // Right-click on empty desktop space -> open the menu.
          if (w == root && event.xbutton.button == Button3) {
              show_context_menu(event.xbutton.x_root, event.xbutton.y_root);
              break;
          }

          handle_button_press(&event.xbutton);
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
          if (event.xexpose.window == switcher) {
            draw_switcher();
            break;
          }
          if (event.xexpose.window == context_menu) {
            draw_context_menu();
            break;
          }
          if (event.xexpose.window == keybindings_window) {
            draw_keybindings_window();
            break;
          }

          Client* client = find_client(event.xexpose.window);
          if (client) {
            draw_frame(client);
          }
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
          XKeyEvent* key = &event.xkey;

          // First check user configuration.
          if (handle_custom_keybinding(key)) {
            break;
          }
          unsigned int state = key->state & ~(LockMask | Mod2Mask);
          KeySym keysym = XLookupKeysym(key, 0);

          // Alt+Tab / Alt+Shift+Tab
          if (keysym == XK_Tab && (state == Mod1Mask || state == (Mod1Mask | ShiftMask))) {
              bool reverse = (state & ShiftMask);
              cycle_switcher(reverse);
              break;
          }

          //// Alt+Tab
          //if (state == Mod1Mask && keysym == XK_Tab) {
          //  focus_next();
          //  break;
          //}

          // Alt+F4
          if (state == Mod1Mask && keysym == XK_F4) {
            Client* client = get_focused_client();
            if (client) {
              close_client(client);
            }
            break;
          }

          // Alt+F1
          if (state == Mod1Mask && keysym == XK_F1) {
            std::system("wezterm start >/dev/null 2>&1 &");
            break;
          }

          // Alt+Shift+Q (Quit mew)
          //if (state == (Mod1Mask | ShiftMask) && keysym == XK_q) {
          //  XCloseDisplay(display);
          //  return 0;
          //}
        break;
      }
      case KeyRelease:
      {
        if (!switcher_active) {
          break;
        }

        KeySym keysym = XLookupKeysym(&event.xkey, 0);
        // Close the switcher as soon as Alt is no longer held
        // (works no matter the order you release Tab / Alt)
        if (keysym == XK_Alt_L || keysym == XK_Alt_R ||
          keysym == XK_Tab    || !is_alt_held()) {

          if (!is_alt_held()) {
            if (!switcher_list.empty() && switcher_index < switcher_list.size()) {
              focus_client(switcher_list[switcher_index]);
            }
            hide_switcher();
          }
        }
        break;

        //KeySym keysym = XLookupKeysym(&event.xkey, 0);

        //// Release of Alt commits the switch
        //if ((keysym == XK_Alt_L || keysym == XK_Alt_R) && switcher_active) {
        //  if (!switcher_list.empty() && switcher_index < switcher_list.size()) {
        //    focus_client(switcher_list[switcher_index]);
        //  }
        //  hide_switcher();
        //}
        //break;
      }
    }
    if (should_quit) {
      break;
    }
  }

  remove_pidfile();
  XCloseDisplay(display);
  return 0;
}
