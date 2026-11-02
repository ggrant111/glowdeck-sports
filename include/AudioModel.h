#pragma once
#include <stdint.h>
#include <stddef.h>
namespace gd {
enum class SoundKind : uint8_t { Off=0, Horn=1, DoubleHorn=2, Siren=3, CustomWav=4 };
struct AudioChoice {
    SoundKind kind = SoundKind::Horn;
    char clip[32] = {}; // filename only, without path; uppercase A-Z, digits, _ and -
};
struct WavInfo {
    bool valid=false;
    uint32_t dataBytes=0;
    uint32_t durationMs=0;
};
SoundKind validatedSound(uint8_t raw);
bool validClipName(const char* clip);
WavInfo inspectCanonicalWav(const uint8_t* header, size_t length, uint32_t actualFileBytes);
bool validVolume(int v);
}
