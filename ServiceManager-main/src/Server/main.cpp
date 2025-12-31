/**
 * @file main.cpp
 * @brief Process Management Server - HTTP API for managing system processes
 * @version 1.0
 * @date 2025-01-01
 * 
 * This server provides a REST API for managing system processes and Docker containers.
 * It reads configuration from a file and exposes HTTP endpoints for process control.
 * 
 * Endpoints:
 * - GET /process/list - Returns JSON array of all processes and their status
 * - POST /process/control - Controls processes (start/stop/kill/status)
 */

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <limits>
#include <filesystem>
#include <cstdlib>

#include "command.hpp"
#include "httplib.h"
#include "ProcessRunner.hpp"

// Configuration constants
constexpr int DEFAULT_PORT = 6755;
constexpr const char* DEFAULT_CONFIG_PATH = "./config/cmds.conf";
constexpr const char* FALLBACK_CONFIG_PATH = "/home/raima/.sermn/cmds.conf";

// Global variables
std::vector<command> g_commands;
std::unique_ptr<ProcessRunner> g_processRunner;
std::string g_configPath;
int g_port = DEFAULT_PORT;

// Function declarations
int initializeSystem();
bool loadConfiguration();
void startHttpServer();
void printUsage(const char* programName);
std::string escapeJsonString(const std::string& input);

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    std::cout << "🚀 Process Management Server v1.0" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--config" || arg == "-c") {
            if (i + 1 < argc) {
                g_configPath = argv[++i];
            } else {
                std::cerr << "Error: --config requires a file path" << std::endl;
                return 1;
            }
        } else if (arg == "--port" || arg == "-p") {
            if (i + 1 < argc) {
                try {
                    g_port = std::stoi(argv[++i]);
                    if (g_port <= 0 || g_port > 65535) {
                        throw std::out_of_range("Port out of range");
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid port number" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --port requires a port number" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Initialize system
    if (initializeSystem() != 0) {
        return 1;
    }
    
    // Load configuration
    if (!loadConfiguration()) {
        return 1;
    }
    
    // Create process runner
    g_processRunner = std::make_unique<ProcessRunner>(g_commands);
    
    std::cout << "✅ Loaded " << g_commands.size() << " commands from configuration" << std::endl;
    std::cout << "🌐 Starting HTTP server on port " << g_port << std::endl;
    
    // Start HTTP server
    startHttpServer();
    
    return 0;
}

/**
 * @brief Initialize system and validate configuration
 */
int initializeSystem() {
    // Determine configuration file path
    if (g_configPath.empty()) {
        if (std::filesystem::exists(DEFAULT_CONFIG_PATH)) {
            g_configPath = DEFAULT_CONFIG_PATH;
        } else if (std::filesystem::exists(FALLBACK_CONFIG_PATH)) {
            g_configPath = FALLBACK_CONFIG_PATH;
        } else {
            std::cerr << "❌ Configuration file not found!" << std::endl;
            std::cerr << "   Searched locations:" << std::endl;
            std::cerr << "   - " << DEFAULT_CONFIG_PATH << std::endl;
            std::cerr << "   - " << FALLBACK_CONFIG_PATH << std::endl;
            std::cerr << "   Use --config to specify a custom location" << std::endl;
            return 1;
        }
    }
    
    // Validate configuration file exists
    if (!std::filesystem::exists(g_configPath)) {
        std::cerr << "❌ Configuration file not found: " << g_configPath << std::endl;
        return 1;
    }
    
    std::cout << "📋 Using configuration file: " << g_configPath << std::endl;
    return 0;
}

/**
 * @brief Load commands from configuration file
 * 
 * Configuration file format:
 * Line 1: Number of commands (N)
 * For each command (N times):
 *   Line 1: Description
 *   Line 2: Mode (C for command, D for Docker)
 *   Line 3: Path/Command
 *   Line 4: Working directory
 */
bool loadConfiguration() {
    std::ifstream file(g_configPath);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open configuration file: " << g_configPath << std::endl;
        return false;
    }
    
    int numCommands;
    if (!(file >> numCommands) || numCommands < 0) {
        std::cerr << "❌ Invalid number of commands in configuration file" << std::endl;
        return false;
    }
    
    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    
    g_commands.clear();
    g_commands.reserve(numCommands);
    
    for (int i = 0; i < numCommands; ++i) {
        command cmd;
        
        // Read description
        if (!std::getline(file, cmd.Desc)) {
            std::cerr << "❌ Failed to read description for command " << i << std::endl;
            return false;
        }
        
        // Read mode
        if (!(file >> cmd.Mode)) {
            std::cerr << "❌ Failed to read mode for command " << i << std::endl;
            return false;
        }
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        // Validate mode
        if (cmd.Mode != 'C' && cmd.Mode != 'D') {
            std::cerr << "❌ Invalid mode '" << cmd.Mode << "' for command " << i 
                      << ". Must be 'C' (command) or 'D' (Docker)" << std::endl;
            return false;
        }
        
        // Read path
        if (!std::getline(file, cmd.Path)) {
            std::cerr << "❌ Failed to read path for command " << i << std::endl;
            return false;
        }
        
        // Read folder
        if (!std::getline(file, cmd.Folder)) {
            std::cerr << "❌ Failed to read folder for command " << i << std::endl;
            return false;
        }
        
        // Validate required fields
        if (cmd.Desc.empty() || cmd.Path.empty()) {
            std::cerr << "❌ Empty description or path for command " << i << std::endl;
            return false;
        }
        
        g_commands.push_back(std::move(cmd));
        std::cout << "📝 Loaded: " << cmd.Desc << " (" << cmd.Mode << ")" << std::endl;
    }
    
    return true;
}

/**
 * @brief Escape special characters in JSON strings
 */
std::string escapeJsonString(const std::string& input) {
    std::string result;
    result.reserve(input.length() * 2); // Reserve space for potential escaping
    
    for (char c : input) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b";  break;
            case '\f': result += "\\f";  break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:   result += c;      break;
        }
    }
    
    return result;
}

/**
 * @brief Start HTTP server and handle requests
 */
void startHttpServer() {
    httplib::Server server;
    
    // Enable CORS for web interface
    server.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // Handle OPTIONS requests for CORS
    server.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        return; // Headers already set in pre-routing handler
    });
    
    /**
     * GET /process/list - Return list of all processes with their status
     */
    server.Get("/process/list", [](const httplib::Request&, httplib::Response& res) {
        try {
            std::string jsonResponse = "[\n";
            
            for (size_t i = 0; i < g_commands.size(); ++i) {
                if (i > 0) {
                    jsonResponse += ",\n";
                }
                
                const auto& cmd = g_commands[i];
                jsonResponse += "  {\n";
                jsonResponse += "    \"id\": " + std::to_string(i) + ",\n";
                jsonResponse += "    \"desc\": \"" + escapeJsonString(cmd.Desc) + "\",\n";
                jsonResponse += "    \"status\": \"" + (cmd.Status == RUNNING ? "RUNNING" : "DEAD") + "\",\n";
                jsonResponse += "    \"mode\": \"" + std::string(1, cmd.Mode) + "\",\n";
                jsonResponse += "    \"pid\": " + std::to_string(cmd.Pid) + "\n";
                jsonResponse += "  }";
            }
            
            jsonResponse += "\n]";
            
            res.set_content(jsonResponse, "application/json");
            res.status = 200;
            
        } catch (const std::exception& e) {
            std::cerr << "Error in /process/list: " << e.what() << std::endl;
            res.status = 500;
            res.set_content("Internal server error", "text/plain");
        }
    });
    
    /**
     * POST /process/control - Control processes (start/stop/kill/status)
     * Parameters:
     * - fn: Function to execute (start/stop/kill/end/status)
     * - id: Process ID (index in commands array)
     */
    server.Post("/process/control", [](const httplib::Request& req, httplib::Response& res) {
        try {
            // Validate required parameters
            if (!req.has_param("fn") || !req.has_param("id")) {
                res.status = 400;
                res.set_content("Missing required parameters: fn and id", "text/plain");
                return;
            }
            
            std::string function = req.get_param_value("fn");
            std::string idStr = req.get_param_value("id");
            
            // Parse and validate ID
            int id;
            try {
                id = std::stoi(idStr);
            } catch (const std::exception&) {
                res.status = 400;
                res.set_content("Invalid id parameter: must be a number", "text/plain");
                return;
            }
            
            if (id < 0 || id >= static_cast<int>(g_commands.size())) {
                res.status = 404;
                res.set_content("Process ID out of range", "text/plain");
                return;
            }
            
            // Execute requested function
            if (function == "start") {
                if (g_commands[id].Status == RUNNING) {
                    res.set_content("Process is already running (PID: " + 
                                  std::to_string(g_commands[id].Pid) + ")", "text/plain");
                } else {
                    pid_t pid = g_processRunner->start(id);
                    if (pid > 0) {
                        res.set_content("Process started successfully (PID: " + 
                                      std::to_string(pid) + ")", "text/plain");
                    } else {
                        res.status = 500;
                        res.set_content("Failed to start process", "text/plain");
                    }
                }
                
            } else if (function == "kill" || function == "end" || function == "stop") {
                bool force = (function == "kill");
                if (g_processRunner->kill(id, force)) {
                    res.set_content("Process terminated successfully", "text/plain");
                } else {
                    res.status = 500;
                    res.set_content("Failed to terminate process", "text/plain");
                }
                
            } else if (function == "status") {
                const auto& cmd = g_commands[id];
                std::string statusResponse = "{\n";
                statusResponse += "  \"id\": " + std::to_string(id) + ",\n";
                statusResponse += "  \"desc\": \"" + escapeJsonString(cmd.Desc) + "\",\n";
                statusResponse += "  \"status\": \"" + (cmd.Status == RUNNING ? "RUNNING" : "DEAD") + "\",\n";
                statusResponse += "  \"pid\": " + std::to_string(cmd.Pid) + "\n";
                statusResponse += "}";
                res.set_content(statusResponse, "application/json");
                
            } else {
                res.status = 400;
                res.set_content("Unknown function: " + function + 
                              ". Valid functions: start, stop, kill, end, status", "text/plain");
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error in /process/control: " << e.what() << std::endl;
            res.status = 500;
            res.set_content("Internal server error", "text/plain");
        }
    });
    
    // Health check endpoint
    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    });
    
    // Start server
    std::cout << "🎯 Server endpoints:" << std::endl;
    std::cout << "   GET  /process/list    - List all processes" << std::endl;
    std::cout << "   POST /process/control - Control processes" << std::endl;
    std::cout << "   GET  /health          - Health check" << std::endl;
    std::cout << std::endl;
    
    if (!server.listen("0.0.0.0", g_port)) {
        std::cerr << "❌ Failed to start server on port " << g_port << std::endl;
        std::cerr << "   Port may be in use or insufficient permissions" << std::endl;
    }
}

/**
 * @brief Print usage information
 */
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help           Show this help message" << std::endl;
    std::cout << "  -c, --config FILE    Configuration file path" << std::endl;
    std::cout << "  -p, --port PORT      HTTP server port (default: " << DEFAULT_PORT << ")" << std::endl;
    std::cout << std::endl;
    std::cout << "Default config locations:" << std::endl;
    std::cout << "  1. " << DEFAULT_CONFIG_PATH << std::endl;
    std::cout << "  2. " << FALLBACK_CONFIG_PATH << std::endl;
}