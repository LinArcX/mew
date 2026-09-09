#ifndef MEW_KEYBINDINGSWINDOW_H
#define MEW_KEYBINDINGSWINDOW_H

#include "panel.h"
#include "configs.h"

#include <vector>

#include <X11/Xlib.h>

namespace Mew
{
  class KeyBindingsWindow
  {
    public:
      void raiseKeybindingsWindowIfActive(Display* display, Panel* panel);
 
      void buildKeybindingsDisplay(Configs* configs);
 
      void drawKeybindingsWindow();

      void showKeybindingsWindow();

      void moveKeybindingsWindow();

      void keybindingsWindowApplyGeometry();

      void loadKeybindings();

      void hideKeybindingsWindow();
 
      int kbCloseButtonX();
 
      void handleKeybindingsWindowClick(XButtonEvent* event);
 
      void grabKey(KeyCode keycode, unsigned int modifiers);
 
      void grabKeys();
 
    private:
      Window keybindings_window = None;

      bool keybindings_window_active = false;

      std::vector<std::string> kb_display_lines;
  };
}

#endif // MEW_KEYBINDINGSWINDOW_H
