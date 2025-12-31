# Process Management System

A distributed process management system for remotely monitoring and controlling system processes and Docker containers. The system consists of multiple components that work together to provide comprehensive process lifecycle management.

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                Process Management System                     │
└─────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│                ServiceMN (Port 6755)                         │
│  - HTTP Server for process management                        │
│  - REST API endpoints                                        │
│  - Process lifecycle management                              │
│  - Docker container support                                  │
└──────────────────────────────────────────────────────────────┘
         ↑                    ↑                    ↑
         │                    │                    │
    HTTP Requests        HTTP Requests      HTTP Requests
         │                    │                    │
    ┌────┴────┐          ┌────┴────┐          ┌────┴────┐
    │Interface │          │ Website  │         │ Arduino  │
    │(CLI)     │          │(Web UI)  │         │(ESP32)   │
    │Port 6755 │          │Port 6756 │         │WiFi      │
    └──────────┘          └──────────┘         └──────────┘
```

## 🚀 Components

### 1. ServiceMN (Server)
- **Purpose**: Core process management server
- **Port**: 6755 (configurable)
- **Features**:
  - REST API for process control
  - Support for regular commands and Docker containers
  - Process lifecycle management (start/stop/kill/status)
  - Configuration file-based setup
  - CORS support for web interface

### 2. Interface (CLI Client)
- **Purpose**: Command-line interface for process management
- **Features**:
  - Interactive process listing
  - Process control commands
  - Formatted table display with color coding
  - Configurable server connection
  - Keyboard shortcuts

### 3. Website (Web Dashboard)
- **Purpose**: Web-based monitoring dashboard
- **Port**: 6756 (configurable)
- **Features**:
  - Real-time process monitoring
  - Interactive web interface
  - Configurable server connection
  - Auto-refresh functionality
  - Responsive design

### 4. Arduino Controller (Optional)
- **Purpose**: Hardware controller with OLED display
- **Features**:
  - WiFi connectivity
  - OLED display for process status
  - Physical buttons for control
  - Real-time monitoring

## 🔧 Building

### Prerequisites
- C++17 compatible compiler (GCC 7+ or Clang 5+)
- CMake 3.16+ (optional, for advanced build)
- pthread library
- For Arduino: Arduino IDE or PlatformIO

### Quick Build
```bash
# Make build script executable
chmod +x build.sh

# Build all components
./build.sh
```

### CMake Build (Recommended)
```bash
mkdir cmake-build && cd cmake-build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Manual Build
```bash
# Create build directory
mkdir -p build

# Build Server
cd src/Server
g++ -std=c++17 -O3 -Wall -pthread -o ../../build/ServiceMN main.cpp ProcessRunner.cpp

# Build Interface
cd ../Interface
g++ -std=c++17 -O3 -Wall -pthread -o ../../build/interface main.cpp

# Build Website
cd ../website
g++ -std=c++17 -O3 -Wall -pthread -o ../../build/website main.cpp

# Copy HTML file
cp monitor.html ../../build/
```

## ⚙️ Configuration

### Server Configuration File
Create `build/config/cmds.conf` with the following format:

```
<number_of_commands>
<description_1>
<mode_1>
<command_1>
<working_directory_1>
<description_2>
<mode_2>
<command_2>
<working_directory_2>
...
```

**Modes:**
- `C`: Regular system command
- `D`: Docker container

**Example:**
```
3
Web Server
C
python3 -m http.server 8080
/tmp
Database Service
D
postgres:13
/var/lib/postgresql
Log Monitor
C
tail -f /var/log/syslog
/var/log
```

## 🚀 Usage

### 1. Start the Server
```bash
# Basic usage
./build/ServiceMN

# With custom configuration
./build/ServiceMN --config /path/to/cmds.conf --port 8080

# Show help
./build/ServiceMN --help
```

### 2. Use CLI Interface
```bash
# Connect to server
./build/interface

# Connect to custom server
./build/interface --host 192.168.1.100 --port 8080
```

**CLI Commands:**
- `l, list` - List all processes
- `s <id>` - Start process
- `k <id>` - Kill process (force)
- `stop <id>` - Stop process (graceful)
- `status <id>` - Get process status
- `h, help` - Show help
- `q, quit` - Exit

### 3. Web Dashboard
```bash
# Start web server
./build/website

# Custom port and HTML file
./build/website --port 8080 --file custom.html
```

Access dashboard at: `http://localhost:6756`

### 4. Arduino Controller
1. Open `src/Manager.ino` in Arduino IDE
2. Update WiFi credentials and server IP
3. Upload to ESP32 with OLED display

## 📡 API Endpoints

### GET /process/list
Returns JSON array of all processes:
```json
[
  {
    "id": 0,
    "desc": "Web Server",
    "status": "RUNNING",
    "mode": "C",
    "pid": 1234
  }
]
```

### POST /process/control
Control processes with form parameters:
- `fn`: Function (start/stop/kill/end/status)
- `id`: Process ID

### GET /health
Health check endpoint returning "OK"

## 🔒 Security Considerations

- Server binds to all interfaces (0.0.0.0) by default
- No authentication implemented - use firewall rules
- CORS enabled for web interface
- Consider running behind reverse proxy for production

## 🐛 Troubleshooting

### Common Issues

1. **Port already in use**
   ```bash
   # Check what's using the port
   netstat -tulpn | grep :6755
   # Use different port
   ./build/ServiceMN --port 8080
   ```

2. **Configuration file not found**
   ```bash
   # Create config directory
   mkdir -p build/config
   # Copy example config
   cp config/cmds.conf.example build/config/cmds.conf
   ```

3. **Permission denied for process control**
   - Ensure user has permissions to execute commands
   - For Docker: add user to docker group

4. **Web interface can't connect**
   - Check server is running and accessible
   - Verify firewall settings
   - Update server host/port in web interface

## 📝 Development

### Code Structure
```
src/
├── Server/           # Process management server
│   ├── main.cpp      # HTTP server and API
│   ├── ProcessRunner.cpp/.hpp  # Process lifecycle management
│   └── command.hpp   # Command structure definition
├── Interface/        # CLI client
│   └── main.cpp      # Interactive command-line interface
├── website/          # Web dashboard server
│   ├── main.cpp      # HTTP server for dashboard
│   └── monitor.html  # Web interface
└── Manager.ino       # Arduino controller
```

### Adding Features
1. **New API endpoints**: Modify `src/Server/main.cpp`
2. **Process management**: Extend `ProcessRunner` class
3. **Web interface**: Update `monitor.html`
4. **CLI commands**: Modify `src/Interface/main.cpp`

## 📄 License

This project is open source. See individual files for specific licensing information.

## 🤝 Contributing

1. Fork the repository
2. Create feature branch
3. Make changes with proper documentation
4. Test all components
5. Submit pull request

## 📞 Support

For issues and questions:
1. Check troubleshooting section
2. Review configuration examples
3. Check server logs for error messages
4. Ensure all components are built correctly