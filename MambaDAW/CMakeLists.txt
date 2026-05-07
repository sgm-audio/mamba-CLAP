cmake_minimum_required(VERSION 3.14)
project(MambaDAW LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Handle ABI compatibility for LibTorch
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0")
endif()

# Find dependencies
find_package(Torch REQUIRED)
find_package(CLAP REQUIRED)

# Include directories
include_directories(${TORCH_INCLUDE_DIRS})
include_directories(${CLAP_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
)

# Create shared library
add_library(MambaDAW MODULE ${SOURCES})
set_target_properties(MambaDAW PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
)

# Link libraries
target_link_libraries(MambaDAW PRIVATE ${TORCH_LIBRARIES} ${CLAP_LIBRARIES})

# Set output directory
set_target_properties(MambaDAW PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release
)

# Install
install(TARGETS MambaDAW DESTINATION clap)