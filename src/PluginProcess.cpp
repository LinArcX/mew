#include "PluginProcess.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>

PluginProcess::PluginProcess()
{
}

PluginProcess::~PluginProcess()
{
  stop();
}

bool PluginProcess::start(const std::string& command)
{
  stop();

  m_command = command;
  m_output.clear();
  m_exited = false;
  m_exitCode = 0;

  int pipefd[2];
  if (pipe(pipefd) == -1)
  {
    return false;
  }

  m_pid = fork();
  if (m_pid == -1)
  {
    close(pipefd[0]);
    close(pipefd[1]);
    m_pid = -1;
    return false;
  }

  if (m_pid == 0)
  {
    // Child
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);

    execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
    _exit(1);
  }

  // Parent
  close(pipefd[1]);
  m_fd = pipefd[0];
  int flags = fcntl(m_fd, F_GETFL, 0);
  if (flags != -1)
  {
    fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);
  }

  return true;
}

void PluginProcess::tick()
{
  if (m_pid > 0 && !m_exited)
  {
    int status = 0;
    pid_t res = waitpid(m_pid, &status, WNOHANG);
    if (res == m_pid)
    {
      m_exited = true;
      if (WIFEXITED(status))
      {
        m_exitCode = WEXITSTATUS(status);
      }
      else if (WIFSIGNALED(status))
      {
        m_exitCode = 128 + WTERMSIG(status);
      }
    }
  }

  if (m_fd >= 0)
  {
    char buf[256];
    ssize_t n = 0;
    while ((n = read(m_fd, buf, sizeof(buf) - 1)) > 0)
    {
      buf[n] = '\0';
      m_output += buf;
      if (m_output.size() > 4096)
      {
        m_output.erase(0, m_output.size() - 4096);
      }
    }
    if (n == 0)
    {
      close(m_fd);
      m_fd = -1;
    }
  }
}

void PluginProcess::stop()
{
  if (m_pid > 0)
  {
    kill(m_pid, SIGTERM);
    int status = 0;
    waitpid(m_pid, &status, 0);
    m_pid = -1;
  }
  if (m_fd >= 0)
  {
    close(m_fd);
    m_fd = -1;
  }
}

std::string PluginProcess::statusString() const
{
  if (m_pid <= 0)
  {
    return "Not running";
  }
  if (m_exited)
  {
    return "Crashed/Exited (Code: " + std::to_string(m_exitCode) + ")";
  }
  return "Running (PID: " + std::to_string(m_pid) + ")";
}
