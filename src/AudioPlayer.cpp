#include "AudioPlayer.h"
#include <driver/i2s.h>
#include <SD.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
namespace {
constexpr i2s_port_t PORT=I2S_NUM_0;
constexpr int SAMPLE_RATE=16000;
constexpr int SD_CS=5;
// DAC2 is GPIO26 (built-in amplifier on certain CYD variants).
static uint16_t dacSample(int16_t signedPcm,uint8_t volume){
    int adjusted=(int(signedPcm)*int(volume))/100;
    int v=(adjusted>>8)+128;
    if(v<0)v=0;if(v>255)v=255;
    return uint16_t(v<<8);
}
static bool sendSamples(const uint16_t *samples,size_t n) {
    size_t bytes=0;
    return i2s_write(PORT,samples,n*sizeof(uint16_t),&bytes,pdMS_TO_TICKS(200))==ESP_OK &&
           bytes==n*sizeof(uint16_t);
}
}
bool AudioPlayer::begin() {
    const i2s_config_t cfg = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN),
      .sample_rate=SAMPLE_RATE,
      .bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format=I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format=I2S_COMM_FORMAT_STAND_MSB,
      .intr_alloc_flags=0, .dma_buf_count=4, .dma_buf_len=256,
      .use_apll=false, .tx_desc_auto_clear=true, .fixed_mclk=0
    };
    if(i2s_driver_install(PORT,&cfg,0,nullptr)!=ESP_OK) {error_="I2S driver failed";return false;}
    if(i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)!=ESP_OK) {error_="DAC channel failed";return false;}
    if(xTaskCreatePinnedToCore(taskThunk,"GameDayAudio",6144,this,1,&task_,1)!=pdPASS) {
        error_="Audio task allocation failed";i2s_driver_uninstall(PORT);return false;
    }
    return true;
}
void AudioPlayer::stop() {cancel_=true;}
bool AudioPlayer::play(gd::AudioChoice choice,uint8_t vol) {
    if(busy_||!task_||choice.kind==gd::SoundKind::Off||vol==0)return false;
    if(choice.kind==gd::SoundKind::CustomWav&&!gd::validClipName(choice.clip)) {
        error_="Invalid custom audio name";return false;
    }
    request_=choice;requestVolume_=vol>70?70:vol;cancel_=false;busy_=true;
    xTaskNotifyGive(task_);
    return true;
}
void AudioPlayer::taskThunk(void* p) {static_cast<AudioPlayer*>(p)->taskLoop();}
void AudioPlayer::taskLoop() {
    for(;;) {
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        run(request_,requestVolume_);
        busy_=false;
    }
}
void AudioPlayer::run(gd::AudioChoice choice,uint8_t vol) {
    error_="";
    i2s_zero_dma_buffer(PORT);
    if(choice.kind==gd::SoundKind::CustomWav) {
        if(!custom(choice.clip,vol) && !cancel_) {
            // Missing/corrupt custom file -> built-in safe fallback.
            preset(gd::SoundKind::Horn,vol);
        }
    } else preset(choice.kind,vol);
    uint16_t silence[128];
    for(auto &v:silence)v=0x8000;
    sendSamples(silence,128);
    i2s_zero_dma_buffer(PORT);
}
void AudioPlayer::preset(gd::SoundKind kind,uint8_t vol) {
    if(kind==gd::SoundKind::Off)return;
    const uint32_t duration=kind==gd::SoundKind::Siren?1700:kind==gd::SoundKind::DoubleHorn?2100:2400;
    uint16_t buffer[128];
    const uint32_t count=duration*16U;
    for(uint32_t i=0;i<count && !cancel_;) {
        size_t take=(count-i)>128?128:(count-i);
        for(size_t j=0;j<take;j++,i++){
            float seconds=float(i)/SAMPLE_RATE;
            float f=kind==gd::SoundKind::Horn ? 196.0f :
              kind==gd::SoundKind::DoubleHorn ? (seconds<1.0f?247.0f:294.0f) :
              300.0f+140.0f*sinf(seconds*9.0f);
            float envelope=(seconds<0.12f?seconds/0.12f:1.0f)*
                ((count-i)<3200?float(count-i)/3200.0f:1.0f);
            float wave=0.65f*sinf(6.2831853f*f*seconds)
                +0.25f*sinf(6.2831853f*f*2.0f*seconds);
            int32_t value=int32_t(9000.0f*wave*envelope);
            buffer[j]=dacSample(int16_t(value),vol);
        }
        if(!sendSamples(buffer,take)){error_="DAC output interrupted";break;}
    }
}
bool AudioPlayer::custom(const char* name,uint8_t vol) {
    // This CYD routes touch and SD through one VSPI host on DIFFERENT pins.
    // The foreground firmware must suspend touch during a custom clip;
    // SD pins are reassigned for the duration of this task.
    sdSession_=true;
    SPIClass sdSpi(VSPI);
    sdSpi.begin(18,19,23,SD_CS);
    bool mounted=SD.begin(SD_CS,sdSpi,4000000);
    if(!mounted){error_="SD card missing";sdSpi.end();sdSession_=false;return false;}
    String path="/audio/"+String(name)+".wav";
    File file=SD.open(path.c_str(),FILE_READ);
    if(!file){error_="WAV not found";SD.end();sdSpi.end();sdSession_=false;return false;}
    uint8_t header[44];
    const bool got=file.read(header,sizeof(header))==sizeof(header);
    gd::WavInfo info=gd::inspectCanonicalWav(header,sizeof(header),file.size());
    if(!got||!info.valid){error_="WAV must be mono 16k 16-bit PCM (<=10s)";}
    else {
        uint32_t remaining=info.dataBytes;
        uint8_t pcm[256];uint16_t out[128];
        while(remaining && !cancel_) {
            size_t target=remaining>sizeof(pcm)?sizeof(pcm):remaining;
            size_t actual=file.read(pcm,target);
            if(actual!=target){error_="WAV read failed";break;}
            for(size_t i=0;i<actual/2;i++) {
                uint16_t raw=uint16_t(pcm[i*2])|(uint16_t(pcm[i*2+1])<<8);
                out[i]=dacSample(int16_t(raw),vol);
            }
            if(!sendSamples(out,actual/2)){error_="DAC stream interrupted";break;}
            remaining-=actual;
        }
        if(cancel_)error_="Playback stopped";
        else if(!remaining)error_="";
    }
    file.close();SD.end();sdSpi.end();sdSession_=false;
    return error_.isEmpty();
}
