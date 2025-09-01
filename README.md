# embedded_virt_board_game

A board game rendered on the PC screen, all logic and draw commands dictated by an **MCU** with buttons. This is a simple turn based game where two players compete for resources using strategy and cards. 
This repo renders a 2-player, turn-based ‘virtual board’ on the PC. An MCU (buttons only) sends high-level draw/game commands; the PC viewer (C++/CMake) replays them from a **.cmdlog** file to render frames.
The PC Viewer is used for the debugging process as it would be more difficult to debug the drawing instructions from the .cmdlog files if the final product has not yet been created. Intended for re-use in future proejects. 

## Features
- Minimal **text command language** (`BG`, `TEXT`, `FONT_*`, `IMAGE_LOAD_SHEET`, `ANIM_*`, `FLIP`)  
- Font size/color/selection + text alignment and spacing  
- Spritesheet loading with per-animation FPS and cached textures  
- Per-frame background color and a simple scene queue

## Repository layout
/code C++ source (viewer, parser, renderer)
/logs Example .cmdlog sessions (e.g., sample.cmdlog, sample2.cmdlog)
/assets Fonts, sprites, demo images
/docs Specs, diagrams (future)

## Quickstart
### Linux/macOS 
git clone https://github.com/Ybbor7456/embedded_virt_board_game.git
cd embedded_virt_board_game
cmake -S code -B build
cmake --build build --config Release
./build/virt_board logs/sample.cmdlog  # or logs/sample2.cmdlog

### Windows (PowerShell)
git clone https://github.com/Ybbor7456/embedded_virt_board_game.git
cd embedded_virt_board_game
cmake -S code -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\virt_board.exe .\logs\sample.cmdlog

Requires a C++17+ compiler. 


