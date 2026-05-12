cmake_minimum_required(VERSION 3.14)
project(MambaClapPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use local CLAP SDK clone instead of FetchContent
add_library(clap INTERFACE)
target_include_directories(clap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/clap-sdk/include)

# Find Torch
find_package(Torch REQUIRED)

# Find FFTW3
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3F REQUIRED fftw3f)

set(SOURCES src/clap-plugin.cpp)
set(HEADERS include/clap-plugin.hpp)

add_library(${PROJECT_NAME} MODULE ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TORCH_INCLUDE_DIRS}
    ${FFTW3F_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${TORCH_LIBRARIES}
    clap
    dl
    pthread
    ${FFTW3F_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Release"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    "-DTORCH_API_INCLUDE_EXTENSION_H"
    "-DTORCH_EXTENSION_NAME=MambaClapPlugin"
    "-D_GLIBCXX_USE_CXX11_ABI=1"
)
