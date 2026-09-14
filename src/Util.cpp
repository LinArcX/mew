#include "Util.hpp"

#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace Util
{

std::string trim(const std::string& str)
{
  size_t start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
  {
    return "";
  }
  size_t end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

std::string expandHome(const std::string& path)
{
  if (path == "~")
  {
    const char* home = getenv("HOME");
    return home ? std::string(home) : path;
  }
  if (path.rfind("~/", 0) == 0)
  {
    const char* home = getenv("HOME");
    if (!home)
    {
      return path;
    }
    return std::string(home) + path.substr(1);
  }
  return path;
}

std::string codepointToUtf8(unsigned int codepoint)
{
  std::string out;
  if (codepoint < 0x80)
  {
    out.push_back(static_cast<char>(codepoint));
  }
  else if (codepoint < 0x800)
  {
    out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  else if (codepoint < 0x10000)
  {
    out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  else if (codepoint < 0x110000)
  {
    out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  return out;
}

std::string getConfigDirectory()
{
  const char* home = getenv("HOME");
  if (!home)
  {
    return "";
  }
  return std::string(home) + "/.config/mew";
}

void createConfigDirectory()
{
  const char* home = getenv("HOME");
  if (!home)
  {
    return;
  }
  std::string config = std::string(home) + "/.config";
  std::string mew = config + "/mew";
  mkdir(config.c_str(), 0755);
  mkdir(mew.c_str(), 0755);
}

std::string getPidfile()
{
  return "/tmp/mew.pid";
}

void writePidfile()
{
  std::ofstream f(getPidfile());
  if (f)
  {
    f << getpid() << '\n';
  }
}

void removePidfile()
{
  unlink(getPidfile().c_str());
}

}
