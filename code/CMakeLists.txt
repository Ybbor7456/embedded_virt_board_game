cmake_minimum_required(VERSION 3.15)
project(cmdviewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fetch raylib locally (no admin install needed) ---
include(FetchContent)

# Download a specific release ZIP so we don't need git
FetchContent_Declare(
  raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.zip
)

# Build only the library (faster)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

# --- Your app ---
add_executable(cmdviewer src/main.cpp)
target_link_libraries(cmdviewer PRIVATE raylib)

# Nicer warnings
if(MSVC)
  target_compile_options(cmdviewer PRIVATE /W4)
else()
  target_compile_options(cmdviewer PRIVATE -Wall -Wextra -Wpedantic)
endif()
