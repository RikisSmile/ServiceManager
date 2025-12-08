#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

// --- Platform-specific includes ---

    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    using SOCKET_TYPE = int;
    #define CLOSE_SOCKET(s) close(s)
    #define INIT_NET() true

// ----------------------------------

const int PORT = 6756;
const int BACKLOG = 10;
const int BUFFER_SIZE = 1024;
const std::string HTML_FILE = "monitor.html";


// --- Main Server Logic ---
void start_server() {
    if (!INIT_NET()) return;

    // 1. Create socket file descriptor
    SOCKET_TYPE server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Error: Cannot create socket." << std::endl;
        return;
    }

    // Optional: Allow reuse of the address/port immediately after server shutdown
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (const char*)&opt, sizeof(opt))) {
        std::cerr << "Error: setsockopt failed." << std::endl;
        CLOSE_SOCKET(server_fd);
        return;
    }

    // 2. Bind the socket to the port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "Error: Bind failed. Port " << PORT << " may be in use." << std::endl;
        CLOSE_SOCKET(server_fd);
        return;
    }

    // 3. Listen for incoming connections
    if (listen(server_fd, BACKLOG) < 0) {
        std::cerr << "Error: Listen failed." << std::endl;
        CLOSE_SOCKET(server_fd);
        return;
    }

    std::cout << "🚀 C++ HTTP Server started successfully." << std::endl;
    std::cout << "🌐 Listening on port " << PORT << ". Access at http://localhost:" << PORT << std::endl;
    std::cout << "---" << std::endl;

    // 4. Accept and handle connections in a loop
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        
        SOCKET_TYPE client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_socket < 0) {
            std::cerr << "Error: Accept failed." << std::endl;
            continue;
        }

        // Read the incoming request (max BUFFER_SIZE bytes)
        char buffer[BUFFER_SIZE] = {0};
        recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        // std::cout << "Received request:\n" << buffer << std::endl; // Uncomment to see request headers

        // Check if the request is for the root path "/"
        if (strncmp(buffer, "GET / ", 6) == 0 || strncmp(buffer, "GET /HTTP", 9) == 0) {
            
            // Read the HTML file content
            std::ifstream file(HTML_FILE);
            std::stringstream html_content;
            if (file.is_open()) {
                html_content << file.rdbuf();
                file.close();
            } else {
                // If file not found, send 404
                std::string not_found_response = 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 10\r\n\r\n"
                    "404 Error.";
                send(client_socket, not_found_response.c_str(), not_found_response.length(), 0);
            }

            // Construct the HTTP response
            std::string html_string = html_content.str();
            std::string http_response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: " + std::to_string(html_string.length()) + "\r\n"
                "Connection: close\r\n\r\n" + html_string;
            
            // Send the response
            send(client_socket, http_response.c_str(), http_response.length(), 0);
        } else {
             // Simple fallback for any other request (e.g., /favicon.ico)
            std::string fallback_response = 
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 10\r\n\r\n"
                "404 Error.";
            send(client_socket, fallback_response.c_str(), fallback_response.length(), 0);
        }

        // Close the client connection
        CLOSE_SOCKET(client_socket);
    }

    // Cleanup
    CLOSE_SOCKET(server_fd);
    #ifdef _WIN32
        WSACleanup();
    #endif
}

int main() {
    start_server();
    return 0;
}