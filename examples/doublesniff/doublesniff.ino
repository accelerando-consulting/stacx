#pragma STACX_BOARD espressif:esp32:corinda

#include "variant_pins.h"

#undef HELLO_PIXEL
#undef HELLO_PIN
#include "defaults.h"
#include "config.h"
#include "stacx.h"

//
// Example stack: A bidirectional uart sniffer using two hardware serial ports
//
//

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"

#include "leaf_pixel.h"
#include "abstract_app.h"
#include "leaf_oled.h"
#include "leaf_modbus_slave.h"

#define MODBUS_DEFAULTS "4:3100=1200,4:3101=150,4:310c=2700,4:310d=80,4:311a=69,4:331a=1120,4:331b=10"

class SnurtAppLeaf : public AbstractAppLeaf
{
protected: // configuration preferences

protected: // ephemeral state
  HardwareSerial *portA;
  HardwareSerial *portB;
  
public:
  SnurtAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target)
    , Debuggable(name)
  {
    portA = new HardwareSerial(0);
    portB = new HardwareSerial(1);
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_NOTICE);

    LEAF_NOTICE("Port A receives on D3");
    portA->begin(115200, SERIAL_8N1, 3, -1);

    LEAF_NOTICE("Port B receives on D4");
    portB->begin(115200, SERIAL_8N1, 4, -1);
    
    LEAF_LEAVE;
  }

  virtual void start(void)
  {
    AbstractAppLeaf::start();

    message("pixel", "set/color/0", "red");
  }

  virtual void loop(void)
  {
    Leaf::loop();
    static int dir = 0;
    bool newline=false;
    bool newlineA=false;
    bool newlineB=false;
    bool unprintableA=false;
    bool unprintableB=false;
    
    while (portA->available()) {
      char c = portA->read();
      if (((unprintableA ||(c < 32))/* && (c!='\r') && (c!='\n')*/)) {
	// unprintable characters in red
	Serial.printf("\033[37;41m%02X\033[37;43m", (int)c);
	unprintableA=true;
	if (c=='\n') {
	  unprintableA=false;
	  if (!newlineA) {
	    newlineA=true;
	    Serial.print("\033[39m\033[49m"); // back to default colours at end of line
	    Serial.print("\r\n");
	    dir=0;
	  }
	}
      }
      else {
	// printable characters as is
	if (dir != 'A') {
	  Serial.print("\033[37;43m");
	  dir = 'A';
	}
	Serial.write(c);
      }
      if ((c!='\n') && (c!='\r')) {
	newlineA=false;
      }
      message("pixel", "cmd/blink/0", "orange");
    }
    while (portB->available()) {
      char c = portB->read();
      if ((c=='\r' || c=='\n')) {
	if (dir) {
	  Serial.print("\033[39m\033[49m"); // back to default colours at end of line
	  dir = 0;
	}
	unprintableB=false;
      }
      else if (dir != 'B') {
	Serial.print("\033[37;42m");
	dir = 'B';
      }
      if (c=='\n') {
	if (newline || newlineB) {
	  continue; // skip double newlines
	}
	else {
	  newline = newlineB=true; // remember that the last printed character was a newline
	}
      }
      else {
	if (c!='\r') {
	  newline = newlineB = false;
	}
      }
      if ((unprintableB || (c < 32)) && (c!='\r') && (c!='\n')) {
	// unprintable characters in red
	Serial.printf("\033[37;44m%02X\033[37;42m", (int)c);
	unprintableB=true;
      }
      else {
	// printable characters as is
	Serial.write(c);
      }
      message("pixel", "cmd/blink/0", "green");
    }
  }
};


Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),

  //(new IpEspLeaf("wifi","prefs"))->inhibit(),
  //(new PubsubEspAsyncMQTTLeaf("wifimqtt","prefs,wifi"))->inhibit(),
  (new IpNullLeaf("nullip", "fs")),
  (new PubsubNullLeaf("nullmqtt", "nullip,fs")),

  new     PixelLeaf("pixel",      LEAF_PIN(PIXEL_PIN), PIXEL_COUNT, 0x00FF00),

  new SnurtAppLeaf("app", "fs,pixel"),

  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
