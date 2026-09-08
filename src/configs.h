#ifndef MEW_CONFIGS_H
#define MEW_CONFIGS_H

#include "panel.h"

#include <string>

namespace Mew
{
  class Configs
  {
    public:
      void load(Panel * const panel);

      std::string getConfigDirectory();

      void applyBackground(
        Window root,
        const int screen,
        Display * display);

    private:
      double titleFontSize = 17.0;

      std::string mouseTheme = "";

      int mouseSize = 24;

      unsigned long backgroundColor = 0x3B3C3C;

      unsigned long panelColor = 0x222222;

      std::string backgroundImage;

      // last of color/image in config wins
      bool useBackgroundImage = false; 

      std::string loginSound;

      std::string logoutSound;

      // reserved
      std::string windowTheme;

      Pixmap backgroundPixmap = None;
  };
}

#endif // MEW_CONFIGS_H
