/**
 * @file command.hpp
 * @brief Command structure definition for process management
 * @version 1.0
 * @date 2025-01-01
 */

#pragma once
#include <string>

/**
 * @brief Process status enumeration
 */
enum STATUS {
    DEAD = 0,    ///< Process is not running
    RUNNING = 1  ///< Process is currently running
};

/**
 * @brief Command structure representing a manageable process
 * 
 * This structure holds all information needed to manage a system process
 * or Docker container, including execution parameters and current status.
 */
struct command {
    std::string Desc;           ///< Human-readable description of the command
    std::string Path;           ///< Command path or Docker image name
    char        Mode = 'C';     ///< Execution mode: 'C' for command, 'D' for Docker
    std::string Folder = ".";   ///< Working directory for command execution
    short       Status = DEAD;  ///< Current process status (DEAD/RUNNING)
    int         Pid = -1;       ///< Process ID when running (-1 if not running)
    
    /**
     * @brief Default constructor
     */
    command() = default;
    
    /**
     * @brief Constructor with basic parameters
     * @param desc Process description
     * @param path Command path or Docker image
     * @param mode Execution mode ('C' or 'D')
     * @param folder Working directory
     */
    command(const std::string& desc, const std::string& path, 
            char mode = 'C', const std::string& folder = ".") 
        : Desc(desc), Path(path), Mode(mode), Folder(folder) {}
};
