// PlatformIO entry point; keep the upstream Arduino sketch as the application source.
#include <Arduino.h>
#define setup upstreamSetup
#include "../ESP32-CAM_MJPEG2SD.ino"
#undef setup

void setup() {

}