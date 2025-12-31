/**
 * @file main.cpp
 * @brief Web Dashboard Server for Process Management System
 * @version 1.0
 * @date 2025-01-01
 * 
 * Serves a web-based dashboard for monitoring and controlling processes.
 * Provides a simple HTTP server that serves the monitor.html file.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <filesystem>
#include <thread>
#include <chrono>

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SOCKET_TYPE = SOCKET;
    #define CLOSE_SOCKET(s) closesocket(s)
    #define INIT_NET() initializeWinsock()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    using SOCKET_TYPE = int;
    #define CLOSE_SOCKET(s) close(s)
    #define INIT_NET() true
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

// Configuration constants
constexpr int DEFAULT_PORT = 6756;
constexpr int BACKLOG = 10;
constexpr int BUFFER_SIZE = 4096;
constexpr const char* HTML_FILE = "monitor.html";

// Global variables
int g_port = DEFAULT_PORT;
std::string g_htmlFile = HTML_FILE;

#ifdef _WIN32
/**
 * @brief Initialize Winsock on Windows
 */
bool initializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
    return true;
}
#endif

/**
 * @brief Read file content into string
 * @param filename Path to file to read
 * @return File content as string, empty if file not found
 */
std::string readFileContent(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

/**
 * @brief Get MIME type based on file extension
 * @param filename File name or path
 * @return MIME type string
 */
std::string getMimeType(const std::string& filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "text/plain";
    }
    
    std::string extension = filename.substr(dotPos + 1);
    
    // Convert to lowercase
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == "html" || extension == "htm") return "text/html";
    if (extension == "css") return "text/css";
    if (extension == "js") return "application/javascript";
    if (extension == "json") return "application/json";
    if (extension == "png") return "image/png";
    if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
    if (extension == "gif") return "image/gif";
    if (extension == "ico") return "image/x-icon";
    
    return "text/plain";
}

/**
 * @brief Create HTTP response
 * @param statusCode HTTP status code
 * @param statusText HTTP status text
 * @param contentType Content-Type header value
 * @param body Response body
 * @return Complete HTTP response string
 */
std::string createHttpResponse(int statusCode, const std::string& statusText,
                              const std::string& contentType, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "Cache-Control: no-cache\r\n";
    response << "\r\n";
    response << body;
    return response.str();
}

/**
 * @brief Parse HTTP request to extract method and path
 * @param request Raw HTTP request string
 * @param method Output parameter for HTTP method
 * @param path Output parameter for request path
 * @return true if parsing successful, false otherwise
 */
bool parseHttpRequest(const std::string& request, std::string& method, std::string& path) {
    std::istringstream iss(request);
    std::string httpVersion;
    
    if (!(iss >> method >> path >> httpVersion)) {
        return false;
    }
    
    // Remove query parameters from path
    size_t queryPos = path.find('?');
    if (queryPos != std::string::npos) {
        path = path.substr(0, queryPos);
    }
    
    return true;
}

/**
 * @brief Handle client connection
 * @param clientSocket Client socket descriptor
 */
void handleClient(SOCKET_TYPE clientSocket) {
    char buffer[BUFFER_SIZE] = {0};
    
    // Read request
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesReceived <= 0) {
        CLOSE_SOCKET(clientSocket);
        return;
    }
    
    std::string request(buffer, bytesReceived);
    std::string method, path;
    
    if (!parseHttpRequest(request, method, path)) {
        std::string errorResponse = createHttpResponse(400, "Bad Request", "text/plain", "Bad Request");
        send(clientSocket, errorResponse.c_str(), errorResponse.length(), 0);
        CLOSE_SOCKET(clientSocket);
        return;
    }
    
    std::cout << "📥 " << method << " " << path << std::endl;
    
    std::string response;
    
    if (method == "GET") {
        if (path == "/" || path == "/index.html") {
            // Serve main HTML file
            std::string htmlContent = readFileContent(g_htmlFile);
            if (!htmlContent.empty()) {
                response = createHttpResponse(200, "OK", "text/html", htmlContent);
            } else {
                std::string errorBody = "<!DOCTYPE html><html><head><title>Error</title></head><body>"
                                      "<h1>404 - File Not Found</h1>"
                                      "<p>The file '" + g_htmlFile + "' was not found.</p>"
                                      "<p>Make sure the HTML file is in the same directory as the server executable.</p>"
                                      "</body></html>";
                response = createHttpResponse(404, "Not Found", "text/html", errorBody);
            }
        } else if (path == "/health") {
            // Health check endpoint
            response = createHttpResponse(200, "OK", "text/plain", "OK");
        } else {
            // Try to serve static file
            std::string filename = path.substr(1); // Remove leading slash
            if (filename.empty()) {
                filename = g_htmlFile;
            }
            
            std::string fileContent = readFileContent(filename);
            if (!fileContent.empty()) {
                std::string mimeType = getMimeType(filename);
                response = createHttpResponse(200, "OK", mimeType, fileContent);
            } else {
                std::string errorBody = "404 - File Not Found: " + path;
                response = createHttpResponse(404, "Not Found", "text/plain", errorBody);
            }
        }
    } else {
        // Method not allowed
        std::string errorBody = "405 - Method Not Allowed: " + method;
        response = createHttpResponse(405, "Method Not Allowed", "text/plain", errorBody);
    }
    
    // Send response
    send(clientSocket, response.c_str(), response.length(), 0);
    CLOSE_SOCKET(clientSocket);
}

/**
 * @brief Start HTTP server
 */
void startServer() {
    if (!INIT_NET()) {
        std::cerr << "❌ Failed to initialize network" << std::endl;
        return;
    }
    
    // Create socket
    SOCKET_TYPE serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "❌ Failed to create socket" << std::endl;
        #ifdef _WIN32
            WSACleanup();
        #endif
        return;
    }
    
    // Set socket options
    #ifndef _WIN32
        int opt = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "⚠️  Warning: Failed to set SO_REUSEADDR" << std::endl;
        }
    #endif
    
    // Bind socket
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(g_port);
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "❌ Failed to bind socket to port " << g_port << std::endl;
        std::cerr << "   Port may be in use or insufficient permissions" << std::endl;
        CLOSE_SOCKET(serverSocket);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return;
    }
    
    // Listen for connections
    if (listen(serverSocket, BACKLOG) == SOCKET_ERROR) {
        std::cerr << "❌ Failed to listen on socket" << std::endl;
        CLOSE_SOCKET(serverSocket);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return;
    }
    
    std::cout << "🚀 Web Dashboard Server v1.0" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << "🌐 Server listening on port " << g_port << std::endl;
    std::cout << "📁 Serving HTML file: " << g_htmlFile << std::endl;
    std::cout << "🔗 Access at: http://localhost:" << g_port << std::endl;
    std::cout << "📊 Dashboard: http://localhost:" << g_port << "/" << std::endl;
    std::cout << "❤️  Health check: http://localhost:" << g_port << "/health" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    std::cout << "================================" << std::endl;
    
    // Accept connections
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        SOCKET_TYPE clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "❌ Failed to accept connection" << std::endl;
            continue;
        }
        
        // Handle client in separate thread for better performance
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }
    
    // Cleanup
    CLOSE_SOCKET(serverSocket);
    #ifdef _WIN32
        WSACleanup();
    #endif
}

/**
 * @brief Print usage information
 */
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help           Show this help message" << std::endl;
    std::cout << "  -p, --port PORT      HTTP server port (default: " << DEFAULT_PORT << ")" << std::endl;
    std::cout << "  -f, --file FILE      HTML file to serve (default: " << HTML_FILE << ")" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << "                    # Start server on default port" << std::endl;
    std::cout << "  " << programName << " --port 8080        # Start server on port 8080" << std::endl;
    std::cout << "  " << programName << " --file custom.html # Serve custom HTML file" << std::endl;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--port" || arg == "-p") {
            if (i + 1 < argc) {
                try {
                    g_port = std::stoi(argv[++i]);
                    if (g_port <= 0 || g_port > 65535) {
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
        } else if (arg == "--file" || arg == "-f") {
            if (i + 1 < argc) {
                g_htmlFile = argv[++i];
            } else {
                std::cerr << "Error: --file requires a file path" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Check if HTML file exists
    if (!std::filesystem::exists(g_htmlFile)) {
        std::cerr << "⚠️  Warning: HTML file '" << g_htmlFile << "' not found" << std::endl;
        std::cerr << "   Server will return 404 for the main page" << std::endl;
    }
    
    // Start server
    startServer();
    
    return 0;
}