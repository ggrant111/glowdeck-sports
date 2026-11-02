#include "DdpStream.h"
#include <WiFi.h>
bool DdpStream::start(const String &ip,int count,gd::SportEvent event,uint32_t color){
    stop();
    if(count<1||count>gd::DDP_MAX_PIXELS||!address_.fromString(ip))return false;
    done_=xSemaphoreCreateBinary();if(!done_)return false;
    count_=count;event_=event;color_=color;stop_=false;
    if(xTaskCreate(run,"gd-ddp",4096,this,1,nullptr)!=pdPASS){vSemaphoreDelete(done_);done_=nullptr;stop_=true;return false;}
    return true;
}
void DdpStream::stop(){
    stop_=true;
    if(done_){xSemaphoreTake(done_,portMAX_DELAY);vSemaphoreDelete(done_);done_=nullptr;}
}
void DdpStream::run(void *arg){
    auto &s=*static_cast<DdpStream*>(arg);WiFiUDP udp;
    uint8_t packet[10+gd::DDP_CHUNK*3];uint8_t seq=1;uint32_t start=millis();
    while(!s.stop_&&WiFi.status()==WL_CONNECTED&&uint32_t(millis()-start)<8000){
        uint32_t elapsed=millis()-start;
        for(int first=0;first<s.count_&&!s.stop_;first+=gd::DDP_CHUNK){
            size_t n=gd::ddpPacket(packet,sizeof(packet),s.event_,s.color_,s.count_,first,elapsed,seq);
            if(!udp.beginPacket(s.address_,4048)||udp.write(packet,n)!=n||!udp.endPacket()){s.stop_=true;break;}
        }
        seq=seq%15+1;vTaskDelay(pdMS_TO_TICKS(50));
    }
    udp.stop();xSemaphoreGive(s.done_);vTaskDelete(nullptr);
}
