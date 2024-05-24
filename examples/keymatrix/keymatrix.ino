#include "defaults.h"
#include "stacx.h"

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_shell.h"

#include "abstract_app.h"
#include "leaf_wire.h"
#include "leaf_keymatrix_pcf8574.h"
#include "leaf_power.h"
#if USE_OLED
#include "leaf_oled.h"
#endif

#ifdef USE_HELLO_PIXEL
#include "leaf_pixel.h"
#define PIXEL_COUNT 1
#define PIXEL_PIN 9
Adafruit_NeoPixel pixels(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel *helloPixelSetup() {
  pixels.begin();
  return &pixels;
}
#endif

// This example presumes the Accelerando "Molly" 4x4 matrix keypad, driven via PCF8574

class MollyAppLeaf : public AbstractAppLeaf 
{
public:
  MollyAppLeaf(String name, String targets)
    : AbstractAppLeaf(name, targets)
    , Debuggable(name)
  {};

};
  

Leaf *leaves[] = {
        new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
	new FSPreferencesLeaf("prefs"),
	new PowerLeaf("power"),
	new ShellLeaf("shell"),

	new IpNullLeaf("nullip", "fs"),
	new PubsubNullLeaf("nullmqtt", "nullip"),


#ifdef USE_HELLO_PIXEL
	new PixelLeaf("pixel", LEAF_PIN(PIXEL_PIN), PIXEL_COUNT,  0, &pixels),
#endif
	new WireBusLeaf("wire", /*sda=*/SDA, /*scl=*/SCL),
	(new KeyMatrixPCF8574Leaf("matrix",
				  0x20, 4, 4, "",
				  KeyMatrixPCF8574Leaf::STROBE_IS_MSB,
				  KeyMatrixPCF8574Leaf::SCAN_BY_ROW)
	  )->setTrace(L_INFO),
#if USE_OLED
	new OledLeaf("screen", 0x3c),
#endif

	new MollyAppLeaf("app", "fs,prefs,matrix"),
	
	NULL
};
