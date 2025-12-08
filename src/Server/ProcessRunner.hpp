#pragma once

#include <vector>
#include "command.hpp"   // your definition

class ProcessRunner {
public:
    explicit ProcessRunner(std::vector<command> &commands);
    ~ProcessRunner();

    // Start the command at given index. Returns child PID, or -1 on error
    pid_t start(size_t index);

    // Kill the command process at given index. If force == true, send SIGKILL; else SIGTERM.
    bool kill(size_t index, bool force = false);

    // Get the PID of the process at index (or -1 if not started)
    pid_t getPid(size_t index) const;

private:
    std::vector<command> &commands_;
};
