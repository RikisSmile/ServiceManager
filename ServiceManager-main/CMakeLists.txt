cmake_minimum_required(VERSION 3.16)
project(ProcessManager VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -Wall")

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(src/Server)
include_directories(src/Interface)
include_directories(src/website)

# Server executable
add_executable(ServiceMN
    src/Server/main.cpp
    src/Server/ProcessRunner.cpp
)
target_link_libraries(ServiceMN Threads::Threads)

# Interface executable  
add_executable(interface
    src/Interface/main.cpp
)
target_link_libraries(interface Threads::Threads)

# Website executable
add_executable(website
    src/website/main.cpp
)
target_link_libraries(website Threads::Threads)

# Install targets
install(TARGETS ServiceMN interface website
    RUNTIME DESTINATION bin
)

# Copy HTML file to build directory
configure_file(src/website/monitor.html ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/monitor.html COPYONLY)

# Create config directory structure
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config)

# Print build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")