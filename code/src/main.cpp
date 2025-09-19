#include "audio/audio.h"
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
#include <iomanip>          // quoted()
#include <string>
#include <vector>       
#include <functional> 
#include <cctype>
#include <cmath>


enum class Align { Left, Center, Right }; //allignment
enum class Screen { Title, Rules, Instructions, Game }; // boxes
enum class UIMode { Normal, Fallback };

struct CmdCls        {};
struct CmdFlip       { unsigned frame = 0; };
struct CmdText       { 
    int x = 0, 
    y = 0; 
    std::string s; 
    int size = -1; 
    Color col = BLACK; 
    int fontIndex = -1; 
    };

// Images & animation
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
struct CmdImgLoadSheet {
    int id;
    std::string path;
    int cols;
    int rows;
};
// clear scene for next frame
struct CmdAnimSetFps   {
    int id;
    float fps; 
};              
// present frame & tag a frame number
struct CmdAnimDraw     {
    int id;
    int x; 
    int y; 
    float scale;
    }; // draw txt string

struct DrawAnim { 
    int id; 
    int x; 
    int y; 
    float scale; }; //draw lists of sprites and text


struct Scene {
    std::vector<CmdText>  texts;  // text
    std::vector<DrawAnim> anims;  // sprites
    void clear() { texts.clear(); anims.clear(); }
};

struct CmdAnimSetTotal { 
    int id; 
    int total; }; // frame cap

// font and color
struct CmdFontSize { int size; };
struct FontRes { 
    Font font{};
    bool loaded=false;
    };
struct CmdBg { 
    int r=0, g=0, b=0, a=255; };
struct CmdFontLoad   { 
    int id;
    std::string path; 
    int sizePx; 
    };
struct CmdFontUse    { int id; };
struct CmdFontColor  { unsigned char r,g,b,a; };
struct CmdTextAlign  { Align a; };
struct CmdTextSpacing{ float px; };


// interacting boxes

struct MenuItem {
    std::string id; 
    std::string label;
    Rectangle rect;                         //visual button region
    std::string targetLog;                  //targeted screen log
    
};

struct AppState{
    Screen screen = Screen::Title; 
    int selected = 0;
    std::vector<MenuItem> tileMenu;         //
    Color focusColor      = YELLOW;         // outline color for selected item
    float focusThickness  = 3.0f;           // outline thickness
    Color focusShadow    = CLITERAL(Color){  0,   0,   0,  80};
    Color idleOutline    = CLITERAL(Color){255, 255, 255, 48};
    std::string currentPath;
    std::vector<std::string> navStack;
 };
struct CmdHitbox { 
    std::string id;
    Rectangle r; };           
struct CmdTarget { 
    std::string id;
     std::string path; }; 



struct CmdMusic{
    std::string path;
    bool loop = true;
    float volume = 1.f;  
    };


struct CmdStopMusic{
    int fadeMusic = 0; 
};

struct CmdSfx{
    std::string path; 
    float volume = 1.f;
};

struct CmdMusicVolume{
std::string path; 
float volume = 1.f;
int fadeMusic = 0;
};


//default values/declarations/definitions
static std::unordered_map<int, AnimSheet> g_anims; // id → animation
static int g_font_size = 20; 
static std::unordered_map<int, FontRes> g_fonts;
static Color g_textColor   = BLACK;
static Align g_textAlign   = Align::Left;
static float g_textSpacing = 1.0f;
static UIMode gMode = UIMode::Normal;
static std::string gLastError;


// Variant that can hold any command 
using Command = std::variant<
    CmdCls, CmdFlip, CmdText,
    CmdImgLoadSheet, CmdAnimSetFps, CmdAnimDraw,
    CmdAnimSetTotal, CmdBg, CmdFontSize, CmdFontLoad, 
    CmdFontUse, CmdFontColor, CmdTextAlign, CmdTextSpacing,
    CmdHitbox, CmdTarget, CmdMusic, CmdStopMusic, CmdSfx,
    CmdMusicVolume
>;

/*
█░█ ▀█▀ █ █░░ █ ▀█▀ █ █▀▀ █▀
█▄█ ░█░ █ █▄▄ █ ░█░ █ ██▄ ▄█
*/

// removes leading/trailing whitespace, e.g. "    yo   " → "yo"


static inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}


static inline std::string lower_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

static inline std::string normalize_key(const std::string& s) {
    return lower_copy(trim(s));   // reuse your trim()
}

static MenuItem* findMenuItem(AppState& S, const std::string& key) {
    const std::string k = normalize_key(key);

    // 1) try id
    for (auto& it : S.tileMenu)
        if (!it.id.empty() && normalize_key(it.id) == k) return &it;

    // 2) fallback to label
    for (auto& it : S.tileMenu)
        if (!it.label.empty() && normalize_key(it.label) == k) return &it;

    return nullptr;
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

static inline bool is_absolute_path(const std::string& p) {
    return !p.empty() && (p[0] == '/' || p[0] == '\\' || (p.size() > 1 && p[1] == ':'));
}

static inline bool ensure_file(const std::string& full, const char* what) {
    if (!FileExists(full.c_str())) {               // raylib helper
        TraceLog(LOG_ERROR, "%s missing: %s", what, full.c_str());
        return false;
    }
    return true;
}

/* 
█▀▀ █▄░█ █▀▄   █░█ ▀█▀ █ █░░ █ ▀█▀ █ █▀▀ █▀
██▄ █░▀█ █▄▀   █▄█ ░█░ █ █▄▄ █ ░█░ █ ██▄ ▄█
*/ 

/*
█▀█ ▄▀█ █▀█ █▀ █▀▀ █▀█ █▀
█▀▀ █▀█ █▀▄ ▄█ ██▄ █▀▄ ▄█
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
static bool LoadCmdLog(const std::string& path, std::vector<Command>& out);

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




/*
█▀█ ▄▀█ █▀█ █▀ █▀▀
█▀▀ █▀█ █▀▄ ▄█ ██▄
▄▀█ █▄░█ █ █▀▄▀█ ▄▀█ ▀█▀ █ █▀█ █▄░█
█▀█ █░▀█ █ █░▀░█ █▀█ ░█░ █ █▄█ █░▀█         
*/


// image load, id, path, col, row
static std::optional<CmdImgLoadSheet> parseImgLoadSheet(const std::string& line) {
    std::istringstream ss(line);
    std::string op; 
    int id, cols, rows;
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
// bg rgba values for bg color
static std::optional<CmdBg> parseBg(const std::string& line) {
    std::istringstream iss(line);
    std::string op; int r,g,b,a;
    if (!(iss >> op)) return std::nullopt;
    if (op != "BG") return std::nullopt;
    if (!(iss >> r >> g >> b >> a)) return std::nullopt;
    return CmdBg{r,g,b,a};
}


/*
█▀█ ▄▀█ █▀█ █▀ █▀▀
█▀▀ █▀█ █▀▄ ▄█ ██▄             
█▀▀ █▀█ █▄░█ ▀█▀
█▀░ █▄█ █░▀█ ░█░            // trouble using 2 fonts in 1 cmdlog file
*/


static std::optional<CmdFontSize> parseFontSize(const std::string& line) {
    std::istringstream ss(line);
    std::string k; int sz;
    if (ss >> k && k == "FONT_SIZE" && ss >> sz) return CmdFontSize{sz};
    return std::nullopt;
}

static std::optional<CmdFontLoad> parseFontLoad(const std::string& ln){
    std::istringstream is(ln); std::string op; int id, px; std::string p;
    if(!(is>>op>>id>>std::quoted(p)>>px)) return std::nullopt;
    return CmdFontLoad{id,p,px};
}

static std::optional<CmdFontUse> parseFontUse(const std::string& ln){
    std::istringstream is(ln);
    std::string op;
    int id; 
    if(!(is>>op>>id)) return std::nullopt; 
    return CmdFontUse{id};
}

static std::optional<CmdFontColor> parseFontColor(const std::string& ln){
    std::istringstream is(ln); 
    std::string op; 
    int r,g,b,a; 
    if(!(is>>op>>r>>g>>b>>a)){ return std::nullopt;}
    return CmdFontColor{(unsigned char)r,(unsigned char)g,(unsigned char)b,(unsigned char)a};
}

static std::optional<CmdTextAlign> parseTextAlign(const std::string& ln){
    std::istringstream is(ln); std::string op, val; 
    if(!(is>>op>>val)) return std::nullopt;
    Align a = (val=="CENTER"?Align::Center: val=="RIGHT"?Align::Right: Align::Left);
    return CmdTextAlign{a};
}

static std::optional<CmdTextSpacing> parseTextSpacing(const std::string& ln){
    std::istringstream is(ln); 
    std::string op; 
    float px; 
    if(!(is>>op>>px)) return std::nullopt;
    return CmdTextSpacing{px};
}

static inline std::string trim_and_strip_comment(std::string s) {
    // strip '#' comment
    if (auto p = s.find('#'); p != std::string::npos) s.resize(p);
    // trim ends
    auto wsfront = std::find_if_not(s.begin(), s.end(), ::isspace);
    auto wsback  = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
    if (wsfront >= wsback) return {};
    return std::string(wsfront, wsback);
}

static inline std::pair<std::string, std::vector<std::string>>

lex_cmd(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    bool inQ = false;

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') { inQ = !inQ; continue; }
        if (!inQ && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);

    std::string op = out.empty() ? "" : out.front();
    if (!out.empty()) out.erase(out.begin());
    return {op, out};
}


// Decide which concrete command a line represents
//CLS, FLIP, TEXT, IMAGE_LOAD_SHEET, etc. 
static std::optional<Command> parseLine(const std::string& raw) {
    std::string line = trim(raw);
    std::string line2 = trim_and_strip_comment(raw);
    

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

    auto [op, args] = lex_cmd(line);
    if (op == "HITBOX" && args.size() == 5) {
    CmdHitbox c;
    c.id = args[0];
    c.r  = Rectangle{ std::stof(args[1]), std::stof(args[2]),
                      std::stof(args[3]), std::stof(args[4]) };
    return Command{c};
}
    
// TARGET id path
    if (op == "TARGET" && args.size() >= 2) {
    CmdTarget c;
    c.id   = args[0];
    c.path = args[1]; // keep as-is; your loader can resolve relative paths
    return Command{c};
}

    std::istringstream ss(line);
    std::string id; float x,y,w,h;
    if (ss >> op >> id >> x >> y >> w >> h; op == "HITBOX") {
        return Command{ CmdHitbox{ id, Rectangle{ x,y,w,h } } };
    }

    if (line.rfind("TARGET", 0) == 0) {
    std::string op, id;
    std::istringstream ss(line);
    ss >> op >> id;
    size_t q1 = line.find('"');
    size_t q2 = line.find('"', q1 + 1);
    if (q1 != std::string::npos && q2 != std::string::npos) {
        std::string p = line.substr(q1 + 1, q2 - q1 - 1);
        return Command{ CmdTarget{ id, p } };
    }
}

    if (line.rfind("SOUND" && !args.empty())){
        CmdMusic c; 
        c.path = args[0];
        c.loop;
        c.volume;
    }

    if (line.rfind("STOPSOUND" && !args.empty())){
        CmdStopMusic c; 
        c.fadeMusic;
    }

    if (line.rfind("SFX" && !args.empty())){
        CmdSfx c; 
        c.path = args[0];
        c.volume;
    }

    if (line.rfind("MUSICVOLUME" && !args.empty())){
        CmdMusicVolume c; 
        c.path;
        c.fadeMusic;
        c.volume;
    }

    return std::nullopt;
}




/*
█▀▀ █▄░█ █▀▄   █▀█ ▄▀█ █▀█ █▀ █▀▀ █▀█ █▀
██▄ █░▀█ █▄▀   █▀▀ █▀█ █▀▄ ▄█ ██▄ █▀▄ ▄█
*/



/*
// command executors and animation runtime 
*/


static bool LoadCmdLog(const std::string& path, std::vector<Command>& out) {
    out.clear();

    std::ifstream fin(path);
    if (!fin) {
          TraceLog(LOG_ERROR, "Could not open cmdlog: %s", path.c_str());
        return false;
    }

    std::string line;
    while (std::getline(fin, line)) {
        if (auto c = parseLine(line)) {
            out.push_back(std::move(*c));
        }
    }
    return true;
}


static void doImgLoadSheet(const CmdImgLoadSheet& c, const std::string& baseDir) {
    // Resolve path relative to the .cmdlog directory
    std::string full = c.path;
    bool absolute = is_absolute_path(full);
    if (!absolute) full = join_path(baseDir, c.path);

    // If already loaded, don't reset frame
    auto it = g_anims.find(c.id);
    if (it != g_anims.end() && it->second.valid()) { //prevents unneeded load frames
                        // if not at end , and doesnt return id = 0
        return;
    }
    
    if (!ensure_file(full, "TEXTURE")) {
        return;  // leave animation missing; draw will just skip, loads TraceLog() for "TEXTURE"
    }
    
    // load texture 
    AnimSheet anim;
    anim.tex = LoadTexture(full.c_str());
    if (anim.tex.id == 0) {
        TraceLog(LOG_ERROR, "LoadTexture failed: %s", full.c_str());
        return;
    }
    SetTextureFilter(anim.tex, TEXTURE_FILTER_POINT); // if ID is not 0

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
            bg = Color{ 
            (unsigned char)p->r, (unsigned char)p->g,
            (unsigned char)p->b, (unsigned char)p->a };
        }
    }
    return bg;
    }

std::vector<Command> frameCmds;



static inline bool BtnUp()    { return IsKeyPressed(KEY_UP);    }
static inline bool BtnDown()  { return IsKeyPressed(KEY_DOWN);  }
static inline bool BtnLeft()  { return IsKeyPressed(KEY_LEFT);  }
static inline bool BtnRight() { return IsKeyPressed(KEY_RIGHT); }
static inline bool BtnA()     { return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_A); }
static inline bool BtnB()     { return IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_B); }



static inline void DrawFocusBox(const Rectangle& r, Color outline, float thickness, Color shadow) {
    DrawRectangleLinesEx({ r.x + 2, r.y + 2, r.width, r.height }, thickness, shadow);
    DrawRectangleLinesEx(r, thickness, outline);
}

/* Builds the Title menu: labels, target logs, and hit-rects */
void initTitleMenu(AppState& S) {
    
    S.screen   = Screen::Title;
    
   
    // Adjust targetLog filenames to match repo (e.g., "logs/title.cmdlog")
    S.tileMenu.clear();
    S.tileMenu.push_back({ "start", "Start",        {0,0,0,0}, "logs/start.cmdlog" });  //remove for data driven approach
    S.tileMenu.push_back({ "rules", "Rules",        {0,0,0,0}, "logs/rules.cmdlog" }); // remove 
    S.tileMenu.push_back({ "description", "Description", {0,0,0,0}, "logs/description.cmdlog" }); // remove 
    S.selected = 0; //default value
 
    const int logicalW = 1024;
    const int logicalH = 576;

    //current font size for a sensible box, font size is already global 
    const int   padX       = 24;    // x padding
    const int   padY       = 12;    // y padding
    const int   spacingY   = 56;   // vertical space between items
    const float fontPx     = (float)g_font_size;
    Font        font       = GetFontDefault();   // raylib method to return built-in font

    // First item y so that the three items are vertically centered
    const int totalH = (int)(3 * (fontPx + padY * 2) + 2 * spacingY);   // estimates total list height, default hitbox hovers
    int y = (logicalH - totalH) / 2;                                    // replace once data driven approach is taken

    for (auto& it : S.tileMenu) {
        // Measure label and make a padded rectangle
        Vector2 txt = MeasureTextEx(font, it.label.c_str(), fontPx, 1.0f);
        float w = txt.x + padX * 2;
        float h = txt.y + padY * 2;
        float x = (logicalW - w) * 0.5f;

        it.rect = { x, (float)y, w, h };
        y += (int)(h + spacingY);
    }
}

// Handle up/down selection at the title screen 
static void UpdateMenu(AppState& S) {
    if (S.tileMenu.empty()) return;
    const int cur = S.selected;

   /* int delta = 0;
    if (IsKeyPressed(KEY_UP)) delta = -1;; 
    if (IsKeyPressed(KEY_DOWN)) delta = +1;;
    if (IsKeyPressed(KEY_LEFT)); delta = -1;
    if (IsKeyPressed(KEY_RIGHT)) delta = +1;;


    if (delta != 0) {
        int n = (int)S.tileMenu.size();
        S.selected = (S.selected + delta + n) % n;
    } */
    auto center = [](const Rectangle& r) -> Vector2 {
        return { r.x + r.width * 0.5f, r.y + r.height * 0.5f };
    };

    const Vector2 c0 = center(S.tileMenu[cur].rect);

    auto choose = [&](Vector2 dir) {
        int best = -1;
        float bestScore = 1e9f;

        for (int i = 0; i < (int)S.tileMenu.size(); ++i) {
            if (i == cur) continue;

            Vector2 c = center(S.tileMenu[i].rect);
            Vector2 v{ c.x - c0.x, c.y - c0.y };
            float len = sqrtf(v.x*v.x + v.y*v.y);
            if (len < 1e-3f) continue;

            Vector2 vn{ v.x / len, v.y / len };
            float d = vn.x * dir.x + vn.y * dir.y;  

            if (d <= 0.2f) continue;                

            float angPenalty = (1.0f - d) * 1000.0f; 
            float dist       = len;                  
            float score      = angPenalty + dist;

            if (score < bestScore) { bestScore = score; best = i; }
        }
        if (best >= 0) S.selected = best;
    };

    if (BtnRight()) choose({ 1, 0 });
    if (BtnLeft())  choose({-1, 0 });
    if (BtnDown())  choose({ 0, 1 });
    if (BtnUp())    choose({ 0,-1 });
}

/* Reset state when you go "back" to title (B button). */
void HandleBackToTitle(AppState& S) {
    S.screen   = Screen::Title;
    S.selected = 0;

}

/* Draws the overlay (the focus ring) over the current menu item */
void DrawMenuOverlay(const AppState& S) {
    if (S.screen != Screen::Title) return;
    if (S.tileMenu.empty()) return;

    // Draw faint boxes for all items (optional)
    for (size_t i = 0; i < S.tileMenu.size(); ++i) {
    const Rectangle& r = S.tileMenu[i].rect;
    Color c = (i == (size_t)S.selected) ? Fade(S.focusColor, 0.25f) : S.idleOutline;
    DrawRectangleLinesEx(r, 1.0f, c);
}

    // Emphasize the focused one
    DrawFocusBox(S.tileMenu[S.selected].rect, S.focusColor, S.focusThickness, S.focusShadow);
}


static void ApplyUiMetaFromCmds(AppState& S, const std::vector<Command>& cmds) {
    auto findId = [&](const std::string& id) -> MenuItem* {
        for (auto& it : S.tileMenu) if (it.id == id) return &it;

                                                            // 2) fallback by label text 
       // for (auto& it : S.tileMenu)                       // removed search by label
        //    if (normalize_key(it.label) == k) return &it;

        return nullptr;
    };

    S.tileMenu.reserve(S.tileMenu.size() + cmds.size());
    for (const auto& c : cmds) {
        if (const auto* hb = std::get_if<CmdHitbox>(&c)) {
            if (auto* it = findId(hb->id)) it->rect = hb->r;
            else S.tileMenu.push_back({ hb->id, hb->id, hb->r, "" });
        } else if (const auto* tg = std::get_if<CmdTarget>(&c)) {
            if (auto* it = findId(tg->id)) it->targetLog = tg->path;
            else S.tileMenu.push_back({ tg->id, tg->id, {0,0,0,0}, tg->path });
        }
    }
    }


static bool LoadScreen(AppState& S, const std::string& path, std::vector<Command>& cmds, bool pushToStack = true) {
    if (!LoadCmdLog(path, cmds)) return false;
    S.tileMenu.clear();
    ApplyUiMetaFromCmds(S, cmds);  
    S.currentPath = path;
    S.selected    = 0;

    if (pushToStack) {
        if (S.navStack.empty() || S.navStack.back() != path) S.navStack.push_back(path);
    }
    return true;
}

static bool GoBack(AppState& S, std::vector<Command>& cmds, const std::string& titlePath = "logs/title.cmdlog") {
    if (!S.navStack.empty()) S.navStack.pop_back();
    const std::string* prev = S.navStack.empty() ? &titlePath : &S.navStack.back();
    return LoadScreen(S, *prev, cmds, false);
}

static void DumpNav(const AppState& S, const char* label = nullptr) {
    if (label) TraceLog(LOG_INFO, "%s", label);
    TraceLog(LOG_INFO, "current: %s", S.currentPath.c_str());
    TraceLog(LOG_INFO, "stack:%zu", S.navStack.size());
    for (size_t i = 0; i < S.navStack.size(); ++i) {
        TraceLog(LOG_INFO, "  %zu: %s", i, S.navStack[i].c_str());
    }
}
/*
█▀▄▀█ ▄▀█ █ █▄░█
█░▀░█ █▀█ █ █░▀█
*/

int main(int argc, char** argv) {
    // resoluton
    const int logicalW = 1024, logicalH = 576;

    int windowScale = 1;
    std::string filePath = (argc > 1) ? argv[1] : "logs/title.cmdlog"; // name of path for .cmdlog
    if (argc > 1) filePath = argv[1];        // .\cmdviewer.exe ..\logs\sample.cmdlog
    if (argc > 2) windowScale = std::max(1, std::atoi(argv[2]));

    // Create window and 2D camera (zoom == integer scale)
    InitWindow(logicalW * windowScale, logicalH * windowScale, "Command Viewer (raylib)"); // window name + dims
    Audio_Init();
    SetTraceLogLevel(LOG_INFO);  // loud checks in console (linux not alligning cmdlog text)
    
    SetTargetFPS(60);

    AppState S;
    initTitleMenu(S);

    Scene scene;
    Camera2D cam{};
    cam.zoom = static_cast<float>(windowScale);

    // load command log
    std::vector<Command> cmds;
    const std::string& path = S.tileMenu[S.selected].targetLog;
    if (LoadCmdLog(filePath, cmds)) {
        ApplyUiMetaFromCmds(S, cmds);   // <-- pulls HITBOX/TARGET from the file
        } 
    else {
        CloseWindow();
        return 1;        // bail
         }
    
    if (!LoadScreen(S, filePath, cmds)) { CloseWindow(); return 1; } 

    const std::string baseDir = dirname_of(filePath); // for resolving relative asset paths
   // DumpNav(S, " ********* startup");
    
    size_t idx = 0;                  // index into cmds 
   // unsigned lastFrame = 0;         
    // const int fontSize = 20; 
    // ^^ swapped for g_fontsize

    

    while (!WindowShouldClose()) {
            const float dt = GetFrameTime();
            advanceAllAnims(dt); // advance animation clocks every tick

            UpdateMenu(S);

            if (!cmds.empty()) {    // Build the current frame contents from cmds until the next FLIP
            
        // if (!cmds.empty()) {     
                if (idx >= cmds.size()) idx = 0;    // loop playback when reaching end

                


                if (!S.tileMenu.empty() && BtnA()) {
                        const std::string next = S.tileMenu[S.selected].targetLog;
                      /* TraceLog(LOG_INFO, "A: selected=%d id=%s next=%s",
                        S.selected,
                        S.tileMenu[S.selected].id.c_str(),
                        next.c_str()); */
                     //   DumpNav(S, "before A");
                    if (!LoadScreen(S, next, cmds)) { CloseWindow(); return 1; }
                   // DumpNav(S, "after A");

                if (!LoadScreen(S, next, cmds)) { CloseWindow(); return 1; }
            
        }
             /*   if (BtnA()){
                    DumpNav(S, "***************** A pressed"); 
                } */
                if (BtnB()) {   // backspace to previous window
                    if (!GoBack(S, cmds)) { CloseWindow();
                    return 1; }
                //    DumpNav(S, "*********** B:after");
    }
               

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
                        //t.fontIndex = g_currentFont;               // freeze which font
                        scene.texts.push_back(std::move(t));
                        frameCmds.push_back(cmds[idx]);
                    }
                    else if (std::holds_alternative<CmdFlip>(cmds[idx])) {
                        //  lastFrame = std::get<CmdFlip>(cmds[idx]).frame;
                            ++idx;
                            break;
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

                    
                    else if (std::holds_alternative<CmdFontLoad>(cmds[idx])) {
                    auto c = std::get<CmdFontLoad>(cmds[idx]);
                    if (!g_fonts[c.id].loaded) {
                    // Resolve font path relative to the .cmdlog directory
                    std::string full = c.path;
                    if (!is_absolute_path(full)) full = join_path(baseDir, c.path);

                    // Loud check before we try to load
                    if (!ensure_file(full, "FONT")) {
                        // fall back so drawing still works (centering will use default metrics)
                        g_fonts[c.id].font   = GetFontDefault();
                        g_fonts[c.id].loaded = true;
                    } else {
                        Font f = LoadFontEx(full.c_str(), c.sizePx, nullptr, 0);
                        if (f.baseSize == 0) {
                           // TraceLog(LOG_ERROR, "LoadFontEx failed: %s", full.c_str());
                            g_fonts[c.id].font = GetFontDefault();
                        } else {
                            //TraceLog(LOG_INFO, "Loaded font[%d]: %s (size=%d)", c.id, full.c_str(), c.sizePx);
                            g_fonts[c.id].font = f;
                        }
                        g_fonts[c.id].loaded = true;
                    }
                }
            }

                    else if (std::holds_alternative<CmdFontUse>(cmds[idx])) {
                    // g_currentFont = std::get<CmdFontUse>(cmds[idx]).id;
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
                
   // }

        }

        }
        Color frameBg = getBgForFrame(frameCmds, {222, 218, 216,255}); // Resolve background color for this frame

        BeginDrawing();
        ClearBackground(frameBg);

        BeginMode2D(cam);

        // draw text command
    for (const auto& t : scene.texts) {
        // pick the frozen font (falls back to current/default safely)
    // int idx = (t.fontIndex >= 0) ? t.fontIndex : g_currentFont;
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
        
        if (!S.tileMenu.empty()) DrawMenuOverlay(S);
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
Audio_Shutdown();
CloseWindow();
return 0;}


/* WINDOWS
.\cmdviewer.exe ..\logs\sample.cmdlog
*/


/* LINUX
cmake -S code -B build
cmake --build build -j
./build/cmdviewer logs/name.cmdfile
*/



/*

*/