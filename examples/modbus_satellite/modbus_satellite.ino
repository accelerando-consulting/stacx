#pragma STACX_BOARD espressif:esp32:ttgo-t-oi-plus
#include "variant_pins.h"
#include "defaults.h"
#include "config.h"
#include "stacx.h"

//
// Example stack: A modbus satellite (aka sl*v*) device
//
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
#include "leaf_modbus_slave.h"

#define MODBUS_DEFAULTS "4:3100=1200,4:3101=150,4:310C=2700,4:310D=80,4:311A=69,4:331A=1120,4:331B=10"

Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),

  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "ip"),
  //new IpEspLeaf("wifi","prefs"),
  //new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs"),

  //(new StorageLeaf("modbus_registers"))->setTrace(L_DEBUG)->setMute(),
  (new StorageLeaf("modbus_registers",MODBUS_DEFAULTS))->setTrace(L_DEBUG)->setMute(),
  (new ModbusSlaveLeaf("modbus", "registers=modbus_registers", 1, NULL, 1, 9600, SERIAL_8N1, /*rx=*/D3, /*tx=*/D4, /*nre=*/D0, /*de=*/A0))->setMute()->setTrace(L_INFO),

  NULL
};
