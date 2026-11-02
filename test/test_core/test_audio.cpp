#include <unity.h>
#include "AudioModel.h"
#include "GoalEngine.h"
#include <cstring>
static void little16(uint8_t *p,unsigned n){p[0]=uint8_t(n);p[1]=uint8_t(n>>8);}
static void little32(uint8_t *p,unsigned n){for(unsigned i=0;i<4;i++)p[i]=uint8_t(n>>(8*i));}
static void header(uint8_t* p,uint32_t samples){
    std::memset(p,0,44);std::memcpy(p,"RIFF",4);little32(p+4,36+samples*2);
    std::memcpy(p+8,"WAVEfmt ",8);little32(p+16,16);little16(p+20,1);
    little16(p+22,1);little32(p+24,16000);little32(p+28,32000);
    little16(p+32,2);little16(p+34,16);std::memcpy(p+36,"data",4);
    little32(p+40,samples*2);
}
void test_audio_kind_and_clip_names(){
    TEST_ASSERT_EQUAL_INT(4,(int)gd::validatedSound(4));
    TEST_ASSERT_EQUAL_INT(0,(int)gd::validatedSound(255));
    TEST_ASSERT_TRUE(gd::validClipName("ANA_GOAL"));
    TEST_ASSERT_TRUE(gd::validClipName("BOS-1"));
    TEST_ASSERT_FALSE(gd::validClipName("../secrets"));
    TEST_ASSERT_FALSE(gd::validClipName("goal.wav"));
    TEST_ASSERT_FALSE(gd::validClipName(""));
    TEST_ASSERT_FALSE(gd::validClipName("AAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
}
void test_audio_wav_header_valid(){
    uint8_t h[44];header(h,16000);
    auto x=gd::inspectCanonicalWav(h,44,32044);
    TEST_ASSERT_TRUE(x.valid);TEST_ASSERT_EQUAL_UINT32(1000,x.durationMs);
    TEST_ASSERT_EQUAL_UINT32(32000,x.dataBytes);
}
void test_audio_wav_reject_non_pcm_or_stereo(){
    uint8_t h[44];header(h,16000);little16(h+20,3);
    TEST_ASSERT_FALSE(gd::inspectCanonicalWav(h,44,32044).valid);
    header(h,16000);little16(h+22,2);
    TEST_ASSERT_FALSE(gd::inspectCanonicalWav(h,44,32044).valid);
    header(h,16000);little32(h+24,44100);
    TEST_ASSERT_FALSE(gd::inspectCanonicalWav(h,44,32044).valid);
}
void test_audio_wav_truncated_and_oversized(){
    uint8_t h[44];header(h,16000);
    TEST_ASSERT_FALSE(gd::inspectCanonicalWav(h,40,32044).valid);
    TEST_ASSERT_FALSE(gd::inspectCanonicalWav(h,44,200).valid);
    header(h,160001);
    TEST_ASSERT_FALSE(gd::inspectCanonicalWav(h,44,320046).valid);
}
void test_audio_volume_limits(){
    TEST_ASSERT_TRUE(gd::validVolume(0));
    TEST_ASSERT_TRUE(gd::validVolume(50));
    TEST_ASSERT_TRUE(gd::validVolume(100));
    TEST_ASSERT_FALSE(gd::validVolume(-1));
    TEST_ASSERT_FALSE(gd::validVolume(101));
}
