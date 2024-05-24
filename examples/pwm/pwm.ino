#include "defaults.h"
#include "config.h"
#include "stacx.h"

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_shell.h"
#include "leaf_wire.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_pwm.h"

#include "abstract_app.h"

#ifdef ARDUINO_TTGO_T7_V13_Mini32
#define D3 17
#endif

#ifdef USE_HELLO_PIXEL
Adafruit_NeoPixel pixels(1, HELLO_PIXEL, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel *helloPixelSetup() { pixels.begin(); return &pixels; }
#endif


class PwmAppLeaf : public AbstractAppLeaf
{
protected:
  bool ena = false;
  unsigned long freq=10000;
  float duty=0.5;
  
public:
  PwmAppLeaf(String name, String target)
    : AbstractAppLeaf(name, target)
    , Debuggable(name) {


    }

  virtual void setup(void) {
    AbstractAppLeaf::setup();

    registerCommand(HERE, "run", "start the pwm");
    registerCommand(HERE, "stop", "stop the pwm");
    registerCommand(HERE, "freq", "set the pwm frequency");
    registerCommand(HERE, "duty", "set the pwm duty");
  }

  virtual void start(void) {
    AbstractAppLeaf::start();

    message("pwm", "set/pwm_freq", String(freq));
    message("pwm", "set/pwm_duty", String(duty));
    message("pwm", "set/pwm_state", ABILITY(ena));
  }

  virtual void stop() {
    message("pwm", "cmd/pwm_off");
    AbstractAppLeaf::stop();
  }

  void cmd_run() {
    message("pwm", "cmd/pwm_on");
  }
  void cmd_stop() {
    message("pwm", "cmd/pwm_off");
  }
  void cmd_freq(String payload) {
    freq = payload.toInt();
    message("pwm", "set/pwm_freq", String(freq));
  }
  void cmd_duty(String payload) {
    duty = payload.toFloat();
    message("pwm", "set/pwm_duty", String(duty));
  }
  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN(    "run", cmd_run())
    ELSEWHEN("stop", cmd_stop())
    ELSEWHEN("freq", cmd_freq(payload))
    ELSEWHEN("duty", cmd_duty(payload))
    else {
      handled = AbstractAppLeaf::commandHandler(type, name, topic, payload);
    }

    LEAF_HANDLER_END;
  }


};



Leaf *leaves[] = {
  new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),
  //new WireBusLeaf("wire"),
  //new OledLeaf("screen"),

  (new IpEspLeaf("wifi","fs"))->setTrace(L_NOTICE),
  new PubsubEspAsyncMQTTLeaf(
    "wifimqtt","wifi,fs",
    PUBSUB_SSL_DISABLE,
    PUBSUB_DEVICE_TOPIC_DISABLE
    ),

  (new PWMLeaf("pwm", LEAF_PIN(D3)))->setTrace(L_INFO),

  (new PwmAppLeaf("app","fs,screen,pwm"))->setTrace(L_NOTICE),
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
