#ifndef MEW_STRINGS_H
#define MEW_STRINGS_H

#include <string>

namespace Mew
{
  class Strings
  {
    public:
      static std::string trim(const std::string& str);

      static std::string expandHome(const std::string& path);

    private:
  };
}

#endif // MEW_STRINGS_H
