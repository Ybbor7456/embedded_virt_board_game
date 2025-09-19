cmake_minimum_required(VERSION 3.15)

project(cmdviewer LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(cmdviewer
    src/main.cpp
    src/audio/audio.cpp
)


target_include_directories(cmdviewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)


find_package(PkgConfig REQUIRED)
pkg_check_modules(RAYLIB REQUIRED raylib)
target_include_directories(cmdviewer PRIVATE ${RAYLIB_INCLUDE_DIRS})
target_link_libraries(cmdviewer PRIVATE ${RAYLIB_LIBRARIES})


if(UNIX)
  set_target_properties(cmdviewer PROPERTIES
    BUILD_RPATH  "/usr/local/lib"
    INSTALL_RPATH "/usr/local/lib")
endif()
