#pragma STACX_BOARD espressif:esp32:ttgo-t7-v13-mini32

#include "variant_pins.h"
#include "defaults.h"
#include "config.h"
#include "stacx.h"

//
// Example stack: A modbus central (fka m*st*r) device
//
//
#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_modbus_master.h"

ModbusReadRange *readRanges[] = {
                                    /* addr  qty  fc                     unit   freq      */
  new ModbusReadRange("XY-MD02",     0x0001, 2,   FC_READ_INP_REG,       1,     5000),
  new ModbusReadRange("XY-MD02-cfg", 0x0101, 4,   FC_READ_HOLD_REG,      1,     MODBUS_POLL_ONCE),
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

  (new ModbusMasterLeaf("modbus", NO_PINS, readRanges,
			/*unit=*/1,
			/*uart=*/2, /*baud=*/9600, /*options=*/SERIAL_8N1,
			/*rx_pin=*/D3, /*tx_pin=*/D4,
			/*re_pin=*/D1, /*de_pin=*/D2))->setMute()->setTrace(L_INFO),
  

  NULL
};
