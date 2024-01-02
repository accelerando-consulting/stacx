#include "defaults.h"
#include "config.h"
#include "stacx.h"
#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_ip_null.h"
#include "leaf_joyofclicks.h"
#include "leaf_lt3966.h"
#include "leaf_oled.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_pubsub_null.h"
#include "leaf_pixel.h"
#include "leaf_shell.h"
#include "leaf_wire.h"

#ifndef PIXEL_COUNT
#define PIXEL_COUNT 1
#endif
#ifndef PIXEL_PIN
#define PIXEL_PIN 9
#endif

#ifdef USE_HELLO_PIXEL
Adafruit_NeoPixel pixels(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel *helloPixelSetup() { pixels.begin(); return &pixels; }
#endif

Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell"),
  
  new IpNullLeaf("nullip","fs"),
  new PubsubNullLeaf("nullmqtt","nullip,fs"),

  new WireBusLeaf("wire", 2, 1),
#if USE_OLED
  new OledLeaf("oled"),
  (new JoyOfClicksLeaf("joyhat",0x20,JoyOfClicksLeaf::ORIENTATION_CUSTOM,"center,left,up,right,down"))->setMute(),
#endif
  new PixelLeaf(
    "pixels",
    LEAF_PIN(PIXEL_PIN),
    PIXEL_COUNT,
    Adafruit_NeoPixel::Color(64,0,0),
    &pixels
    ),
  new LT3966Leaf("lamps"),

  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
