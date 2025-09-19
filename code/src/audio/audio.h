#pragma once
#include <string>

// Lifecycle
void Audio_Init();
void Audio_Shutdown();


void Audio_PlaySfx(const std::string& path, float volume = 1.0f);

// Loops
void Audio_PlayMusic(const std::string& path, bool loop = true, float volume = 1.0f);
void Audio_StopMusic();

// Global controls
void Audio_StopAll();
void Audio_SetMasterVolume(float v);
void Audio_SetMusicVolume(float v);
