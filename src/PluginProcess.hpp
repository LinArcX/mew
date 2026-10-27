#pragma once

#include <string>
#include <sys/types.h>

/**
 * @brief Manages an out-of-process plugin child.
 *
 * Spawns a child process via /bin/sh -c, captures its stdout/stderr
 * asynchronously via a non-blocking pipe, and monitors its exit status.
 * This guarantees that a hanging or crashing plugin never freezes the WM.
 */
class PluginProcess
{
public:
  PluginProcess();
  ~PluginProcess();

  /**
   * @brief Spawn the given command.
   * @param command The shell command to run.
   * @return true if the process was successfully forked.
   */
  bool start(const std::string& command);

  /**
   * @brief Non-blocking check for new output and child exit.
   * Must be called periodically from the main loop.
   */
  void tick();

  /**
   * @brief Kill the child process and reap it.
   */
  void stop();

  /** @brief Whether the process is still running. */
  bool isRunning() const { return m_pid > 0 && !m_exited; }

  /** @brief Get the latest output read from the child. */
  const std::string& output() const { return m_output; }

  /** @brief Human-readable status for the diagnostics window. */
  std::string statusString() const;

private:
  pid_t m_pid = -1;
  int m_fd = -1;
  bool m_exited = false;
  int m_exitCode = 0;
  std::string m_command;
  std::string m_output;
};
