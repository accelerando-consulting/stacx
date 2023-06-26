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
#include "leaf_modbus_master.h"

#ifdef helloPixel
Adafruit_NeoPixel pixels(1, 9, NEO_RGB + NEO_KHZ800); // 1 lights on IO9
Adafruit_NeoPixel *helloPixelSetup() { pixels.begin(); return &pixels; }
#endif

ModbusReadRange *readRanges[] = {
                             /* addr       qty  fc   unit    freq      */
  new ModbusReadRange("epever-volts", 0x310D, 1,   FC_READ_INP_REG,   1,     5000),
  NULL
  
};
  

Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),

  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "ip"),
  //new IpEspLeaf("wifi","prefs"),
  //new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs"),
  new PixelLeaf("pixel",        LEAF_PIN( 5 /* D8 out    */), 4, 0, &pixels),

  new ModbusMasterLeaf("modbus", NO_PINS, readRanges,
		       /*unit=*/1,
		       /*uart=*/1, /*baud=*/115200, /*options=*/SERIAL_8N1,
		       /*rx_pin=*/3, /*tx_pin[*/4,
			/*re_pin=*/1, /*de_pin=*/2),

  NULL
};
