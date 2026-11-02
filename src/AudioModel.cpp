#include "AudioModel.h"
#include <cstring>
namespace gd {
SoundKind validatedSound(uint8_t raw) { return raw<=4 ? static_cast<SoundKind>(raw) : SoundKind::Off; }
bool validClipName(const char* name) {
    if (!name) return false;
    size_t n=0;
    while(name[n] && n<32) {
        const char c=name[n];
        if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
        ++n;
    }
    return n>0 && n<=24 && name[n]=='\0';
}
static uint16_t u16(const uint8_t*p) {return (uint16_t)p[0]|(uint16_t(p[1])<<8);}
static uint32_t u32(const uint8_t*p) {return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
WavInfo inspectCanonicalWav(const uint8_t* p,size_t n,uint32_t size) {
    WavInfo out;
    // Restrict first release to canonical RIFF/PCM mono/16k/16-bit WAV.
    if(!p||n<44||size<44||std::memcmp(p,"RIFF",4)||std::memcmp(p+8,"WAVEfmt ",8)||std::memcmp(p+36,"data",4))return out;
    if(u32(p+16)!=16||u16(p+20)!=1||u16(p+22)!=1||u32(p+24)!=16000||
       u32(p+28)!=32000||u16(p+32)!=2||u16(p+34)!=16) return out;
    uint32_t bytes=u32(p+40);
    if(!bytes||bytes>320000||bytes%2||bytes>size-44)return out;
    out.valid=true;out.dataBytes=bytes;out.durationMs=bytes/32;
    return out;
}
bool validVolume(int v){return v>=0 && v<=100;}
}
