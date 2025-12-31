/**
 * @file ProcessRunner.cpp
 * @brief Implementation of ProcessRunner class for process lifecycle management
 * @version 1.0
 * @date 2025-01-01
 */

#include "ProcessRunner.hpp"

#include <unistd.h>     // fork, execvp, chdir
#include <signal.h>     // kill, SIGTERM, SIGKILL
#include <sys/wait.h>   // waitpid
#include <cstring>      // strdup
#include <cerrno>       // errno
#include <iostream>     // std::cerr
#include <sstream>      // std::istringstream
#include <algorithm>    // std::find_if

ProcessRunner::ProcessRunner(std::vector<command>& commands)
    : commands_(commands) {
    // Initialize all PIDs to -1 (not running)
    for (auto& cmd : commands_) {
        cmd.Pid = -1;
        cmd.Status = DEAD;
    }
}

ProcessRunner::~ProcessRunner() {
    // Attempt to gracefully terminate all running processes
    for (size_t i = 0; i < commands_.size(); ++i) {
        if (commands_[i].Status == RUNNING && commands_[i].Pid > 0) {
            std::cerr << "Terminating process " << commands_[i].Pid 
                      << " (" << commands_[i].Desc << ")" << std::endl;
            kill(i, false); // Try graceful termination first
        }
    }
}

std::vector<std::string> ProcessRunner::splitCommand(const std::string& cmdline) {
    std::istringstream iss(cmdline);
    std::vector<std::string> tokens;
    std::string token;
    
    // Simple whitespace-based splitting
    // TODO: Add support for quoted arguments with spaces
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

pid_t ProcessRunner::start(size_t index) {
    // Validate index
    if (index >= commands_.size()) {
        std::cerr << "ProcessRunner::start: Invalid index " << index << std::endl;
        return -1;
    }
    
    command& cmd = commands_[index];
    
    // Check if already running
    if (cmd.Status == RUNNING && cmd.Pid > 0) {
        std::cerr << "ProcessRunner::start: Process already running (PID: " 
                  << cmd.Pid << ")" << std::endl;
        return cmd.Pid;
    }
    
    // Validate command path
    if (cmd.Path.empty()) {
        std::cerr << "ProcessRunner::start: Empty command path" << std::endl;
        return -1;
    }
    
    std::cout << "Starting process: " << cmd.Desc << " (" << cmd.Path << ")" << std::endl;
    
    // Fork new process
    pid_t pid = fork();
    if (pid < 0) {
        perror("ProcessRunner::start: fork failed");
        return -1;
    }
    
    if (pid == 0) {
        // CHILD PROCESS
        
        // Change working directory if specified
        if (!cmd.Folder.empty() && cmd.Folder != ".") {
            if (chdir(cmd.Folder.c_str()) != 0) {
                perror("ProcessRunner::start: chdir failed");
                _exit(1);
            }
        }
        
        if (cmd.Mode == 'C') {
            // Regular command execution
            auto parts = splitCommand(cmd.Path);
            if (parts.empty()) {
                std::cerr << "ProcessRunner::start: No command parts found" << std::endl;
                _exit(1);
            }
            
            // Prepare argv array
            std::vector<char*> argv;
            argv.reserve(parts.size() + 1);
            for (const auto& part : parts) {
                argv.push_back(const_cast<char*>(part.c_str()));
            }
            argv.push_back(nullptr);
            
            // Execute command
            execvp(argv[0], argv.data());
            perror("ProcessRunner::start: execvp failed (C mode)");
            _exit(1);
            
        } else if (cmd.Mode == 'D') {
            // Docker container execution
            auto parts = splitCommand(cmd.Path);
            if (parts.empty()) {
                std::cerr << "ProcessRunner::start: No Docker image specified" << std::endl;
                _exit(1);
            }
            
            // Prepare Docker command: docker start <container_name>
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>("docker"));
            argv.push_back(const_cast<char*>("start"));
            for (const auto& part : parts) {
                argv.push_back(const_cast<char*>(part.c_str()));
            }
            argv.push_back(nullptr);
            
            // Execute Docker command
            execvp("docker", argv.data());
            perror("ProcessRunner::start: execvp failed (Docker mode)");
            _exit(1);
            
        } else {
            std::cerr << "ProcessRunner::start: Unknown mode '" << cmd.Mode << "'" << std::endl;
            _exit(1);
        }
    } else {
        // PARENT PROCESS
        cmd.Pid = pid;
        cmd.Status = RUNNING;
        std::cout << "Process started successfully (PID: " << pid << ")" << std::endl;
        return pid;
    }
}

bool ProcessRunner::kill(size_t index, bool force) {
    // Validate index
    if (index >= commands_.size()) {
        std::cerr << "ProcessRunner::kill: Invalid index " << index << std::endl;
        return false;
    }
    
    command& cmd = commands_[index];
    
    // Check if process is running
    if (cmd.Status != RUNNING || cmd.Pid <= 0) {
        std::cerr << "ProcessRunner::kill: Process not running" << std::endl;
        return false;
    }
    
    std::cout << "Terminating process: " << cmd.Desc << " (PID: " << cmd.Pid 
              << ", force: " << (force ? "yes" : "no") << ")" << std::endl;
    
    if (cmd.Mode == 'C') {
        // Regular process termination
        int signal = force ? SIGKILL : SIGTERM;
        if (::kill(cmd.Pid, signal) == 0) {
            cmd.Status = DEAD;
            cmd.Pid = -1;
            std::cout << "Process terminated successfully" << std::endl;
            return true;
        } else {
            perror("ProcessRunner::kill: kill failed");
            return false;
        }
        
    } else if (cmd.Mode == 'D') {
        // Docker container termination
        pid_t child = fork();
        if (child < 0) {
            perror("ProcessRunner::kill: fork failed (Docker mode)");
            return false;
        }
        
        if (child == 0) {
            // Child process - execute Docker command
            const char* action = force ? "kill" : "stop";
            
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>("docker"));
            argv.push_back(const_cast<char*>(action));
            argv.push_back(const_cast<char*>(cmd.Path.c_str()));
            argv.push_back(nullptr);
            
            execvp("docker", argv.data());
            perror("ProcessRunner::kill: execvp docker failed");
            _exit(1);
        } else {
            // Parent process - wait for Docker command to complete
            int status;
            waitpid(child, &status, 0);
            
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                cmd.Status = DEAD;
                cmd.Pid = -1;
                std::cout << "Docker container terminated successfully" << std::endl;
                return true;
            } else {
                std::cerr << "ProcessRunner::kill: Docker command failed" << std::endl;
                return false;
            }
        }
    }
    
    std::cerr << "ProcessRunner::kill: Unknown mode '" << cmd.Mode << "'" << std::endl;
    return false;
}

pid_t ProcessRunner::getPid(size_t index) const {
    if (index >= commands_.size()) {
        return -1;
    }
    return commands_[index].Pid;
}

bool ProcessRunner::isRunning(size_t index) const {
    if (index >= commands_.size()) {
        return false;
    }
    return commands_[index].Status == RUNNING && commands_[index].Pid > 0;
}

size_t ProcessRunner::getCommandCount() const {
    return commands_.size();
}
