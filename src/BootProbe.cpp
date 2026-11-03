#ifdef GD_BOOT_PROBE
#include <Arduino.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <rom/ets_sys.h>
// No display, Wi-Fi, saved preferences, NHL requests or lighting commands.
__attribute__((constructor(101))) static void probeEarly(){ets_printf("\nGD-PROBE: application constructors reached\n");}
void setup(){
    Serial.begin(115200);delay(250);
    Serial.printf("GD-PROBE: setup reached; reset=%d heap=%u largest=%u\n",int(esp_reset_reason()),unsigned(ESP.getFreeHeap()),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}
void loop(){Serial.printf("GD-PROBE: alive uptime=%lu heap=%u\n",(unsigned long)millis(),unsigned(ESP.getFreeHeap()));delay(1000);}
#endif
