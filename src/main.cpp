// PlatformIO entry point; keep the upstream Arduino sketch as the application source.
#include <Arduino.h>
#define setup upstreamSetup
#include "../ESP32-CAM_MJPEG2SD.ino"
#undef setup

void setup() {
    Serial.begin(115200);

    Serial.println();
    Serial.println("=== Waiting 10 seconds before startup ===");
    delay(10000);

    upstreamSetup();
}