#include <raylib.h>     // core game library
#include <algorithm>    //std::find, std::sort, std::transform
#include <cctype>       //case-sensitive
#include <cstdio>       //
#include <cstdlib>      //std::exit, std::system, std::rand
#include <fstream>      //
#include <optional>     // ye
#include <sstream>      // ye
#include <string>
#include <variant>      //ye
#include <vector>

// ---------------- Command Types ----------------
struct CmdCls {};
struct CmdFlip { unsigned frame = 0; };
struct CmdText { int x = 0, y = 0; std::string s; };

using Command = std::variant<CmdCls, CmdFlip, CmdText>;

// removes whitespace, e.g. "    yo   " -> returns "yo"
static inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

//parses one command text line to CmdText struct
static std::optional<CmdText> parseText(const std::string& line) {
    std::istringstream iss(line);
    std::string op; int x, y;
    if (!(iss >> op >> x >> y)) return std::nullopt;

    // Find the first quote 
    size_t q1 = line.find('"');
    if (q1 == std::string::npos) return std::nullopt;

    // Parse until the 2nd double quote
    std::string out;
    bool closed = false;
    for (size_t i = q1 + 1; i < line.size(); ++i) {
        char c = line[i];
        if (c == '\\') {
            // Handle simple escapes: \"
            if (i + 1 < line.size()) {
                char n = line[i + 1];
                if (n == '"' || n == '\\') { out.push_back(n); ++i; continue; }
            }
            // Unknown escape: keep the backslash as-is
            out.push_back('\\');
        } else if (c == '"') {
            closed = true; 
            break;
        } else {
            out.push_back(c);
        }
    }
    if (!closed) return std::nullopt;

    return CmdText{ x, y, out };
}


static std::optional<Command> parseLine(const std::string& raw) {
    std::string line = trim(raw);
    if (line.empty()) return std::nullopt;

    if (line.rfind("CLS", 0) == 0) return Command{ CmdCls{} };

    if (line.rfind("FLIP", 0) == 0) {
        std::istringstream iss(line);
        std::string op; unsigned frame = 0;
        iss >> op >> frame;
        return Command{ CmdFlip{ frame } };
    }

    if (line.rfind("TEXT", 0) == 0) {
        auto t = parseText(line);
        if (t) return Command{ *t };
    }

    // Minimal viewer: ignore unknown lines (HELLO/VIEW/etc)
    return std::nullopt;
}

struct Scene {
    std::vector<CmdText> texts;
    void clear() { texts.clear(); }
};

int main(int argc, char** argv) {
    // resoluton
    const int logicalW = 1024, logicalH = 576;

    int windowScale = 1;
    const char* filePath = "sample.cmdlog"; // name of path fior .cmdlog
    if (argc > 1) filePath = argv[1]; //.\cmdviewer.exe .sample.cmdlog
    if (argc > 2) windowScale = std::max(1, std::atoi(argv[2])); 

    InitWindow(logicalW * windowScale, logicalH * windowScale, "Command Viewer (raylib)"); // window name + dimin
    SetTargetFPS(60);

    Camera2D cam{};
    cam.zoom = static_cast<float>(windowScale);

    // load command log
    std::ifstream fin(filePath);
    if (!fin) { //file input string
        TraceLog(LOG_WARNING, "Could not open %s. Ensure you run: ./cmdviewer ../sample.cmdlog or other name", filePath);
    }

    std::vector<Command> cmds;
    if (fin) {
        std::string line;
        while (std::getline(fin, line)) {
            if (auto c = parseLine(line)) cmds.push_back(std::move(*c));
        }
    }

    Scene scene; 
    size_t idx = 0; // indexed vector in cmds
    unsigned lastFrame = 0;
    int fontSize = 20;

    while (!WindowShouldClose()) {
        //current frame until FLIP
        if (!cmds.empty()) {
            scene.clear();
            for (; idx < cmds.size(); ++idx) {
                if (std::holds_alternative<CmdCls>(cmds[idx])) {
                    scene.clear();
                } else if (std::holds_alternative<CmdText>(cmds[idx])) {
                    scene.texts.push_back(std::get<CmdText>(cmds[idx]));
                } else if (std::holds_alternative<CmdFlip>(cmds[idx])) {
                    lastFrame = std::get<CmdFlip>(cmds[idx]).frame;
                    ++idx; // consume FLIP
                    break;
                }
            }
            if (idx >= cmds.size()) idx = 0; // loop playback
        }

        BeginDrawing();
        ClearBackground(RAYWHITE); // BLUE, BLACK
        //Color mc = {r,g,b,a}

        BeginMode2D(cam);
        for (const auto& t : scene.texts) { // reference automatically detected strct (CmdText)
            DrawText(t.s.c_str(), t.x, t.y, fontSize, BLACK); 
        }
        EndMode2D();

        // HUD footer
        DrawRectangle(0, GetScreenHeight() - 24, GetScreenWidth(), 24, Fade(BLACK, 0.1f));
        DrawText(TextFormat("Frame: %u   File: %s   Scale: %dx",
                            lastFrame, filePath, windowScale),
                 8, GetScreenHeight() - 22, 18, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
