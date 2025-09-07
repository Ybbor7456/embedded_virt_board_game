cmake_minimum_required(VERSION 3.15)
project(cmdviewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(cmdviewer src/main.cpp)

# Use pkg-config for raylib (works on your box)
find_package(PkgConfig REQUIRED)
pkg_check_modules(RAYLIB REQUIRED raylib)

target_include_directories(cmdviewer PRIVATE ${RAYLIB_INCLUDE_DIRS})
target_link_libraries(cmdviewer PRIVATE ${RAYLIB_LIBRARIES})

# Help the runtime linker find libraylib.so from /usr/local/lib
if(UNIX)
  set_target_properties(cmdviewer PROPERTIES
    BUILD_RPATH "/usr/local/lib"
    INSTALL_RPATH "/usr/local/lib")
endif()

