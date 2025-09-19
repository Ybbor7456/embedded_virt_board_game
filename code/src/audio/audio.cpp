#define MINIAUDIO_IMPLEMENTATION


#include <miniaudio.h>
#include <mutex>
#include <string>
#include <atomic>
#include <cstdio>

#include "audio/audio.h"

// ---- Globals
static ma_engine        g_engine{};
static std::mutex       g_mutex;           // protect music object
static ma_sound         g_music{};         // persistent music player
static std::atomic_bool g_hasMusic{false}; // tracks init state of g_music
static std::atomic_bool g_inited{false};    // tacks init state

void Audio_Init() {
    if (g_inited.exchange(true)) return; // already inited

    ma_result res = ma_engine_init(nullptr, &g_engine);
    if (res != MA_SUCCESS) {
        g_inited = false;
        std::fprintf(stderr, "miniaudio: ma_engine_init failed (%d)\n", res);
        return;
    }
   
    ma_engine_set_volume(&g_engine, 1.0f);
}


static void destroyMusic_NoLock() {
    if (g_hasMusic.load(std::memory_order_acquire)) {       // Caller must hold g_mutex; only the flag is atomic.
        ma_sound_uninit(&g_music);
        g_hasMusic.store(false, std::memory_order_release);
    }
}

void Audio_Shutdown() {                     
    if (!g_inited.exchange(false)) return; // swaps bool value of g_inited(), shutdown implies it has already been initialized. 
                                                                        //memory_order_acq_rel acquire and release
    {
        std::lock_guard<std::mutex> lk(g_mutex);    // RAII object, locks mutex in construciton and releases during deconstruction
        destroyMusic_NoLock();                      // call to prev function that avoids double locking
    }
    ma_engine_uninit(&g_engine);
}

void Audio_PlaySfx(const std::string& path , float volume) {
    if (!g_inited.load()) return;
    ma_result res = ma_engine_play_sound(&g_engine, path.c_str(), nullptr);
}


void Audio_PlayMusic(const std::string& path, bool loop, float volume) {
    if (!g_inited.load()) return;                  // if not intied, return empty

    std::lock_guard<std::mutex> lk(g_mutex);    

    // Tear down any previous track.
    destroyMusic_NoLock();

    ma_result res = ma_sound_init_from_file(&g_engine, path.c_str(),MA_SOUND_FLAG_STREAM, nullptr, nullptr, &g_music);
    if (res != MA_SUCCESS) {
        std::fprintf(stderr, "miniaudio: failed to open music '%s' (%d)\n", path.c_str(), res);
        return;
    }

    ma_sound_set_looping(&g_music, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&g_music, volume);

    res = ma_sound_start(&g_music);
    if (res != MA_SUCCESS) {
        std::fprintf(stderr, "miniaudio: failed to start music '%s' (%d)\n",
                     path.c_str(), res);
        ma_sound_uninit(&g_music);
        return;
    }

    g_hasMusic.store(true, std::memory_order_release);
}


void Audio_StopMusic() { // doesnt stop SFX

    if (!g_inited.load()) return;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_hasMusic.load(std::memory_order_acquire)) { ma_sound_stop(&g_music); } // stops rather than uninits. g_hasMusic is still = true
}

void Audio_StopAll() {      // stops all sound
    if (!g_inited.load()) return;

    // Stop fire-and-forget sounds:
    ma_engine_stop(&g_engine); // stops engine (all)

    // Stop music:
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_hasMusic.load(std::memory_order_acquire)) {
        ma_sound_stop(&g_music);
    }
}

void Audio_SetMusicVolume(float v) {                // lets u change the volume later on comapred to Audio_PlayMusic
// only controls current g_music, g_music is shared state and requires mutex. 
    if (!g_inited.load()) return;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_hasMusic.load(std::memory_order_acquire)) { ma_sound_set_volume(&g_music, v); }
}

void Audio_SetMasterVolume(float v) {       // controls all sound, no mutex required since engine api is thread safe
    if (!g_inited.load()) return;
    if (v < 0.f) v = 0.f;
    if (v > 1.f) v = 1.f;
    ma_engine_set_volume(&g_engine, v);
}

