#include "keyBindingsWindow.h"

#include <sstream>

#include <X11/Xlib.h>

namespace Mew
{
  void KeyBindingsWindow::raiseKeybindingsWindowIfActive(Display* display, Panel* panel)
  {
    if (keybindings_window != None && keybindings_window_active) {
      XRaiseWindow(display, keybindings_window);
    }
    if (panel != None) {
      XRaiseWindow(display, panel);
    }
  }
  
  void KeyBindingsWindow::buildKeybindingsDisplay(Configs* configs)
  {
    kb_display_lines.clear();
    kb_display_lines.push_back(configs->getConfigDirectory() + "/keybindings");
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
  
  void KeyBindingsWindow::drawKeybindingsWindow()
  {
    if (keybindings_window == None || !keybindings_window_active) {
      return;
    }
  
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
  
    // Close
    XDrawLine(display, keybindings_window, gc, close_x + 9, 8, close_x + BUTTON_WIDTH - 9, TITLE_HEIGHT - 9);
    XDrawLine(display, keybindings_window, gc, close_x + BUTTON_WIDTH - 9, 8, close_x + 9, TITLE_HEIGHT - 9);
  
    // Title text
    int text_height = titleFont ? (titleFont->ascent + titleFont->descent) : 10;
    int title_baseline = BORDER_WIDTH
      + (TITLE_HEIGHT - BORDER_WIDTH - text_height) / 2
      + (titleFont ? titleFont->ascent : 10);
    windows.drawTitleText(keybindings_window, BORDER_WIDTH + 8, title_baseline, "Keybindings");
  
    // Content area background
    XSetForeground(display, gc, COLOR_SWITCHER_BG);
    XFillRectangle(display, keybindings_window, gc,
                   BORDER_WIDTH, TITLE_HEIGHT,
                   kb_win_width - BORDER_WIDTH * 2,
                   kb_win_height - TITLE_HEIGHT - BORDER_WIDTH);
  
    // Content text
    int y = TITLE_HEIGHT + KB_PADDING + (titleFont ? titleFont->ascent : 12);
    for (const std::string& line : kb_display_lines) {
      if (!line.empty()) {
          windows.drawTitleText(keybindings_window, BORDER_WIDTH + KB_PADDING, y, line);
      }
      y += KB_LINE_HEIGHT;
      if (y > kb_win_height - BORDER_WIDTH - 4) {
        break; // don't draw past the window (no scrolling yet)
      }
    }
    XFreeGC(display, gc);
  }
   
  void KeyBindingsWindow::keybindingsWindowApplyGeometry()
  {
    XMoveResizeWindow(display, keybindings_window, kb_win_x, kb_win_y, kb_win_width, kb_win_height);
    drawKeybindingsWindow();
  }
  
  void KeyBindingsWindow::moveKeybindingsWindow()
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
      None, None, CurrentTime);
  
    XEvent event;
    while (true) {
      XMaskEvent(display, ButtonMotionMask | ButtonReleaseMask, &event);
  
      if (event.type == MotionNotify) {
        int dx = event.xmotion.x_root - root_x;
        int dy = event.xmotion.y_root - root_y;
  
        kb_win_x = start_x + dx;
        kb_win_y = start_y + dy;
  
        keybindingsWindowApplyGeometry();
      }
  
      if (event.type == ButtonRelease) {
        break;
      }
    }
    XUngrabPointer(display, CurrentTime);
  }
  
  void KeyBindingsWindow::showKeybindingsWindow()
  {
    buildKeybindingsDisplay();
  
    int screen_w = DisplayWidth(display, screen);
    int screen_h = DisplayHeight(display, screen);
  
    if (keybindings_window == None) {
      kb_win_width = KB_DEFAULT_WIDTH;
      kb_win_height = std::min(
        (int)(TITLE_HEIGHT + BORDER_WIDTH + KB_PADDING * 2 + kb_display_lines.size() * KB_LINE_HEIGHT + 12),
        screen_h - 80);
      kb_win_x = (screen_w - kb_win_width) / 2;
      kb_win_y = (screen_h - kb_win_height) / 2;
  
      XSetWindowAttributes attrs{};
      attrs.override_redirect = True;
      attrs.background_pixel  = COLOR_BORDER;
      attrs.border_pixel      = COLOR_SWITCHER_BORDER;
      attrs.event_mask        = ExposureMask | ButtonPressMask;
  
      keybindings_window = XCreateWindow(display, root,
        kb_win_x, kb_win_y, kb_win_width, kb_win_height,
        1,
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
        &attrs
      );
    }
    else {
      // Recompute height in case keybindings changed since last open
      kb_win_height = std::min(
          (int)(TITLE_HEIGHT + BORDER_WIDTH + KB_PADDING * 2 + kb_display_lines.size() * KB_LINE_HEIGHT + 12),
          screen_h - 80);
      XMoveResizeWindow(display, keybindings_window, kb_win_x, kb_win_y, kb_win_width, kb_win_height);
    }
  
    XMapRaised(display, keybindings_window);
    keybindings_window_active = true;
    drawKeybindingsWindow();
  }

  void KeyBindingsWindow::loadKeybindings()
  {
    keybindings.clear();
  
    std::string path = configs.getConfigDirectory() + "/keybindings";
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
      binding.command = strings.expandHome(command);
      binding.display = expand_key_string(key_string);
  
      keybindings.push_back(binding);
  
      printf("mew: keybinding %s -> %s\n", key_string.c_str(), binding.command.c_str());
    }
  }

  void KeyBindingsWindow::hideKeybindingsWindow()
  {
    if (keybindings_window != None && keybindings_window_active) {
      XUnmapWindow(display, keybindings_window);
    }
    keybindings_window_active = false;
  }
  
  int KeyBindingsWindow::kbCloseButtonX()
  {
    return kb_win_width - BUTTON_WIDTH;
  }
  
  void KeyBindingsWindow::handleKeybindingsWindowClick(XButtonEvent* event)
  {
    if (event->y < BORDER_WIDTH || event->y >= TITLE_HEIGHT) {
      return; // clicks below the titlebar do nothing for now
    }
  
    int close_x = kbCloseButtonX();
  
    if (event->x >= close_x) {
      hideKeybindingsWindow();
      return;
    }
  
    // Anywhere else on the titlebar -> drag to move
    moveKeybindingsWindow();
  }

  void KeyBindingsWindow::grabKey(KeyCode keycode, unsigned int modifiers)
  {
    unsigned int lock_masks[] = {
      0,
      LockMask,
      Mod2Mask,
      LockMask | Mod2Mask
    };
  
    for (unsigned int lock : lock_masks) {
      XGrabKey(display,
        keycode,
        modifiers | lock,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync);
    }
  }
  
  void KeyBindingsWindow::grabKeys()
  {
    // Built-in shortcuts
    grabKey(XKeysymToKeycode(display, XK_Tab), Mod1Mask);                  // Alt+Tab
    grabKey(XKeysymToKeycode(display, XK_Tab), Mod1Mask | ShiftMask);      // Alt+Shift+Tab
    grabKey(XKeysymToKeycode(display, XK_F4), Mod1Mask);
    grabKey(XKeysymToKeycode(display, XK_F1), Mod1Mask);
    grabKey(XKeysymToKeycode(display, XK_q), Mod1Mask | ShiftMask);
  
    // User-configured shortcuts
    for (const KeyBinding& binding : keybindings) {
      grabKey(binding.keycode, binding.modifiers);
    }
  }
}
