#include <X11/Xft/Xft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include "hurmit_font_data.h"


#include <X11/Xcursor/Xcursor.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#include <csignal>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <alsa/asoundlib.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <dirent.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Client {
  Window window;
  Window frame;

  int x, y;
  int width, height;

  int old_x, old_y;
  int old_width, old_height;

  bool maximized;
  bool minimized;
  bool fullscreen;

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

static FT_Library ft_library = nullptr;
static FT_Face ft_face = nullptr;

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

struct Config {
  double titleFontSize = 17.0;
  std::string mouseTheme = "";
  int mouseSize = 24;
  unsigned long backgroundColor = 0x3B3C3C;
  unsigned long panelColor = 0x222222;
  std::string backgroundImage;
  bool useBackgroundImage = false; // last of color/image in config wins
  std::string loginSound;
  std::string logoutSound;
  std::string windowTheme; // reserved
};

static Config config;
static Pixmap backgroundPixmap = None;

static const int TITLE_HEIGHT = 28;
static const int BORDER_WIDTH = 6;   // must be >= RESIZE_BORDER so edges stay on the frame
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
static Atom NET_WM_STATE;
static Atom NET_WM_STATE_FULLSCREEN;
static Atom NET_WM_STATE_MAXIMIZED_VERT;
static Atom NET_WM_STATE_MAXIMIZED_HORZ;

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

// Panel
static Window panel = None;
static const int PANEL_HEIGHT = 28;
static unsigned long COLOR_PANEL_BG = 0x222222;
static const unsigned long COLOR_PANEL_TEXT = 0xffffff;
static time_t panel_last_time = 0;
static bool desktop_showing = false;

// Start menu
static Window start_menu = None;
static bool start_menu_active = false;
static const int START_MENU_WIDTH = 180;
static const int START_MENU_ITEM_H = 32;
static const std::vector<std::string> start_menu_items = {
  "󰀻  Apps",
  "  KeyBindings",
  "󰐥  PowerManager  ▸"
};

// PowerManager submenu (opens to the right of start menu)
static Window power_menu = None;
static bool power_menu_active = false;
static const int POWER_MENU_WIDTH = 180;
static const std::vector<std::string> power_menu_items = {
  "󰢻  Reconfigure mew",
  "󰜉  Reboot",
  "󰤆  Poweroff",
  "󰗼  Logout"
};

struct DesktopApp {
  std::string name;
  std::string exec;
};

static std::vector<DesktopApp> desktopApps;
static std::vector<int> launcherFiltered; // indices into desktopApps
static Window launcher = None;
static bool launcher_active = false;
static std::string launcherQuery;
static size_t launcherIndex = 0;
static const int LAUNCHER_WIDTH = 480;
static const int LAUNCHER_LINE_H = 28;
static const int LAUNCHER_PAD = 10;
static const int LAUNCHER_MAX_VISIBLE = 12;

static void focus_next();
static void draw_title_text(Window window, int x, int y, const std::string& text);
static void draw_panel();
static void create_panel();
static void handle_panel_click(int x);
static void draw_frame(Client* client);
static void show_launcher();
static void hide_launcher();
static void draw_launcher();
static void filter_launcher();
static void scan_desktop_apps();


// Usable area excludes the bottom panel
static int usable_height()
{
  return DisplayHeight(display, screen) - PANEL_HEIGHT;
}

// NEW forward declarations
//static int kb_close_button_x();
//static void move_keybindings_window();
//static void keybindings_window_apply_geometry();

static Cursor cursor_default;
static Cursor cursor_resize_h;
static Cursor cursor_resize_v;
static Cursor cursor_resize_tl;
static Cursor cursor_resize_tr;
static Cursor cursor_resize_bl;
static Cursor cursor_resize_br;

static Cursor load_cursor(const char* theme_name, unsigned int fallback_shape)
{
  Cursor c = XcursorLibraryLoadCursor(display, theme_name);
  if (c == None) {
    // Theme doesn't have this shape (or no theme installed) — fall
    // back to the old core-font cursor rather than leaving it unset.
    c = XCreateFontCursor(display, fallback_shape);
  }
  return c;
}

static void load_cursors()
{
  cursor_default   = load_cursor("left_ptr",           XC_left_ptr);
  cursor_resize_h  = load_cursor("sb_h_double_arrow",  XC_sb_h_double_arrow);
  cursor_resize_v  = load_cursor("sb_v_double_arrow",  XC_sb_v_double_arrow);
  cursor_resize_tl = load_cursor("top_left_corner",    XC_top_left_corner);
  cursor_resize_tr = load_cursor("top_right_corner",   XC_top_right_corner);
  cursor_resize_bl = load_cursor("bottom_left_corner", XC_bottom_left_corner);
  cursor_resize_br = load_cursor("bottom_right_corner",XC_bottom_right_corner);
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
        hurmit_ttf,
        (FT_Long)hurmit_ttf_len,
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
  FcPatternAddDouble(pattern, FC_PIXEL_SIZE, config.titleFontSize);

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
  if (panel != None) {
    XRaiseWindow(display, panel);
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
    int text_height = title_font ? (title_font->ascent + title_font->descent) : 10;
    int title_baseline = BORDER_WIDTH
      + (TITLE_HEIGHT - BORDER_WIDTH - text_height) / 2
      + (title_font ? title_font->ascent : 10);
    draw_title_text(keybindings_window, BORDER_WIDTH + 8, title_baseline, "Keybindings");

    // Content area background
    XSetForeground(display, gc, COLOR_SWITCHER_BG);
    XFillRectangle(display, keybindings_window, gc,
                   BORDER_WIDTH, TITLE_HEIGHT,
                   kb_win_width - BORDER_WIDTH * 2,
                   kb_win_height - TITLE_HEIGHT - BORDER_WIDTH);

    // Content text
    int y = TITLE_HEIGHT + KB_PADDING + (title_font ? title_font->ascent : 12);
    for (const std::string& line : kb_display_lines) {
        if (!line.empty()) {
            draw_title_text(keybindings_window, BORDER_WIDTH + KB_PADDING, y, line);
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

        int baseline = y + (SWITCHER_LINE_H + (title_font ? title_font->ascent : 10)) / 2 - 2;
        draw_title_text(switcher, SWITCHER_PAD + 6, baseline, title);
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
    return "/tmp/mew.pid";
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

static void load_config()
{
  // Reset to defaults before reading (last entry in file wins for bg)
  config.backgroundColor = 0x3B3C3C;
  config.backgroundImage.clear();
  config.useBackgroundImage = false;

  std::string path = get_config_directory() + "/config";
  std::ifstream file(path);
  if (!file.is_open()) {
    fprintf(stderr, "mew: no config file: %s (using defaults)\n", path.c_str());
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#')
      continue;

    size_t eq = line.find('=');
    if (eq == std::string::npos)
      continue;

    std::string key = trim(line.substr(0, eq));
    std::string val = trim(line.substr(eq + 1));

    // strip optional quotes
    if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
      val = val.substr(1, val.size() - 2);

    if (key == "title_font_size") {
      config.titleFontSize = std::atof(val.c_str());
      if (config.titleFontSize < 8.0)
        config.titleFontSize = 8.0;
    }
    else if (key == "mouse_theme") {
      config.mouseTheme = val;
    }
    else if (key == "mouse_size") {
      config.mouseSize = std::atoi(val.c_str());
      if (config.mouseSize < 8)
        config.mouseSize = 8;
    }
    else if (key == "background_color") {
      if (!val.empty() && val[0] == '#')
        val = "0x" + val.substr(1);
      config.backgroundColor = std::strtoul(val.c_str(), nullptr, 0);
      config.useBackgroundImage = false; // last wins
    }
    else if (key == "panel_color") {
      if (!val.empty() && val[0] == '#')
        val = "0x" + val.substr(1);
      config.panelColor = std::strtoul(val.c_str(), nullptr, 0);
      COLOR_PANEL_BG = config.panelColor;
    }
    else if (key == "background_image") {
      config.backgroundImage = expand_home(val);
      config.useBackgroundImage = true; // last wins
    }
    else if (key == "login_sound") {
      config.loginSound = expand_home(val);
    }
    else if (key == "logout_sound") {
      config.logoutSound = expand_home(val);
    }
    else if (key == "window_theme") {
      config.windowTheme = val;
    }
  }

  //printf("mew: config loaded (font=%.1f, mouse=%s/%d, bg=0x%06lx)\n",
  //       config.titleFontSize,
  //       config.mouseTheme.c_str(),
  //       config.mouseSize,
  //       config.backgroundColor);
}

// Minimal WAV player (PCM, 16-bit). Runs in a forked child so the WM stays responsive.
static void play_sound(const std::string& path)
{
  if (path.empty())
    return;

  pid_t pid = fork();
  if (pid != 0)
    return; // parent continues

  // --- child ---
  FILE* f = fopen(path.c_str(), "rb");
  if (!f)
    _exit(1);

  // Parse RIFF/WAV header (minimal)
  char riff[12];
  if (fread(riff, 1, 12, f) != 12 || memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
    fclose(f);
    _exit(1);
  }

  uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
  uint32_t sampleRate = 0, dataSize = 0;
  long dataOffset = 0;

  while (true) {
    char chunkId[4];
    uint32_t chunkSize = 0;
    if (fread(chunkId, 1, 4, f) != 4 || fread(&chunkSize, 4, 1, f) != 1)
      break;

    if (memcmp(chunkId, "fmt ", 4) == 0) {
      uint16_t fmt, ch, bps;
      uint32_t sr;
      fread(&fmt, 2, 1, f);
      fread(&ch, 2, 1, f);
      fread(&sr, 4, 1, f);
      fseek(f, 6, SEEK_CUR); // skip byte rate + block align
      fread(&bps, 2, 1, f);
      audioFormat = fmt;
      numChannels = ch;
      sampleRate = sr;
      bitsPerSample = bps;
      if (chunkSize > 16)
        fseek(f, chunkSize - 16, SEEK_CUR);
    }
    else if (memcmp(chunkId, "data", 4) == 0) {
      dataSize = chunkSize;
      dataOffset = ftell(f);
      break;
    }
    else {
      fseek(f, chunkSize, SEEK_CUR);
    }
  }

  if (audioFormat != 1 || bitsPerSample != 16 || dataOffset == 0) {
    fclose(f);
    _exit(1);
  }

  fseek(f, dataOffset, SEEK_SET);

  snd_pcm_t* pcm = nullptr;
  if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    fclose(f);
    _exit(1);
  }

  snd_pcm_set_params(pcm,
                     SND_PCM_FORMAT_S16_LE,
                     SND_PCM_ACCESS_RW_INTERLEAVED,
                     numChannels,
                     sampleRate,
                     1,          // soft resample
                     100000);    // 100 ms latency

  const size_t bufFrames = 1024;
  std::vector<char> buf(bufFrames * numChannels * 2);
  size_t remaining = dataSize;

  while (remaining > 0) {
    size_t toRead = std::min(remaining, buf.size());
    size_t n = fread(buf.data(), 1, toRead, f);
    if (n == 0)
      break;
    snd_pcm_sframes_t frames = n / (numChannels * 2);
    snd_pcm_sframes_t written = snd_pcm_writei(pcm, buf.data(), frames);
    if (written < 0)
      snd_pcm_recover(pcm, (int)written, 0);
    remaining -= n;
  }

  snd_pcm_drain(pcm);
  snd_pcm_close(pcm);
  fclose(f);
  _exit(0);
}

static void apply_background()
{
  // Free previous pixmap if any
  if (backgroundPixmap != None) {
    XFreePixmap(display, backgroundPixmap);
    backgroundPixmap = None;
  }

  int screen_w = DisplayWidth(display, screen);
  int screen_h = DisplayHeight(display, screen);

  if (config.useBackgroundImage && !config.backgroundImage.empty()) {
    int img_w = 0, img_h = 0, channels = 0;
    unsigned char* data = stbi_load(
      config.backgroundImage.c_str(),
      &img_w, &img_h, &channels, 4 // force RGBA
    );

    if (data && img_w > 0 && img_h > 0) {
      backgroundPixmap = XCreatePixmap(
        display, root, screen_w, screen_h,
        DefaultDepth(display, screen)
      );

      // Build XImage from stretched pixels (nearest-neighbor)
      Visual* visual = DefaultVisual(display, screen);
      int depth = DefaultDepth(display, screen);

      // Create 32-bit ZPixmap buffer
      size_t bufSize = (size_t)screen_w * screen_h * 4;
      char* xdata = (char*)malloc(bufSize);
      if (xdata) {
        for (int y = 0; y < screen_h; ++y) {
          int sy = y * img_h / screen_h;
          for (int x = 0; x < screen_w; ++x) {
            int sx = x * img_w / screen_w;
            unsigned char* src = data + (sy * img_w + sx) * 4;
            // X11 typically wants BGRX on little-endian
            char* dst = xdata + (y * screen_w + x) * 4;
            dst[0] = (char)src[2]; // B
            dst[1] = (char)src[1]; // G
            dst[2] = (char)src[0]; // R
            dst[3] = 0;
          }
        }

        XImage* image = XCreateImage(
          display, visual, depth, ZPixmap, 0,
          xdata, screen_w, screen_h, 32, 0
        );

        if (image) {
          GC gc = XCreateGC(display, backgroundPixmap, 0, nullptr);
          XPutImage(display, backgroundPixmap, gc, image,
                    0, 0, 0, 0, screen_w, screen_h);
          XFreeGC(display, gc);
          // XDestroyImage frees xdata
          XDestroyImage(image);
        } else {
          free(xdata);
        }
      }

      stbi_image_free(data);

      XSetWindowBackgroundPixmap(display, root, backgroundPixmap);
      XClearWindow(display, root);
      printf("mew: background image set: %s\n", config.backgroundImage.c_str());
      return;
    }

    if (data)
      stbi_image_free(data);
    fprintf(stderr, "mew: failed to load background image: %s\n",
            config.backgroundImage.c_str());
  }

  // Solid color fallback / explicit color
  XSetWindowBackground(display, root, config.backgroundColor);
  XClearWindow(display, root);
}

static void hide_power_menu()
{
  if (power_menu != None && power_menu_active) {
    XUnmapWindow(display, power_menu);
  }
  power_menu_active = false;
}

static void hide_start_menu()
{
  hide_power_menu();
  if (start_menu != None && start_menu_active) {
    XUnmapWindow(display, start_menu);
  }
  start_menu_active = false;
}

static void draw_menu_window(Window win, const std::vector<std::string>& items, int width)
{
  if (win == None)
    return;

  int height = (int)items.size() * START_MENU_ITEM_H;
  GC gc = XCreateGC(display, win, 0, nullptr);

  XSetForeground(display, gc, COLOR_PANEL_BG);
  XFillRectangle(display, win, gc, 0, 0, width, height);

  XSetForeground(display, gc, 0x555555);
  XDrawRectangle(display, win, gc, 0, 0, width - 1, height - 1);

  for (size_t i = 0; i < items.size(); ++i) {
    int y = (int)i * START_MENU_ITEM_H;
    int baseline = y + (START_MENU_ITEM_H + (title_font ? title_font->ascent : 10)) / 2 - 2;
    draw_title_text(win, 12, baseline, items[i]);
  }

  XFreeGC(display, gc);
}

static void draw_start_menu()
{
  if (start_menu == None || !start_menu_active)
    return;
  draw_menu_window(start_menu, start_menu_items, START_MENU_WIDTH);
}

static void draw_power_menu()
{
  if (power_menu == None || !power_menu_active)
    return;
  draw_menu_window(power_menu, power_menu_items, POWER_MENU_WIDTH);
}

static void show_power_menu()
{
  int height = (int)power_menu_items.size() * START_MENU_ITEM_H;
  int screen_h = DisplayHeight(display, screen);
  // Align to the right of the start menu, same bottom edge
  int x = START_MENU_WIDTH;
  int y = screen_h - PANEL_HEIGHT - height;

  if (power_menu == None) {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = COLOR_PANEL_BG;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    power_menu = XCreateWindow(
      display, root,
      x, y, POWER_MENU_WIDTH, height,
      1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs
    );
  } else {
    XMoveResizeWindow(display, power_menu, x, y, POWER_MENU_WIDTH, height);
  }

  XMapRaised(display, power_menu);
  power_menu_active = true;
  draw_power_menu();
}

static void show_start_menu()
{
  hide_power_menu();
  int height = (int)start_menu_items.size() * START_MENU_ITEM_H;
  int screen_h = DisplayHeight(display, screen);
  int x = 0;
  int y = screen_h - PANEL_HEIGHT - height;

  if (start_menu == None) {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = COLOR_PANEL_BG;
    attrs.event_mask = ExposureMask | ButtonPressMask;

    start_menu = XCreateWindow(
      display, root,
      x, y, START_MENU_WIDTH, height,
      1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs
    );
  } else {
    XMoveResizeWindow(display, start_menu, x, y, START_MENU_WIDTH, height);
  }

  XMapRaised(display, start_menu);
  start_menu_active = true;
  draw_start_menu();
}

// Portable reboot / poweroff (no systemctl).
// Tries the reboot(2) syscall first; falls back to /sbin/{reboot,poweroff}.
static void do_reboot()
{
  sync();
  if (reboot(RB_AUTOBOOT) != 0) {
    execl("/sbin/reboot", "reboot", (char*)nullptr);
    execl("/bin/reboot", "reboot", (char*)nullptr);
  }
}

static void do_poweroff()
{
  sync();
  if (reboot(RB_POWER_OFF) != 0) {
    execl("/sbin/poweroff", "poweroff", (char*)nullptr);
    execl("/bin/poweroff", "poweroff", (char*)nullptr);
  }
}

static void handle_power_menu_click(int y)
{
  int index = y / START_MENU_ITEM_H;
  hide_start_menu(); // closes both menus

  if (index == 0) {
    // Reconfigure mew → SIGHUP self
    need_reconfigure = 1;
  }
  else if (index == 1) {
    do_reboot();
  }
  else if (index == 2) {
    do_poweroff();
  }
  else if (index == 3) {
    should_quit = true;
  }
}

static void handle_start_menu_click(int y)
{
  int index = y / START_MENU_ITEM_H;
  if (index == 0) {
    hide_start_menu();
    show_launcher();
  }
  else if (index == 1) {
    hide_start_menu();
    show_keybindings_window();
  }
  else if (index == 2) {
    // Open PowerManager submenu to the right (keep start menu open)
    if (power_menu_active)
      hide_power_menu();
    else
      show_power_menu();
  }
}

static std::string desktop_field(const std::string& content, const std::string& key)
{
  std::string prefix = key + "=";
  std::istringstream ss(content);
  std::string line;
  while (std::getline(ss, line)) {
    line = trim(line);
    if (line.rfind(prefix, 0) == 0)
      return line.substr(prefix.size());
  }
  return "";
}

static void scan_desktop_dir(const std::string& dir)
{
  DIR* d = opendir(dir.c_str());
  if (!d)
    return;

  struct dirent* ent;
  while ((ent = readdir(d)) != nullptr) {
    std::string name = ent->d_name;
    if (name.size() < 9 || name.substr(name.size() - 8) != ".desktop")
      continue;

    std::string path = dir + "/" + name;
    std::ifstream f(path);
    if (!f.is_open())
      continue;

    std::ostringstream oss;
    oss << f.rdbuf();
    std::string content = oss.str();

    // Skip hidden / NoDisplay
    if (content.find("NoDisplay=true") != std::string::npos)
      continue;
    if (content.find("Hidden=true") != std::string::npos)
      continue;

    std::string appName = desktop_field(content, "Name");
    std::string exec = desktop_field(content, "Exec");
    if (appName.empty() || exec.empty())
      continue;

    // Strip field codes %f %F %u %U %i %c %k
    std::string cleaned;
    for (size_t i = 0; i < exec.size(); ++i) {
      if (exec[i] == '%' && i + 1 < exec.size()) {
        ++i;
        continue;
      }
      cleaned.push_back(exec[i]);
    }
    exec = trim(cleaned);

    DesktopApp app;
    app.name = appName;
    app.exec = exec;
    desktopApps.push_back(app);
  }
  closedir(d);
}

static void scan_desktop_apps()
{
  desktopApps.clear();
  const char* home = getenv("HOME");
  if (home) {
    scan_desktop_dir(std::string(home) + "/.local/share/applications");
  }
  scan_desktop_dir("/usr/share/applications");
  scan_desktop_dir("/usr/local/share/applications");

  // Sort by name
  std::sort(desktopApps.begin(), desktopApps.end(),
    [](const DesktopApp& a, const DesktopApp& b) {
      return a.name < b.name;
    });
}

static void filter_launcher()
{
  launcherFiltered.clear();
  std::string q = launcherQuery;
  // lowercase query
  for (char& c : q) {
    if (c >= 'A' && c <= 'Z')
      c = (char)(c + 32);
  }

  for (size_t i = 0; i < desktopApps.size(); ++i) {
    std::string n = desktopApps[i].name;
    for (char& c : n) {
      if (c >= 'A' && c <= 'Z')
        c = (char)(c + 32);
    }
    if (q.empty() || n.find(q) != std::string::npos)
      launcherFiltered.push_back((int)i);
  }

  if (launcherIndex >= launcherFiltered.size())
    launcherIndex = launcherFiltered.empty() ? 0 : launcherFiltered.size() - 1;
}

static void hide_launcher()
{
  if (launcher != None && launcher_active) {
    XUnmapWindow(display, launcher);
    XUngrabKeyboard(display, CurrentTime);
  }
  launcher_active = false;
  launcherQuery.clear();
  launcherIndex = 0;
}

static void draw_launcher()
{
  if (launcher == None || !launcher_active)
    return;

  int visible = (int)std::min(launcherFiltered.size(), (size_t)LAUNCHER_MAX_VISIBLE);
  int height = LAUNCHER_PAD * 2 + LAUNCHER_LINE_H + visible * LAUNCHER_LINE_H + 8;

  GC gc = XCreateGC(display, launcher, 0, nullptr);

  // Background
  XSetForeground(display, gc, 0x1e1e1e);
  XFillRectangle(display, launcher, gc, 0, 0, LAUNCHER_WIDTH, height);

  // Border
  XSetForeground(display, gc, 0x555555);
  XDrawRectangle(display, launcher, gc, 0, 0, LAUNCHER_WIDTH - 1, height - 1);

  // Search box
  XSetForeground(display, gc, 0x2a2a2a);
  XFillRectangle(display, launcher, gc, LAUNCHER_PAD, LAUNCHER_PAD,
                 LAUNCHER_WIDTH - LAUNCHER_PAD * 2, LAUNCHER_LINE_H);

  std::string prompt = "> " + launcherQuery + "_";
  int baseline = LAUNCHER_PAD + (LAUNCHER_LINE_H + (title_font ? title_font->ascent : 10)) / 2 - 2;
  draw_title_text(launcher, LAUNCHER_PAD + 8, baseline, prompt);

  // Results
  int y0 = LAUNCHER_PAD + LAUNCHER_LINE_H + 4;
  for (int i = 0; i < visible; ++i) {
    int y = y0 + i * LAUNCHER_LINE_H;
    if ((size_t)i == launcherIndex) {
      XSetForeground(display, gc, 0x0a64c8);
      XFillRectangle(display, launcher, gc, 4, y, LAUNCHER_WIDTH - 8, LAUNCHER_LINE_H);
    }
    int appIdx = launcherFiltered[i];
    int bl = y + (LAUNCHER_LINE_H + (title_font ? title_font->ascent : 10)) / 2 - 2;
    draw_title_text(launcher, LAUNCHER_PAD + 8, bl, desktopApps[appIdx].name);
  }

  XFreeGC(display, gc);
}

static void show_launcher()
{
  if (desktopApps.empty())
    scan_desktop_apps();

  launcherQuery.clear();
  launcherIndex = 0;
  filter_launcher();

  int visible = (int)std::min(launcherFiltered.size(), (size_t)LAUNCHER_MAX_VISIBLE);
  int height = LAUNCHER_PAD * 2 + LAUNCHER_LINE_H + visible * LAUNCHER_LINE_H + 8;
  int screen_w = DisplayWidth(display, screen);
  int screen_h = DisplayHeight(display, screen);
  int x = (screen_w - LAUNCHER_WIDTH) / 2;
  int y = (screen_h - height) / 3;

  if (launcher == None) {
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0x1e1e1e;
    attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

    launcher = XCreateWindow(
      display, root,
      x, y, LAUNCHER_WIDTH, height,
      1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect | CWBackPixel | CWEventMask,
      &attrs
    );
  } else {
    XMoveResizeWindow(display, launcher, x, y, LAUNCHER_WIDTH, height);
  }

  XMapRaised(display, launcher);
  XGrabKeyboard(display, launcher, True, GrabModeAsync, GrabModeAsync, CurrentTime);
  launcher_active = true;
  draw_launcher();
}

static void launch_selected()
{
  if (launcherFiltered.empty() || launcherIndex >= launcherFiltered.size())
    return;
  int appIdx = launcherFiltered[launcherIndex];
  std::string cmd = desktopApps[appIdx].exec + " >/dev/null 2>&1 &";
  std::system(cmd.c_str());
  hide_launcher();
}

static void handle_launcher_key(XKeyEvent* event)
{
  KeySym sym = XLookupKeysym(event, 0);
  char buf[8] = {};
  XLookupString(event, buf, sizeof(buf) - 1, &sym, nullptr);

  if (sym == XK_Escape) {
    hide_launcher();
    return;
  }
  if (sym == XK_Return) {
    launch_selected();
    return;
  }
  if (sym == XK_Up || sym == XK_KP_Up) {
    if (launcherIndex > 0)
      --launcherIndex;
    draw_launcher();
    return;
  }
  if (sym == XK_Down || sym == XK_KP_Down) {
    if (!launcherFiltered.empty() && launcherIndex + 1 < launcherFiltered.size()
        && launcherIndex + 1 < (size_t)LAUNCHER_MAX_VISIBLE)
      ++launcherIndex;
    draw_launcher();
    return;
  }
  if (sym == XK_BackSpace) {
    if (!launcherQuery.empty()) {
      launcherQuery.pop_back();
      launcherIndex = 0;
      filter_launcher();
      draw_launcher();
    }
    return;
  }

  // Printable character
  if (buf[0] >= 32 && buf[0] < 127) {
    launcherQuery.push_back(buf[0]);
    launcherIndex = 0;
    filter_launcher();
    draw_launcher();
  }
}

static void draw_panel()
{
  if (panel == None)
    return;

  int screen_w = DisplayWidth(display, screen);
  GC gc = XCreateGC(display, panel, 0, nullptr);

  XSetForeground(display, gc, COLOR_PANEL_BG);
  XFillRectangle(display, panel, gc, 0, 0, screen_w, PANEL_HEIGHT);

  time_t now = time(nullptr);
  struct tm* tm = localtime(&now);
  char buf[64];
  // Full month name, e.g. "2026-September-07  23:57:01"
  strftime(buf, sizeof(buf), " %Y-%B-%d  %H:%M:%S", tm);

  int text_h = title_font ? (title_font->ascent + title_font->descent) : 12;
  int baseline = (PANEL_HEIGHT + text_h) / 2 - (title_font ? title_font->descent : 2);
  //int clock_w = title_font ? (int)(strlen(buf) * title_font->max_advance_width / 2) : 120; // rough
  //draw_title_text(panel, screen_w - clock_w - 16, baseline, buf);

  // Left: Start button
  draw_title_text(panel, 10, baseline, " ");

  // Right corner order (from right): Desktop, then time/date
  draw_title_text(panel, screen_w - 20, baseline, "");
  draw_title_text(panel, screen_w - 308, baseline, buf);

  XFreeGC(display, gc);
  panel_last_time = now;
}

static void create_panel()
{
  int screen_w = DisplayWidth(display, screen);
  int screen_h = DisplayHeight(display, screen);

  XSetWindowAttributes attrs{};
  attrs.override_redirect = True;
  attrs.background_pixel = COLOR_PANEL_BG;
  attrs.event_mask = ExposureMask | ButtonPressMask;

  panel = XCreateWindow(
    display, root,
    0, screen_h - PANEL_HEIGHT, screen_w, PANEL_HEIGHT,
    0,
    CopyFromParent, InputOutput, CopyFromParent,
    CWOverrideRedirect | CWBackPixel | CWEventMask,
    &attrs
  );

  XMapRaised(display, panel);
  draw_panel();
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

static void handle_panel_click(int x)
{
  int screen_w = DisplayWidth(display, screen);

  // Left: Start menu
  if (x < 40) {
    if (start_menu_active)
      hide_start_menu();
    else
      show_start_menu();
    return;
  }

  // Right: Desktop icon (far right)
  if (x > screen_w - 50) {
    if (!desktop_showing) {
      for (Client* c : clients) {
        if (!c->minimized)
          minimize_client(c);
      }
      desktop_showing = true;
    }
    else {
      for (Client* c : clients) {
        if (c->minimized) {
          c->minimized = false;
          XMapWindow(display, c->frame);
        }
      }
      desktop_showing = false;
      if (!clients.empty())
        focus_client(clients.back());
    }
    return;
  }
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
            usable_height()
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

static void set_fullscreen(Client* client, bool enable)
{
  if (!client)
    return;

  if (enable && !client->fullscreen) {
    if (!client->maximized) {
      client->old_x = client->x;
      client->old_y = client->y;
      client->old_width = client->width;
      client->old_height = client->height;
    }
    client->fullscreen = true;
    client->maximized = true;
    client->x = 0;
    client->y = 0;
    client->width = DisplayWidth(display, screen);
    client->height = DisplayHeight(display, screen);

    // Borderless: move client to 0,0 inside frame and make frame cover whole screen
    XMoveResizeWindow(display, client->frame, 0, 0,
                      client->width, client->height);
    XMoveResizeWindow(display, client->window, 0, 0,
                      client->width, client->height);
    // Skip normal draw_frame (no decorations)
  }
  else if (!enable && client->fullscreen) {
    client->fullscreen = false;
    client->maximized = false;
    client->x = client->old_x;
    client->y = client->old_y;
    client->width = client->old_width;
    client->height = client->old_height;
    resize_client(client);
  }
  focus_client(client);
}

static void snap_client(Client* client, const std::string& edge)
{
  if (!client)
    return;

  int screen_w = DisplayWidth(display, screen);
  int screen_h = usable_height();

  // Save current geometry so maximize can restore later if needed
  if (!client->maximized) {
    client->old_x = client->x;
    client->old_y = client->y;
    client->old_width = client->width;
    client->old_height = client->height;
  }
  client->maximized = false;

  if (edge == "left") {
    client->x = 0;
    client->y = 0;
    client->width = screen_w / 2 - BORDER_WIDTH * 2;
    client->height = screen_h - TITLE_HEIGHT - BORDER_WIDTH;
  }
  else if (edge == "right") {
    client->x = screen_w / 2;
    client->y = 0;
    client->width = screen_w / 2 - BORDER_WIDTH * 2;
    client->height = screen_h - TITLE_HEIGHT - BORDER_WIDTH;
  }
  else if (edge == "top") {
    client->x = 0;
    client->y = 0;
    client->width = screen_w - BORDER_WIDTH * 2;
    client->height = screen_h / 2 - TITLE_HEIGHT - BORDER_WIDTH;
  }
  else if (edge == "bottom") {
    client->x = 0;
    client->y = screen_h / 2;
    client->width = screen_w - BORDER_WIDTH * 2;
    client->height = screen_h / 2 - TITLE_HEIGHT - BORDER_WIDTH;
  }
  else {
    return;
  }

  if (client->width < MIN_WIDTH)
    client->width = MIN_WIDTH;
  if (client->height < MIN_HEIGHT)
    client->height = MIN_HEIGHT;

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


static unsigned int cursor_for_direction(ResizeDirection direction)
{
  switch (direction) {
    case RESIZE_LEFT:
    case RESIZE_RIGHT:
      return cursor_resize_h;

    case RESIZE_TOP:
    case RESIZE_BOTTOM:
      return cursor_resize_v;

    case RESIZE_TOP_LEFT:
      return cursor_resize_tl;

    case RESIZE_TOP_RIGHT:
      return cursor_resize_tr;

    case RESIZE_BOTTOM_LEFT:
      return cursor_resize_bl;

    case RESIZE_BOTTOM_RIGHT:
      return cursor_resize_br;

    default:
      return cursor_default;
  }
    //switch (direction) {

    //    case RESIZE_LEFT:
    //    case RESIZE_RIGHT:
    //        return XC_sb_h_double_arrow;

    //    case RESIZE_TOP:
    //    case RESIZE_BOTTOM:
    //        return XC_sb_v_double_arrow;

    //    case RESIZE_TOP_LEFT:
    //    case RESIZE_BOTTOM_RIGHT:
    //        return XC_top_left_corner;

    //    case RESIZE_TOP_RIGHT:
    //    case RESIZE_BOTTOM_LEFT:
    //        return XC_top_right_corner;

    //    default:
    //        return XC_left_ptr;
    //}
}


static void update_cursor(
    Client* client,
    int x,
    int y)
{
  ResizeDirection direction = get_resize_direction(client, x, y);
  Cursor cursor = cursor_for_direction(direction);
  XDefineCursor(display, client->frame, cursor);

    //ResizeDirection direction =
    //    get_resize_direction(client, x, y);

    //Cursor cursor = XCreateFontCursor(
    //    display,
    //    cursor_for_direction(direction)
    //);

    //XDefineCursor(
    //    display,
    //    client->frame,
    //    cursor
    //);

    //XFreeCursor(
    //    display,
    //    cursor
    //);
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
  int screen_h = usable_height();

  // Minimum size = 2/3 of usable screen
  int min_w = (screen_w * 2) / 3;
  int min_h = (screen_h * 2) / 3;

  int w = attr.width;
  int h = attr.height;
  if (w < min_w) w = min_w;
  if (h < min_h) h = min_h;

  int frame_w = w + BORDER_WIDTH * 2;
  int frame_h = h + TITLE_HEIGHT + BORDER_WIDTH;

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

  client->width = w;
  client->height = h;

  client->old_x = x;
  client->old_y = y;

  client->old_width = w;
  client->old_height = h;

  client->maximized = false;
  client->minimized = false;
  client->fullscreen = false;

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

            const std::string& cmd = binding.command;
            Client* client = get_focused_client();

            // Internal window-manager actions
            if (cmd == "snap-left") {
              snap_client(client, "left");
              return true;
            }
            if (cmd == "snap-right") {
              snap_client(client, "right");
              return true;
            }
            if (cmd == "snap-top") {
              snap_client(client, "top");
              return true;
            }
            if (cmd == "snap-bottom") {
              snap_client(client, "bottom");
              return true;
            }
            if (cmd == "maximize") {
              maximize_client(client);
              return true;
            }
            if (cmd == "minimize") {
              minimize_client(client);
              return true;
            }
            if (cmd == "fullscreen") {
              if (client)
                set_fullscreen(client, !client->fullscreen);
              return true;
            }
            if (cmd == "center") {
              if (client) {
                if (client->fullscreen)
                  set_fullscreen(client, false);
                if (client->maximized)
                  maximize_client(client); // restore from maximized
                int screen_w = DisplayWidth(display, screen);
                int screen_h = usable_height();
                client->width = (screen_w * 2) / 3;
                client->height = (screen_h * 2) / 3;
                int frame_w = client->width + BORDER_WIDTH * 2;
                int frame_h = client->height + TITLE_HEIGHT + BORDER_WIDTH;
                client->x = (screen_w - frame_w) / 2;
                client->y = (screen_h - frame_h) / 2;
                if (client->x < 0) client->x = 0;
                if (client->y < 0) client->y = 0;
                resize_client(client);
                focus_client(client);
              }
              return true;
            }

            // External command
            std::string command =
                cmd +
                " >/dev/null 2>&1 &";

            printf(
                "mew: running: %s\n",
                cmd.c_str()
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

  create_config_directory();
  load_config();
  load_keybindings();

  load_title_font();

  setenv("XCURSOR_THEME", config.mouseTheme.c_str(), 0);
  char size_buf[16];
  snprintf(size_buf, sizeof(size_buf), "%d", config.mouseSize);
  setenv("XCURSOR_SIZE", size_buf, 0);

  load_cursors();
  XDefineCursor(display, root, cursor_default);

  apply_background();
  play_sound(config.loginSound);

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

  NET_WM_STATE =
      XInternAtom(display, "_NET_WM_STATE", False);
  NET_WM_STATE_FULLSCREEN =
      XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
  NET_WM_STATE_MAXIMIZED_VERT =
      XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
  NET_WM_STATE_MAXIMIZED_HORZ =
      XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);

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

  create_panel();
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

      // reload config + keybindings
      load_config();
      load_keybindings();
      apply_background();
      if (panel != None) {
        XSetWindowBackground(display, panel, COLOR_PANEL_BG);
        draw_panel();
      }

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

    // Update panel clock once per second
    time_t now = time(nullptr);
    if (now != panel_last_time) {
      draw_panel();
    }

    XEvent event;
    if (XPending(display) == 0) {
      // Sleep a bit so we don't busy-loop; still responsive
      struct timespec ts = {0, 50 * 1000 * 1000}; // 50 ms
      nanosleep(&ts, nullptr);
      continue;
    }
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

      case ClientMessage:
      {
        XClientMessageEvent* cm = &event.xclient;
        if (cm->message_type == NET_WM_STATE) {
          Client* client = find_client(cm->window);
          if (!client)
            break;

          // data.l[0]: 0=remove, 1=add, 2=toggle
          // data.l[1], data.l[2]: properties
          long action = cm->data.l[0];
          Atom a1 = (Atom)cm->data.l[1];
          Atom a2 = (Atom)cm->data.l[2];

          bool want_fs = (a1 == NET_WM_STATE_FULLSCREEN || a2 == NET_WM_STATE_FULLSCREEN);
          bool want_max = (a1 == NET_WM_STATE_MAXIMIZED_VERT || a1 == NET_WM_STATE_MAXIMIZED_HORZ ||
                           a2 == NET_WM_STATE_MAXIMIZED_VERT || a2 == NET_WM_STATE_MAXIMIZED_HORZ);

          if (want_fs) {
            if (action == 1 || (action == 2 && !client->fullscreen))
              set_fullscreen(client, true);
            else if (action == 0 || (action == 2 && client->fullscreen))
              set_fullscreen(client, false);
          }
          else if (want_max) {
            if (action == 1 || (action == 2 && !client->maximized)) {
              if (!client->maximized)
                maximize_client(client);
            }
            else if (action == 0 || (action == 2 && client->maximized)) {
              if (client->maximized)
                maximize_client(client);
            }
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

          // Panel click
          if (w == panel) {
              handle_panel_click(event.xbutton.x);
              break;
          }

          // Start menu click
          if (start_menu_active && w == start_menu) {
              handle_start_menu_click(event.xbutton.y);
              break;
          }

          // PowerManager submenu click
          if (power_menu_active && w == power_menu) {
              handle_power_menu_click(event.xbutton.y);
              break;
          }

          // Launcher click (select item)
          if (launcher_active && w == launcher) {
              int y = event.xbutton.y;
              int y0 = LAUNCHER_PAD + LAUNCHER_LINE_H + 4;
              if (y >= y0) {
                size_t idx = (size_t)((y - y0) / LAUNCHER_LINE_H);
                if (idx < launcherFiltered.size() && idx < (size_t)LAUNCHER_MAX_VISIBLE) {
                  launcherIndex = idx;
                  launch_selected();
                }
              }
              break;
          }

          // Click elsewhere closes menus
          if (start_menu_active || power_menu_active)
              hide_start_menu();
          if (launcher_active)
              hide_launcher();

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
          if (event.xexpose.window == keybindings_window) {
            draw_keybindings_window();
            break;
          }
          if (event.xexpose.window == panel) {
            draw_panel();
            break;
          }
          if (event.xexpose.window == launcher) {
            draw_launcher();
            break;
          }
          if (event.xexpose.window == start_menu) {
            draw_start_menu();
            break;
          }
          if (event.xexpose.window == power_menu) {
            draw_power_menu();
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

          // Launcher takes all keys while open
          if (launcher_active) {
            handle_launcher_key(key);
            break;
          }

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

  play_sound(config.logoutSound);
  remove_pidfile();
  XCloseDisplay(display);
  return 0;
}
