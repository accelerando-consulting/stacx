#pragma STACX_BOARD espressif:esp32:ttgo-t7-v13-mini32
#define HELLO_PIXEL 0
#define PIXEL_BLINK false

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

#ifdef USE_HELLO_PIXEL
Adafruit_NeoPixel pixels(4, 5, NEO_GRB + NEO_KHZ800); // 4 lights on IO5

Adafruit_NeoPixel *helloPixelSetup()
{
  pixels.begin();
  pixels.setBrightness(255);
  pixels.clear();
  pixels.show();
  return &pixels;
}
#endif


#define MODBUS_DEFAULTS "4:3100=1200,4:3101=150,4:310c=2700,4:310d=80,4:311a=69,4:331a=1120,4:331b=10"

class TesterAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  bool state = false;
  unsigned long last_press=0;

public:
  TesterAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target)
    , Debuggable(name)
  {
    // default variables or constructor argument processing goes here
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_INFO);
    LEAF_LEAVE;
  }

  virtual void start(void)
  {
    AbstractAppLeaf::start();
    message("speaker","set/tempo", "240");
    message("speaker","cmd/tune", "G4 A4 F4:2 R F3 C4:2");

    message("pixel", "set/flash", "500");
    message("pixel", "set/color/1", "red");
    message("pixel", "set/color/2", "red");
    message("pixel", "set/color/3", "red");
  }

  virtual void loop(void)
  {
    Leaf::loop();
  }

  virtual void status_pub()
    {
      publish("state", ABILITY(state));
    }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false)
  {
    LEAF_HANDLER(L_DEBUG);

    LEAF_INFO("RECV %s/%s %s <= [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("set/state", {
      state = parseBool(payload));
    }
    ELSEWHENFROM("sw1", "event/press", {
      LEAF_NOTICE("bzzt");
      message("rumble", "cmd/oneshot", "100");
      message("pixel", "set/color/1", "red");
      message("pixel", "set/color/2", "red");
      message("pixel", "set/color/3", "red");
    })
    ELSEWHENFROM("sw2", "event/press", {
      LEAF_NOTICE("beep");
      message("rumble", "cmd/oneshot", "100");
      message("speaker", "cmd/tone", "220,100");
      message("registers", "cmd/load", "/modbus_defaults.json");
    })
    ELSEWHENEITHER("event/release","status/button",{})
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload);
      if (!handled) {
	LEAF_INFO("app did not consume type=%s name=%s topic=%s payload=%s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
      }
    }

    LEAF_HANDLER_END;
  }

};

class ModbusTestSlaveLeaf : public ModbusSlaveLeaf
{
public:
  ModbusTestSlaveLeaf(String name, String target, int unit=1, Stream *port=NULL,
		      int uart=-1, int baud = 115200, int options = SERIAL_8N1, int bus_rx_pin=-1, int bus_tx_pin=-1, int bus_nre_pin=-1,  int bus_de_pin=-1)
    : ModbusSlaveLeaf(name, target, unit, port, uart, baud, options, bus_rx_pin, bus_tx_pin, bus_nre_pin, bus_de_pin)
    , Debuggable(name, L_NOTICE)
    {
    }

  const char *fc_name(uint8_t code)
    {
      char buf[5];
      switch (code) {
      case FC_READ_COILS:
	return "FC_READ_COILS";
      case FC_READ_DISCRETE_INPUT:
	return "FC_READ_DISCRETE_INPUT";
      case FC_READ_HOLDING_REGISTERS:
	return "FC_READ_HOLDING_REGISTERS";
      case FC_READ_INPUT_REGISTERS:
	return "FC_READ_INPUT_REGISTERS";
      case FC_READ_EXCEPTION_STATUS:
	return "FC_READ_EXCEPTION_STATUS";
      case FC_WRITE_COIL:
	return "FC_WRITE_COIL";
      case FC_WRITE_MULTIPLE_COILS:
	return "FC_WRITE_MULTIPLE_COILS";
      case FC_WRITE_REGISTER:
	return "FC_WRITE_REGISTER";
      case FC_WRITE_MULTIPLE_REGISTERS:
	return "FC_WRITE_MULTIPLE_REGISTERS";
      default:
	snprintf(buf, sizeof(buf), "0x%02x", code);
	return buf;
      }
    }


  virtual void onCallback(uint8_t code, uint16_t addr, uint16_t len)
  {
    LEAF_NOTICE("Modbus FC %02x (%s) addr=0x%04x count=%d", (int)code, fc_name(code), (int)addr, len);
    switch (code) {
    case FC_READ_COILS:
    case FC_READ_DISCRETE_INPUT:
    case FC_READ_HOLDING_REGISTERS:
    case FC_READ_INPUT_REGISTERS:
    case FC_READ_EXCEPTION_STATUS:
      message("pixel", "set/color/1", "green");
      message("pixel", "cmd/flash/1", "orange");
      break;
    case FC_WRITE_COIL:
    case FC_WRITE_MULTIPLE_COILS:
      message("pixel", "set/color/1", "green");
      message("pixel", "cmd/flash/1", "blue");
      break;
    }
    if (addr == 0x3100) {
      message("pixel", "set/color/2", "green");
    }
    if (addr == 0x310D) {
      message("pixel", "set/color/3", "green");
    }
  }
};


Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),

  (new IpEspLeaf("wifi","prefs"))->inhibit(),
  (new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs,wifi"))->inhibit(),
  (new IpNullLeaf("nullip", "fs"))->inhibit(),
  (new PubsubNullLeaf("nullmqtt", "nullip,fs"))->inhibit(),

  // Peripherals on the EasyThree Leaf
  //new  WireBusLeaf("wire",       /*sda=*/21, /*scl=*/22),
  (new   ButtonLeaf("sw1",        LEAF_PIN( 26 /* D0  IN */), LOW))->setMute(),
  (new   ButtonLeaf("sw2",        LEAF_PIN( 18 /* D5  IN */), LOW))->setMute(),
  new      ToneLeaf("speaker",    LEAF_PIN( 19 /* D6 OUT */), 800, 100),
  (new ActuatorLeaf("rumble","",  LEAF_PIN( 23 /* D7 OUT */)))->setMute(),
  new     PixelLeaf("pixel",      LEAF_PIN(  5 /* D8 OUT */), 4, 0, &pixels),

  // Modbus Leaf
  //(new StorageLeaf("modbus_registers"))->setTrace(L_DEBUG)->setMute(),
  (new FSPreferencesLeaf("registers",MODBUS_DEFAULTS,"/modbus.json"))->setTrace(L_NOTICE)->setMute(),
  (new ModbusTestSlaveLeaf("modbus", "registers,pixel", 1, NULL, 2, 115200, SERIAL_8N1, /*rx=*/D3, /*tx=*/D4, /*nre=*/D1, /*de=*/D2))->setMute()->setTrace(L_INFO),

  new TesterAppLeaf("app", "fs,wifi,wifimqtt,wire,sw1,sw2,speaker,rumble,pixel,modbus,registers"),


  NULL
};
