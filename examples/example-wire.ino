#include "config.h"
#include "stacx.h"


//
// Example stack: A setup for the EasyThree IO board
//
// You should define your stack here (or you can incorporate stacx as a git 
// submodule in your project by including stacx/stacx.h
//

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"

#include "leaf_wire.h"
#include "leaf_button.h"
#include "leaf_tone.h"
#include "leaf_actuator.h"
#include "leaf_pixel.h"
#include "leaf_pinextender_pcf8574.h"
#include "abstract_app.h"
#include "leaf_oled.h"

#ifdef helloPixel
Adafruit_NeoPixel pixels(1, 9, NEO_RGB + NEO_KHZ800); // 1 lights on IO9

Adafruit_NeoPixel *helloPixelSetup() 
{
#if EARLY_SERIAL
  Serial.printf("%d Start boot animation\n", (int)millis());
#endif  
  pixels.begin();

  pixels.setBrightness(255);
  pixels.clear();
  pixels.show();

  // rotate the pixels through RGBW 8 times, shifting the pattern each time
  int count = pixels.numPixels();
  for (int cycle=0; cycle<8; cycle++) {
    for (int pixel=0; pixel<count; pixel++) {
      pixels.clear();
      switch (cycle%4) {
      case 0:
	pixels.setPixelColor(pixel, pixels.Color(255,0,0));
	break;
      case 1:
	pixels.setPixelColor(pixel, pixels.Color(0,255,0));
	break;
      case 2:
	pixels.setPixelColor(pixel, pixels.Color(0,0,255));
	break;
      case 3:
	pixels.setPixelColor(pixel, pixels.Color(255,255,255));
	break;
      }
      pixels.show();
      delay(50);
    }
    pixels.clear();
    pixels.show();
  }
#if EARLY_SERIAL
  Serial.printf("%d Finish boot animation\n", (int)millis());
#endif  
  return &pixels;
}
#endif

Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),

  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "ip"),
  //new IpEspLeaf("wifi","prefs"),
  //new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs"),
  
#ifdef ARDUINO_ESP32C3_DEV 
  new WireBusLeaf("wire",       /*sda=*/2, /*scl=*/1),
#else
  new WireBusLeaf("wire",       /*sda=*/21, /*scl=*/22),
#endif
#ifdef helloPixel
  new PixelLeaf("pixel",        LEAF_PIN( 5 /* D8 out    */), 4, 0, &pixels),
#endif

  NULL
};
