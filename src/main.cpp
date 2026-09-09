#include "Mew.hpp"
#include "Util.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <csignal>
#include <unistd.h>

static volatile sig_atomic_t g_needReconfigure = 0;

static void sighupHandler(int)
{
  g_needReconfigure = 1;
  if (Mew::instance())
  {
    Mew::instance()->requestReconfigure();
  }
}

int main(int argc, char** argv)
{
  bool doReconfigure = false;
  for (int i = 1; i < argc; ++i)
  {
    if (std::strcmp(argv[i], "--reconfigure") == 0)
    {
      doReconfigure = true;
    }
    else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
    {
      printf("Usage: mew [--reconfigure]\n");
      return 0;
    }
  }

  if (doReconfigure)
  {
    std::ifstream f(Util::getPidfile());
    pid_t pid = 0;
    if (f >> pid && pid > 1)
    {
      if (kill(pid, SIGHUP) == 0)
      {
        printf("mew: reconfigure sent to pid %d\n", static_cast<int>(pid));
        return 0;
      }
      fprintf(stderr, "mew: failed to signal pid %d\n", static_cast<int>(pid));
      return 1;
    }
    fprintf(stderr, "mew: no running instance found\n");
    return 1;
  }

  struct sigaction sa{};
  sa.sa_handler = sighupHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGHUP, &sa, nullptr);

  Mew mew;
  return mew.run();
}
