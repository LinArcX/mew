#ifndef MEW_CORE_H
#define MEW_CORE_H

#include <string>
#include <vector>

#include <X11/Xlib.h>

struct Client
{
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

struct KeyBinding
{
  KeyCode keycode;

  unsigned int modifiers;

  std::string command;

  // human-readable form, e.g. "Win-q"
  std::string display;   
};

enum ResizeDirection
{
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

namespace Mew
{
  class Core
  {
    public:
      std::vector<Client*> clients;
      std::vector<KeyBinding> keybindings;
 
    private:
  };
}

#endif // MEW_CORE_H
