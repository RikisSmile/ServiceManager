#!/bin/bash

# Process Manager Build Script
# Builds all components and places executables in build/ directory

set -e  # Exit on any error

echo "🔨 Building Process Manager System..."

# Create build directory
mkdir -p build

# Check if CMake is available
if command -v cmake &> /dev/null; then
    echo "📦 Using CMake build system..."
    
    # Create build directory for CMake
    mkdir -p cmake-build
    cd cmake-build
    
    # Configure and build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    
    # Copy executables to main build directory
    cp build/* ../build/ 2>/dev/null || true
    
    cd ..
    echo "✅ CMake build completed"
else
    echo "📦 Using manual build system..."
    
    # Build Server
    echo "🔧 Building ServiceMN (Server)..."
    cd src/Server
    g++ -std=c++17 -O3 -Wall -pthread -o ../../build/ServiceMN main.cpp ProcessRunner.cpp
    cd ../..
    
    # Build Interface
    echo "🔧 Building interface (CLI Client)..."
    cd src/Interface
    g++ -std=c++17 -O3 -Wall -pthread -o ../../build/interface main.cpp
    cd ../..
    
    # Build Website
    echo "🔧 Building website (Web Server)..."
    cd src/website
    g++ -std=c++17 -O3 -Wall -pthread -o ../../build/website main.cpp
    cd ../..
    
    # Copy HTML file
    cp src/website/monitor.html build/
    
    echo "✅ Manual build completed"
fi

# Create config directory
mkdir -p build/config

# Set executable permissions
chmod +x build/ServiceMN build/interface build/website

echo ""
echo "🎉 Build completed successfully!"
echo "📁 Executables are in: ./build/"
echo "   - ServiceMN (Process Management Server)"
echo "   - interface (CLI Client)"  
echo "   - website (Web Dashboard Server)"
echo ""
echo "📋 Next steps:"
echo "   1. Create config file: build/config/cmds.conf"
echo "   2. Run server: ./build/ServiceMN"
echo "   3. Run client: ./build/interface"
echo "   4. Run web dashboard: ./build/website"
