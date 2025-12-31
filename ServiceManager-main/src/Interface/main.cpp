
/**
 * @file main.cpp
 * @brief Process Management CLI Client
 * @version 1.0
 * @date 2025-01-01
 * 
 * Command-line interface for interacting with the Process Management Server.
 * Provides interactive commands to list, start, and stop processes remotely.
 */

#include "httplib.h"
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>

// Configuration constants
constexpr const char* DEFAULT_SERVER_HOST = "localhost";
constexpr int DEFAULT_SERVER_PORT = 6755;

/**
 * @brief Structure to hold process information from server
 */
struct ProcessInfo {
    int id;
    std::string description;
    std::string status;
    std::string mode;
    int pid;
    
    ProcessInfo() : id(-1), pid(-1) {}
};

/**
 * @brief Parse JSON response from server into ProcessInfo vector
 * @param jsonBody JSON response body from server
 * @return Vector of ProcessInfo structures
 */
std::vector<ProcessInfo> parseProcessList(const std::string& jsonBody) {
    std::vector<ProcessInfo> processes;
    
    // Simple JSON parser for the expected format
    // This is a basic implementation - for production use a proper JSON library
    size_t pos = 0;
    int currentId = 0;
    
    while (true) {
        // Find next object start
        pos = jsonBody.find('{', pos);
        if (pos == std::string::npos) break;
        
        ProcessInfo process;
        process.id = currentId++;
        
        // Helper lambda to extract field values
        auto extractField = [&](const std::string& fieldName) -> std::string {
            std::string searchPattern = "\"" + fieldName + "\"";
            auto fieldPos = jsonBody.find(searchPattern, pos);
            if (fieldPos == std::string::npos) return "";
            
            auto colonPos = jsonBody.find(':', fieldPos);
            if (colonPos == std::string::npos) return "";
            
            auto valueStart = jsonBody.find_first_not_of(" \t\n\r", colonPos + 1);
            if (valueStart == std::string::npos) return "";
            
            if (jsonBody[valueStart] == '"') {
                // String value
                auto valueEnd = jsonBody.find('"', valueStart + 1);
                if (valueEnd == std::string::npos) return "";
                return jsonBody.substr(valueStart + 1, valueEnd - valueStart - 1);
            } else {
                // Numeric value
                auto valueEnd = jsonBody.find_first_of(",}\n\r", valueStart);
                if (valueEnd == std::string::npos) return "";
                return jsonBody.substr(valueStart, valueEnd - valueStart);
            }
        };
        
        process.description = extractField("desc");
        process.status = extractField("status");
        process.mode = extractField("mode");
        
        std::string pidStr = extractField("pid");
        if (!pidStr.empty()) {
            try {
                process.pid = std::stoi(pidStr);
            } catch (const std::exception&) {
                process.pid = -1;
            }
        }
        
        processes.push_back(process);
        pos += 1;
    }
    
    return processes;
}

/**
 * @brief Display process list in a formatted table
 * @param processes Vector of ProcessInfo to display
 */
void displayProcessList(const std::vector<ProcessInfo>& processes) {
    if (processes.empty()) {
        std::cout << "No processes found." << std::endl;
        return;
    }
    
    // Calculate column widths
    size_t maxIdWidth = 2;
    size_t maxDescWidth = 11; // "Description"
    size_t maxStatusWidth = 6; // "Status"
    size_t maxModeWidth = 4;   // "Mode"
    size_t maxPidWidth = 3;    // "PID"
    
    for (const auto& proc : processes) {
        maxIdWidth = std::max(maxIdWidth, std::to_string(proc.id).length());
        maxDescWidth = std::max(maxDescWidth, proc.description.length());
        maxStatusWidth = std::max(maxStatusWidth, proc.status.length());
        maxModeWidth = std::max(maxModeWidth, proc.mode.length());
        if (proc.pid > 0) {
            maxPidWidth = std::max(maxPidWidth, std::to_string(proc.pid).length());
        }
    }
    
    // Print header
    std::cout << std::string(maxIdWidth + maxDescWidth + maxStatusWidth + maxModeWidth + maxPidWidth + 16, '=') << std::endl;
    std::cout << "| " << std::left << std::setw(maxIdWidth) << "ID"
              << " | " << std::setw(maxDescWidth) << "Description"
              << " | " << std::setw(maxStatusWidth) << "Status"
              << " | " << std::setw(maxModeWidth) << "Mode"
              << " | " << std::setw(maxPidWidth) << "PID" << " |" << std::endl;
    std::cout << std::string(maxIdWidth + maxDescWidth + maxStatusWidth + maxModeWidth + maxPidWidth + 16, '=') << std::endl;
    
    // Print processes
    for (const auto& proc : processes) {
        std::string pidStr = (proc.pid > 0) ? std::to_string(proc.pid) : "-";
        std::string statusDisplay = proc.status;
        
        // Add color coding for status (if terminal supports it)
        if (proc.status == "RUNNING") {
            statusDisplay = "\033[32m" + proc.status + "\033[0m"; // Green
        } else if (proc.status == "DEAD") {
            statusDisplay = "\033[31m" + proc.status + "\033[0m"; // Red
        }
        
        std::cout << "| " << std::left << std::setw(maxIdWidth) << proc.id
                  << " | " << std::setw(maxDescWidth) << proc.description
                  << " | " << std::setw(maxStatusWidth + 9) << statusDisplay // +9 for ANSI codes
                  << " | " << std::setw(maxModeWidth) << proc.mode
                  << " | " << std::setw(maxPidWidth) << pidStr << " |" << std::endl;
    }
    std::cout << std::string(maxIdWidth + maxDescWidth + maxStatusWidth + maxModeWidth + maxPidWidth + 16, '=') << std::endl;
}

/**
 * @brief Fetch and display process list from server
 * @param client HTTP client instance
 * @return true on success, false on failure
 */
bool fetchAndDisplayProcessList(httplib::Client& client) {
    std::cout << "📋 Fetching process list..." << std::endl;
    
    auto response = client.Get("/process/list");
    if (!response) {
        std::cerr << "❌ Failed to connect to server" << std::endl;
        return false;
    }
    
    if (response->status != 200) {
        std::cerr << "❌ Server returned error: " << response->status << std::endl;
        if (!response->body.empty()) {
            std::cerr << "   " << response->body << std::endl;
        }
        return false;
    }
    
    auto processes = parseProcessList(response->body);
    displayProcessList(processes);
    
    return true;
}

/**
 * @brief Send control command to server
 * @param client HTTP client instance
 * @param function Function to execute (start/stop/kill/status)
 * @param processId Process ID to control
 * @return true on success, false on failure
 */
bool sendControlCommand(httplib::Client& client, const std::string& function, int processId) {
    httplib::Params params;
    params.emplace("fn", function);
    params.emplace("id", std::to_string(processId));
    
    std::cout << "🔧 Sending command: " << function << " (ID: " << processId << ")" << std::endl;
    
    auto response = client.Post("/process/control", params);
    if (!response) {
        std::cerr << "❌ Failed to connect to server" << std::endl;
        return false;
    }
    
    if (response->status == 200) {
        std::cout << "✅ " << response->body << std::endl;
        return true;
    } else {
        std::cerr << "❌ Command failed (" << response->status << "): " << response->body << std::endl;
        return false;
    }
}

/**
 * @brief Print usage information
 */
void printUsage() {
    std::cout << std::endl;
    std::cout << "Available commands:" << std::endl;
    std::cout << "  l, list          - List all processes" << std::endl;
    std::cout << "  s <id>           - Start process with given ID" << std::endl;
    std::cout << "  k <id>           - Kill process with given ID (force)" << std::endl;
    std::cout << "  stop <id>        - Stop process with given ID (graceful)" << std::endl;
    std::cout << "  status <id>      - Get status of process with given ID" << std::endl;
    std::cout << "  h, help          - Show this help message" << std::endl;
    std::cout << "  q, quit, exit    - Exit the program" << std::endl;
    std::cout << std::endl;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    std::string serverHost = DEFAULT_SERVER_HOST;
    int serverPort = DEFAULT_SERVER_PORT;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
            std::cout << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -h, --help           Show this help message" << std::endl;
            std::cout << "  --host HOST          Server hostname (default: " << DEFAULT_SERVER_HOST << ")" << std::endl;
            std::cout << "  --port PORT          Server port (default: " << DEFAULT_SERVER_PORT << ")" << std::endl;
            return 0;
        } else if (arg == "--host") {
            if (i + 1 < argc) {
                serverHost = argv[++i];
            } else {
                std::cerr << "Error: --host requires a hostname" << std::endl;
                return 1;
            }
        } else if (arg == "--port") {
            if (i + 1 < argc) {
                try {
                    serverPort = std::stoi(argv[++i]);
                    if (serverPort <= 0 || serverPort > 65535) {
                        throw std::out_of_range("Port out of range");
                    }
                } catch (const std::exception&) {
                    std::cerr << "Error: Invalid port number" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --port requires a port number" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument " << arg << std::endl;
            return 1;
        }
    }
    
    std::cout << "🚀 Process Management Client v1.0" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "🌐 Connecting to: " << serverHost << ":" << serverPort << std::endl;
    
    // Create HTTP client
    httplib::Client client(serverHost, serverPort);
    client.set_connection_timeout(5, 0); // 5 seconds
    client.set_read_timeout(10, 0);      // 10 seconds
    
    // Test connection
    auto healthResponse = client.Get("/health");
    if (!healthResponse || healthResponse->status != 200) {
        std::cerr << "❌ Cannot connect to server at " << serverHost << ":" << serverPort << std::endl;
        std::cerr << "   Make sure the server is running and accessible" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Connected to server successfully" << std::endl;
    
    // Initial process list
    fetchAndDisplayProcessList(client);
    
    printUsage();
    
    // Interactive command loop
    std::string input;
    std::cout << "> ";
    
    while (std::getline(std::cin, input)) {
        // Trim whitespace
        input.erase(0, input.find_first_not_of(" \t"));
        input.erase(input.find_last_not_of(" \t") + 1);
        
        if (input.empty()) {
            std::cout << "> ";
            continue;
        }
        
        // Parse command
        std::istringstream iss(input);
        std::string command;
        iss >> command;
        
        // Convert to lowercase for case-insensitive commands
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);
        
        if (command == "q" || command == "quit" || command == "exit") {
            std::cout << "👋 Goodbye!" << std::endl;
            break;
            
        } else if (command == "l" || command == "list") {
            fetchAndDisplayProcessList(client);
            
        } else if (command == "h" || command == "help") {
            printUsage();
            
        } else if (command == "s" || command == "start") {
            int id;
            if (iss >> id) {
                sendControlCommand(client, "start", id);
            } else {
                std::cerr << "❌ Usage: start <process_id>" << std::endl;
            }
            
        } else if (command == "k" || command == "kill") {
            int id;
            if (iss >> id) {
                sendControlCommand(client, "kill", id);
            } else {
                std::cerr << "❌ Usage: kill <process_id>" << std::endl;
            }
            
        } else if (command == "stop") {
            int id;
            if (iss >> id) {
                sendControlCommand(client, "stop", id);
            } else {
                std::cerr << "❌ Usage: stop <process_id>" << std::endl;
            }
            
        } else if (command == "status") {
            int id;
            if (iss >> id) {
                sendControlCommand(client, "status", id);
            } else {
                std::cerr << "❌ Usage: status <process_id>" << std::endl;
            }
            
        } else {
            std::cerr << "❌ Unknown command: " << command << std::endl;
            std::cerr << "   Type 'help' for available commands" << std::endl;
        }
        
        std::cout << "> ";
    }
    
    return 0;
}
