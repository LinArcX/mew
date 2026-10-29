// Single translation unit providing the stb_image implementation.
// Background.cpp and AppLauncher.cpp include stb_image.h as a plain header;
// third_party/nanovg/nanovg.c is compiled with -DNVG_NO_STB so it does not
// pull in its own (incompatible) copy of the implementation.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
