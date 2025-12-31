/**
 * @file ProcessRunner.hpp
 * @brief Process management class for starting, stopping, and monitoring processes
 * @version 1.0
 * @date 2025-01-01
 */

#pragma once

#include <vector>
#include <memory>
#include "command.hpp"

/**
 * @brief Process management class
 * 
 * Handles lifecycle management of system processes and Docker containers.
 * Supports starting, stopping, and monitoring multiple processes concurrently.
 */
class ProcessRunner {
public:
    /**
     * @brief Constructor
     * @param commands Reference to vector of command structures to manage
     */
    explicit ProcessRunner(std::vector<command>& commands);
    
    /**
     * @brief Destructor - cleans up any running processes
     */
    ~ProcessRunner();
    
    // Disable copy constructor and assignment operator
    ProcessRunner(const ProcessRunner&) = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;
    
    /**
     * @brief Start a process at the specified index
     * @param index Index of the command in the commands vector
     * @return Process ID on success, -1 on failure
     * 
     * Forks a new process and executes the command based on its mode:
     * - Mode 'C': Executes as a regular system command
     * - Mode 'D': Executes as a Docker container using 'docker start'
     */
    pid_t start(size_t index);
    
    /**
     * @brief Terminate a process at the specified index
     * @param index Index of the command in the commands vector
     * @param force If true, sends SIGKILL; otherwise sends SIGTERM
     * @return true on success, false on failure
     * 
     * For regular processes: sends SIGTERM or SIGKILL signal
     * For Docker containers: executes 'docker stop' or 'docker kill'
     */
    bool kill(size_t index, bool force = false);
    
    /**
     * @brief Get the process ID for a command
     * @param index Index of the command in the commands vector
     * @return Process ID if running, -1 if not running or invalid index
     */
    pid_t getPid(size_t index) const;
    
    /**
     * @brief Check if a process is currently running
     * @param index Index of the command in the commands vector
     * @return true if process is running, false otherwise
     */
    bool isRunning(size_t index) const;
    
    /**
     * @brief Get the number of managed commands
     * @return Number of commands in the vector
     */
    size_t getCommandCount() const;

private:
    std::vector<command>& commands_;  ///< Reference to managed commands
    
    /**
     * @brief Split command line into individual arguments
     * @param cmdline Command line string to split
     * @return Vector of command arguments
     */
    static std::vector<std::string> splitCommand(const std::string& cmdline);
};
