#include "strings.h"

namespace Mew
{
  std::string Strings::trim(const std::string& str)
  {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
      return "";
    }
  
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
  }
  
  std::string Strings::expandHome(const std::string& path)
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
}
