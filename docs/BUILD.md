## Prereqs
- CMake (4.x) on PATH
- MinGW-w64 (g++, mingw32-make) on PATH
- Raylib (linked via CMake find_package or vendored source)

## Layout assumptions
- Scripts: `logs/*.cmdlog`
- Assets: `assets/*`
- Viewer source: `code/src/main.cpp`

## Configure & build
powershell
mkdir build_local
cd build_local
cmake ..\code -G "MinGW Makefiles"
mingw32-make -j4

## Run 
.\cmdviewer.exe ..\logs\sample.cmdlog



