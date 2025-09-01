#include <raylib.h>      // core game library
#include <algorithm>     // std::min/std::max, transforms
#include <cctype>        // toupper/tolower helpers
#include <cstdio>
#include <cstdlib>       // std::atoi, std::exit
#include <fstream>       // std::ifstream
#include <optional>      // std::optional for parse results
#include <sstream>       // std::istringstream for tokenizing lines
#include <string>
#include <variant>       // type-safe tagged union for commands
#include <vector>
#include <unordered_map> // associative container (key→value) for animations by id
#include <iomanip>

struct AnimSheet {
    Texture2D tex{};         // loaded spritesheet texture
    int cols = 1, rows = 1;  // grid layout
    int frameW = 0, frameH = 0;
    int total  = 1;          // cols*rows

    // playback state 
    float fps   = 12.0f;     // frames per second
    float accum = 0.0f;      // time accumulator
    int frameIndex = 0;      // current frame

    bool valid() const { return tex.id != 0; }
};

static std::unordered_map<int, AnimSheet> g_anims; // id → animation
enum class Align { Left, Center, Right };

struct CmdCls        {};
struct CmdFlip       { unsigned frame = 0; };
struct CmdText       { int x = 0, y = 0; std::string s; int size = -1; Color col = BLACK; int fontIndex = -1; };
// Images & animation
struct CmdImgLoadSheet { int id; std::string path; int cols; int rows; }; // clear scene for next frame
struct CmdAnimSetFps   { int id; float fps; };              // present frame & tag a frame number
struct CmdAnimDraw     { int id; int x; int y; float scale; }; // draw txt string
struct DrawAnim { int id; int x; int y; float scale; }; //draw lists of sprites and text
struct Scene {
    std::vector<CmdText>  texts;  // text
    std::vector<DrawAnim> anims;  // sprites
    void clear() { texts.clear(); anims.clear(); }
};
struct CmdAnimSetTotal { int id; int total; }; // frame cap
struct CmdFontSize { int size; };
struct FontRes { Font font{}; bool loaded=false; };
struct CmdBg { int r=0, g=0, b=0, a=255; };
struct CmdFontLoad   { int id; std::string path; int sizePx; };
struct CmdFontUse    { int id; };
struct CmdFontColor  { unsigned char r,g,b,a; };
struct CmdTextAlign  { Align a; };
struct CmdTextSpacing{ float px; };

static int g_font_size = 20; 
static std::unordered_map<int, FontRes> g_fonts;
static int   g_currentFont = -1;              // -1 = raylib default
static Color g_textColor   = BLACK;
static Align g_textAlign   = Align::Left;
static float g_textSpacing = 1.0f;

//background color

// Variant that can hold any command 
using Command = std::variant<
    CmdCls, CmdFlip, CmdText,
    CmdImgLoadSheet, CmdAnimSetFps, CmdAnimDraw,
    CmdAnimSetTotal, CmdBg, CmdFontSize, CmdFontLoad, 
    CmdFontUse, CmdFontColor, CmdTextAlign, CmdTextSpacing
>;
/*
// utilities // 
*/
// removes leading/trailing whitespace, e.g. "    yo   " → "yo"
static inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

// xtract the directory portion of a path
static std::string dirname_of(const std::string& path) {
    size_t p = path.find_last_of("/\\");
    if (p == std::string::npos) return ".";  // current dir if no / detected
    return path.substr(0, p);
}

// Join two path segments with a '/'.
static std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char last = a.back();
    if (last == '/' || last == '\\') return a + b;
    return a + "/" + b;
}
/* 
// end utiltiies
*/ 
/*
// parsers
*/
// declarations so parseLine can reference 
static std::optional<CmdAnimSetTotal> parseAnimSetTotal(const std::string& line);
static void doAnimSetTotal(const CmdAnimSetTotal& c);
static std::optional<CmdBg> parseBg(const std::string& line);
static std::optional<CmdFontSize>     parseFontSize(const std::string& line);
static std::optional<CmdFontLoad>     parseFontLoad(const std::string& line);
static std::optional<CmdFontUse>      parseFontUse(const std::string& line);
static std::optional<CmdFontColor>    parseFontColor(const std::string& line);
static std::optional<CmdTextAlign>    parseTextAlign(const std::string& line);
static std::optional<CmdTextSpacing>  parseTextSpacing(const std::string& line);
// 
static std::optional<CmdText> parseText(const std::string& line) {
    std::istringstream iss(line);
    std::string op; int x, y;

    // require the TEXT keyword and coordinates
    if (!(iss >> op) || op != "TEXT") return std::nullopt;
    if (!(iss >> x >> y)) return std::nullopt;

    // find the first quote (you can keep your original logic)
    size_t q1 = line.find('"');
    if (q1 == std::string::npos) return std::nullopt;

    std::string out; out.reserve(64);
    bool closed = false;
    for (size_t i = q1 + 1; i < line.size(); ++i) {
        char c = line[i];
        if (c == '\\') {
            if (i + 1 < line.size()) {
                char n = line[i + 1];
                if (n == '"' || n == '\\') { out.push_back(n); ++i; continue; }
            }
            out.push_back('\\');
        } else if (c == '"') {
            closed = true; break;
        } else {
            out.push_back(c);
        }
    }
    if (!closed) return std::nullopt;

    //stash the current size so each text remembers it
    return CmdText{ x, y, out, -1 };
}

static std::optional<CmdFontSize> parseFontSize(const std::string& line) {
    std::istringstream ss(line);
    std::string k; int sz;
    if (ss >> k && k == "FONT_SIZE" && ss >> sz) return CmdFontSize{sz};
    return std::nullopt;
}

// image load, id, path, col, row
static std::optional<CmdImgLoadSheet> parseImgLoadSheet(const std::string& line) {
    std::istringstream ss(line);
    std::string op; int id, cols, rows;
    if (!(ss >> op >> id)) return std::nullopt;

    // find double quotes 
    size_t q1 = line.find('"');
    if (q1 == std::string::npos) return std::nullopt;
    size_t q2 = line.find('"', q1 + 1); // double quotes
    if (q2 == std::string::npos) return std::nullopt;
    std::string path = line.substr(q1 + 1, q2 - (q1 + 1));

    // parse remaining cols and rows
    std::string tail = line.substr(q2 + 1);
    std::istringstream ss2(tail);
    if (!(ss2 >> cols >> rows)) return std::nullopt;

    return CmdImgLoadSheet{ id, path, cols, rows };
}

//  set fps <id> <fps>
static std::optional<CmdAnimSetFps> parseAnimSetFps(const std::string& line) {
    std::istringstream ss(line);
    std::string op; int id; float fps;
    if (ss >> op >> id >> fps) return CmdAnimSetFps{ id, fps };
    return std::nullopt;
}

//draw id, x, y, scale
static std::optional<CmdAnimDraw> parseAnimDraw(const std::string& line) {
    std::istringstream ss(line);
    std::string op; int id, x, y; float scale;
    if (ss >> op >> id >> x >> y >> scale) return CmdAnimDraw{ id, x, y, scale };
    return std::nullopt;
}

// Decide which concrete command a line represents
//CLS, FLIP, TEXT, IMAGE_LOAD_SHEET, etc. 
static std::optional<Command> parseLine(const std::string& raw) {
    std::string line = trim(raw);
    if (line.empty()) return std::nullopt;

    if (line.rfind("CLS", 0) == 0)  return Command{ CmdCls{} };

    if (line.rfind("FLIP", 0) == 0) {
        std::istringstream iss(line);
        std::string op; unsigned frame = 0;
        iss >> op >> frame;
        return Command{ CmdFlip{ frame } };
    }

    if (line.rfind("TEXT", 0) == 0) {
        if (auto t = parseText(line)) return Command{ *t };
    }

    if (line.rfind("IMAGE_LOAD_SHEET", 0) == 0) {
        if (auto c = parseImgLoadSheet(line)) return Command{ *c };
    }

    if (line.rfind("ANIM_SET_FPS", 0) == 0) {
        if (auto c = parseAnimSetFps(line)) return Command{ *c };
    }

    if (line.rfind("ANIM_DRAW", 0) == 0) {
        if (auto c = parseAnimDraw(line)) return Command{ *c };
    }

    if (line.rfind("ANIM_SET_TOTAL", 0) == 0) {
        if (auto c = parseAnimSetTotal(line)) return Command{ *c };
}
    if (line.rfind("BG", 0) == 0) {
        if (auto c = parseBg(line)) return Command{ *c };
}
   /* if (line.rfind("FONT_SIZE", 0) == 0) {
        if (auto c = parseFontSize(line)) return Command{ *c };
} */
    if (auto c = parseFontSize(line)) return Command{ *c };

    if (line.rfind("FONT_LOAD", 0)   == 0) { if (auto c = parseFontLoad(line))   return Command{*c}; }
    if (line.rfind("FONT_USE", 0)    == 0) { if (auto c = parseFontUse(line))    return Command{*c}; }
    if (line.rfind("FONT_COLOR", 0)  == 0) { if (auto c = parseFontColor(line))  return Command{*c}; }
    if (line.rfind("TEXT_ALIGN", 0)  == 0) { if (auto c = parseTextAlign(line))  return Command{*c}; }
    if (line.rfind("TEXT_SPACING",0) == 0) { if (auto c = parseTextSpacing(line))return Command{*c}; }

    return std::nullopt;
}

static std::optional<CmdAnimSetTotal> parseAnimSetTotal(const std::string& line) {
    std::istringstream ss(line);
    std::string op; int id, total;
    if (ss >> op >> id >> total) return CmdAnimSetTotal{ id, total };
    return std::nullopt;
}
// apply frame cap 
static void doAnimSetTotal(const CmdAnimSetTotal& c) {
    auto it = g_anims.find(c.id);
    if (it != g_anims.end()) {
        int maxFrames = it->second.cols * it->second.rows;
        if (c.total > 0 && c.total <= maxFrames) {
            it->second.total = c.total;
            if (it->second.frameIndex >= c.total) {
                it->second.frameIndex = 0; // reset if out of range
            }
        }
    }
}
// bg rgba values
static std::optional<CmdBg> parseBg(const std::string& line) {
    std::istringstream iss(line);
    std::string op; int r,g,b,a;
    if (!(iss >> op)) return std::nullopt;
    if (op != "BG") return std::nullopt;
    if (!(iss >> r >> g >> b >> a)) return std::nullopt;
    return CmdBg{r,g,b,a};
}

static std::optional<CmdFontLoad> parseFontLoad(const std::string& ln){
    std::istringstream is(ln); std::string op; int id, px; std::string p;
    if(!(is>>op>>id>>std::quoted(p)>>px)) return std::nullopt;
    return CmdFontLoad{id,p,px};
}

static std::optional<CmdFontUse> parseFontUse(const std::string& ln){
    std::istringstream is(ln); std::string op; int id; if(!(is>>op>>id)) return std::nullopt; return CmdFontUse{id};
}

static std::optional<CmdFontColor> parseFontColor(const std::string& ln){
    std::istringstream is(ln); std::string op; int r,g,b,a; if(!(is>>op>>r>>g>>b>>a)) return std::nullopt;
    return CmdFontColor{(unsigned char)r,(unsigned char)g,(unsigned char)b,(unsigned char)a};
}

static std::optional<CmdTextAlign> parseTextAlign(const std::string& ln){
    std::istringstream is(ln); std::string op, val; if(!(is>>op>>val)) return std::nullopt;
    Align a = (val=="CENTER"?Align::Center: val=="RIGHT"?Align::Right: Align::Left);
    return CmdTextAlign{a};
}

static std::optional<CmdTextSpacing> parseTextSpacing(const std::string& ln){
    std::istringstream is(ln); std::string op; float px; if(!(is>>op>>px)) return std::nullopt; return CmdTextSpacing{px};
}
/*
end parsers
*/

/*
// command executors and animation runtime 
*/
static void doImgLoadSheet(const CmdImgLoadSheet& c, const std::string& baseDir) {
    // Resolve path relative to the .cmdlog directory
    std::string full = c.path;
    bool absolute = (full.size() > 1 && (full[1] == ':' || full[0] == '/' || full[0] == '\\'));
    if (!absolute) full = join_path(baseDir, c.path);

    // If already loaded, don't reset frame
    auto it = g_anims.find(c.id);
    if (it != g_anims.end() && it->second.valid()) {
       
        return;
    }
    // load texture 
    AnimSheet anim;
    anim.tex = LoadTexture(full.c_str());
    if (anim.tex.id == 0) {
        TraceLog(LOG_WARNING, "Failed to load texture: %s", full.c_str());
        return;
    }

    SetTextureFilter(anim.tex, TEXTURE_FILTER_POINT);

    anim.cols   = std::max(1, c.cols);
    anim.rows   = std::max(1, c.rows);
    anim.total  = anim.cols * anim.rows;
    anim.frameW = anim.tex.width  / anim.cols;
    anim.frameH = anim.tex.height / anim.rows;

    g_anims[c.id] = anim; //replace animation cache 
}

// update fps
static void doAnimSetFps(const CmdAnimSetFps& c) {
    auto it = g_anims.find(c.id);
    if (it != g_anims.end() && c.fps > 0.0f) it->second.fps = c.fps;
}

// Advance all animations
static void advanceAllAnims(float dt) {
    for (auto& kv : g_anims) {
        AnimSheet& a = kv.second;
        if (!a.valid() || a.fps <= 0.0f) continue;
        const float spf = 1.0f / a.fps;
        a.accum += dt;
        while (a.accum >= spf) {
            a.accum -= spf;
            a.frameIndex = (a.frameIndex + 1) % std::max(1, a.total);
        }
    }
}

// Draw all ANIM_DRAW entries for this frame, draw all queued animations for this frame
static void drawAnims(const Scene& scene) {
    for (const auto& d : scene.anims) {
        auto it = g_anims.find(d.id);
        if (it == g_anims.end()) continue;
        const AnimSheet& a = it->second;
        if (!a.valid() || a.frameW == 0 || a.frameH == 0) continue;
        // Compute source rectangle for the current frame (grid → UV)
        int idx = a.frameIndex;
        int col = idx % a.cols;
        int row = idx / a.cols;

        Rectangle src = { (float)(col * a.frameW), (float)(row * a.frameH),
                          (float)a.frameW, (float)a.frameH };
        Rectangle dst = { (float)d.x, (float)d.y,
                          a.frameW * d.scale, a.frameH * d.scale };
        DrawTexturePro(a.tex, src, dst, {0,0}, 0.0f, WHITE);
    }
}
// Pick the background color for this frame (last BG wins), or fallback
static Color getBgForFrame(const std::vector<Command>& frameCmds, Color fallback) {
    Color bg = fallback;
    for (const auto& c : frameCmds) {
        if (const auto* p = std::get_if<CmdBg>(&c)) {
            bg = Color{ (unsigned char)p->r, (unsigned char)p->g,
                        (unsigned char)p->b, (unsigned char)p->a };
        }
    }
    return bg;
}

std::vector<Command> frameCmds;


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {
    // resoluton
    const int logicalW = 1024, logicalH = 576;

    int windowScale = 1;
    const char* filePath = "sample.cmdlog"; // name of path for .cmdlog
    if (argc > 1) filePath = argv[1];        // .\cmdviewer.exe ..\logs\sample.cmdlog
    if (argc > 2) windowScale = std::max(1, std::atoi(argv[2]));

    // Create window and 2D camera (zoom == integer scale)
    InitWindow(logicalW * windowScale, logicalH * windowScale, "Command Viewer (raylib)"); // window name + dims
    SetTargetFPS(60);

    Camera2D cam{};
    cam.zoom = static_cast<float>(windowScale);

    // load command log
    std::ifstream fin(filePath); // file input stream
    if (!fin) {
        TraceLog(LOG_WARNING, "Could not open %s. Ensure you run: ./cmdviewer ../logs/sample.cmdlog or other name", filePath);
    }

    // Parse all command
    std::vector<Command> cmds;
    if (fin) {
        std::string line;
        while (std::getline(fin, line)) {
            if (auto c = parseLine(line)) cmds.push_back(std::move(*c));
        }
    }

    const std::string baseDir = dirname_of(filePath); // for resolving relative asset paths

    Scene scene;
    size_t idx = 0;                  // index into cmds 
   // unsigned lastFrame = 0;         
    // const int fontSize = 20; 
    // ^^ swapped for g_fontsize

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        advanceAllAnims(dt); // advance animation clocks every tick

       
if (!cmds.empty()) {    // Build the current frame contents from cmds until the next FLIP
   
   if (!cmds.empty()) {     
    if (idx >= cmds.size()) idx = 0;    // loop playback when reaching end


    // bool didCls = false;    // tracks whether this frame started with CLS
    //build loop
    int currentFont = g_font_size;
    
    for (; idx < cmds.size(); ++idx) {
        if (std::holds_alternative<CmdCls>(cmds[idx])) {
            scene.clear();
            frameCmds.clear();
            //didCls = true;
        }
        else if (std::holds_alternative<CmdText>(cmds[idx])) {
            CmdText t = std::get<CmdText>(cmds[idx]);
            if (t.size <= 0) t.size = g_font_size;     // freeze size
            t.col       = g_textColor;                 // freeze color
            t.fontIndex = g_currentFont;               // freeze which font
            scene.texts.push_back(std::move(t));
            frameCmds.push_back(cmds[idx]);
        }

        else if (std::holds_alternative<CmdImgLoadSheet>(cmds[idx])) {
                doImgLoadSheet(std::get<CmdImgLoadSheet>(cmds[idx]), baseDir);
            }
        else if (std::holds_alternative<CmdAnimSetFps>(cmds[idx])) {
                doAnimSetFps(std::get<CmdAnimSetFps>(cmds[idx]));
            }
        else if (std::holds_alternative<CmdAnimSetTotal>(cmds[idx])) {
                doAnimSetTotal(std::get<CmdAnimSetTotal>(cmds[idx]));
            }
        else if (std::holds_alternative<CmdAnimDraw>(cmds[idx])) {
                const auto& c = std::get<CmdAnimDraw>(cmds[idx]);
                scene.anims.push_back({ c.id, c.x, c.y, c.scale });
            }
        else if (std::holds_alternative<CmdBg>(cmds[idx])) {
                frameCmds.push_back(cmds[idx]);
            }
        else if (std::holds_alternative<CmdFontSize>(cmds[idx])) {
            currentFont = std::get<CmdFontSize>(cmds[idx]).size;
            g_font_size = currentFont;             
        }

        else if (std::holds_alternative<CmdFlip>(cmds[idx])) {
              //  lastFrame = std::get<CmdFlip>(cmds[idx]).frame;
                ++idx;
                break;
            }
        else if (std::holds_alternative<CmdFontLoad>(cmds[idx])) {
            auto c = std::get<CmdFontLoad>(cmds[idx]);
        if (!g_fonts[c.id].loaded){
            g_fonts[c.id].font = LoadFontEx(c.path.c_str(), c.sizePx, nullptr, 0);
            g_fonts[c.id].loaded = true;
        }
        }
        else if (std::holds_alternative<CmdFontUse>(cmds[idx])) {
            g_currentFont = std::get<CmdFontUse>(cmds[idx]).id;
        }
        else if (std::holds_alternative<CmdFontColor>(cmds[idx])) {
            auto c = std::get<CmdFontColor>(cmds[idx]);
            g_textColor = Color{(unsigned char)c.r,(unsigned char)c.g,(unsigned char)c.b,(unsigned char)c.a};
            frameCmds.push_back(cmds[idx]);
            //TraceLog(LOG_INFO, "FONT_COLOR = %d,%d,%d,%d", c.r, c.g, c.b, c.a);
        }
        else if (std::holds_alternative<CmdTextAlign>(cmds[idx])) {
            g_textAlign = std::get<CmdTextAlign>(cmds[idx]).a;
        }
        else if (std::holds_alternative<CmdTextSpacing>(cmds[idx])) {
            g_textSpacing = std::get<CmdTextSpacing>(cmds[idx]).px;
        }
        
    }

}

}
    Color frameBg = getBgForFrame(frameCmds, {222, 218, 216,255}); // Resolve background color for this frame

    BeginDrawing();
    ClearBackground(frameBg);

    BeginMode2D(cam);

    // draw text command
  for (const auto& t : scene.texts) {
    // pick the frozen font (falls back to current/default safely)
    int idx = (t.fontIndex >= 0) ? t.fontIndex : g_currentFont;
    Font use =
        (idx >= 0 && idx < (int)g_fonts.size() && g_fonts[idx].loaded)
            ? g_fonts[idx].font
            : GetFontDefault();

    float size    = (t.size > 0) ? (float)t.size : (float)g_font_size;
    float spacing = g_textSpacing; // keep global unless you also freeze it

    Vector2 pos{ (float)t.x, (float)t.y };

    // alignment with the actual font+size being drawn
    if (g_textAlign != Align::Left) {
        Vector2 sz = MeasureTextEx(use, t.s.c_str(), size, spacing);
        if (g_textAlign == Align::Center) pos.x -= sz.x * 0.5f;
        else if (g_textAlign == Align::Right) pos.x -= sz.x;
    }

    // use the frozen color (this is the key change)
    DrawTextEx(use, t.s.c_str(), pos, size, spacing, t.col);
}


    // draw ANIM_DRAW command
    drawAnims(scene);

    EndMode2D();

    EndDrawing();
    }


g_anims.clear();

for (auto &kv : g_anims)
    if (kv.second.valid()) UnloadTexture(kv.second.tex);
g_anims.clear();

for (auto &kv : g_fonts)
    if (kv.second.loaded) UnloadFont(kv.second.font);
g_fonts.clear();

CloseWindow();
return 0;}


/* 
$TOOLS = "$HOME\Tools"
$env:Path = "$TOOLS\cmake-4.1.0-windows-x86_64\bin;$env:Path"

$env:Path = "$TOOLS\mingw\mingw64\bin;$env:Path"

cd C:\Users\robhu\boardgame\build_ok6
cmake ..\code -G "MinGW Makefiles"
mingw32-make -j4
.\cmdviewer.exe ..\logs\sample.cmdlog
*/
