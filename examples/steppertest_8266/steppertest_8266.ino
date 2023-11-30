#pragma STACX_BOARD esp8266:esp8266:d1_mini_pro
//# pragma STACX_BOARD esp8266:esp8266:wifi_kit_8

#undef LED_BUILTIN
#undef HELLO_PIXEL
#define HELLO_PIN 16

#include "defaults.h"
#include "config.h"
#include "stacx.h"

//
// Example stack: A modbus satellite (aka sl*v*) device
//
//

#include "leaf_fs.h"
#include "leaf_fs_preferences.h"
#include "leaf_shell.h"
#include "leaf_oled.h"
#include "leaf_wire.h"
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#if APP_USE_WIFI
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#endif
#include "leaf_pwm.h"
#include "leaf_actuator.h"

#include "abstract_app.h"

#include <TM1638plus.h>

TM1638plus lk(D1, D2, D3, true);

#define USE_PWM 0

#define LED_ENA 0
#define LED_DIR 1
#define LED_RUN 2
#define LED_STP 3

class StepperAppLeaf : public AbstractAppLeaf
{
protected:
  int spm = 120;
  bool ena = false;
  int step_us = 10;
  float step_duty = 0.5;
  bool dir_fwd = true;
  bool run = false;
  OledLeaf *screen=NULL;

public:
  StepperAppLeaf(String name, String target)
    : AbstractAppLeaf(name, target)
    , Debuggable(name) {


    }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    lk.displayBegin();
    screen = (OledLeaf *)get_tap("screen");
    app_display_msec = 5000;
    registerCommand(HERE, "run", "start the pwm");
    registerCommand(HERE, "stop", "stop the pwm");
    registerCommand(HERE, "fwd", "set direction forward");
    registerCommand(HERE, "rev", "set direction reverse");
    registerCommand(HERE, "ena", "enable motor driver");
    registerCommand(HERE, "dis", "disable motor driver");
    registerCommand(HERE, "spd", "set speed in current direction");
    registerCommand(HERE, "stp", "make one step pulse");
    registerCommand(HERE, "fst", "go faster");
    registerCommand(HERE, "slw", "go slower");
  }

  virtual void start(void) {
    AbstractAppLeaf::start();
    lk.displayText("helowrld");
    delay(1000);
    lk.displayText("        ");

    message("dir", "set/actuator", String(dir_fwd?1:0));
    message("ena", "set/actuator", String(run?1:0));
#if USE_PWM
    message("stp", "set/duty", String(step_duty));
#endif
    speed(-1);
  }

  virtual void stop() {
    message("stp", "set/duty", "0");
    AbstractAppLeaf::stop();
  }


  void speed(int spm_new=-1) {
    if (spm_new == 0) {
      message("stp", "set/duty", "0");
      spm = 0;
    }
    else {
      if (spm_new > 0) {
	spm = spm_new;
      }
      String newfreq = String(spm/60.0);
      LEAF_NOTICE("set/freq %s", newfreq.c_str());
      message("stp", "set/freq", newfreq);
    }
    lk.displayHex(4, (spm/1000)%10);
    lk.displayHex(5, (spm/100 )%10);
    lk.displayHex(6, (spm/10  )%10);
    lk.displayHex(7, (spm     )%10);
    mqtt_publish("spm", String(spm));
  }

  void cmd_dis() {
    message("ena", "cmd/off");
    lk.setLED(LED_ENA, 0);
    lk.displayASCII(0,'d');
    lk.displayASCII(1,'i');
    lk.displayASCII(2,'s');
    ena=false;
    mqtt_publish("event","disable");
  }
  void cmd_ena() {
    message("ena", "cmd/on");
    lk.setLED(LED_ENA, 1);
    lk.displayASCII(0,'e');
    lk.displayASCII(1,'n');
    lk.displayASCII(2,'a');
    ena=true;
    mqtt_publish("event", "enable");
  }
  void cmd_fwd() {
    message("dir", "cmd/on");
    dir_fwd=true;
    lk.setLED(LED_DIR, 1);
    lk.displayASCII(0,'f');
    lk.displayASCII(1,'w');
    lk.displayASCII(2,'d');
    mqtt_publish("dir", "fwd");
  }
  void cmd_rev() {
    message("dir", "cmd/off");
    dir_fwd=false;
    lk.setLED(LED_DIR, 0);
    lk.displayASCII(0,'r');
    lk.displayASCII(1,'e');
    lk.displayASCII(2,'v');
    mqtt_publish("dir", "rev");
  }
  void cmd_run() {
    message("stp", "set/duty", String(step_duty));
    mqtt_publish("event", "run");
    if (dir_fwd) {
      cmd_fwd();
    } else {
      cmd_rev();
    }
    speed(-1);
    lk.setLED(LED_RUN, 1);
    run=true;
  }
  void cmd_stop() {
    message("stp", "set/duty", "0");
    mqtt_publish("event", "stop");
    lk.setLED(LED_RUN, 0);
    lk.displayASCII(0,'s');
    lk.displayASCII(1,'t');
    lk.displayASCII(2,'p');
    run=false;

  }

  void cmd_stp(String payload) {
    lk.setLED(LED_STP, 1);
    mqtt_publish("event", "single");
#if !USE_PWM
    message("stp", "cmd/oneshot_usec", payload);
#endif
    delay(10);
    lk.setLED(LED_STP, 0);
  }
  void cmd_adj(int faster) {
    if (faster) {
      if (spm < 200) {
	speed(spm+10);
      }
      else {
	speed(spm+100);
      }
    }
    else {
      if (spm < 10) {
	speed(0);
      }
      else if (spm <= 200) {
	speed(spm-10);
      }
      else {
	speed(spm-100);
      }
    }
  }

  void cmd_spd(String payload) {
    speed(payload.toInt());
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN(    "dis", cmd_dis())
    ELSEWHEN("ena", cmd_ena())
    ELSEWHEN("fwd", cmd_fwd())
    ELSEWHEN("rev", cmd_rev())
    ELSEWHEN("run", cmd_run())
    ELSEWHEN("spd", cmd_spd(payload))
    ELSEWHEN("stp", cmd_stp(payload))
    ELSEWHEN("stop", cmd_stop())
    ELSEWHEN("fst", cmd_adj(1))
    ELSEWHEN("slw", cmd_adj(0))
    else {
      AbstractAppLeaf::commandHandler(type, name, topic, payload);
    }

    LEAF_HANDLER_END;
  }

  void handle_keypress(int k) {
    LEAF_ENTER_INT(L_NOTICE, k);

    switch (k) {
    case 0:
      if (ena) cmd_dis(); else cmd_ena();
      break;
    case 1:
      if (dir_fwd) cmd_rev(); else cmd_fwd();
      break;
    case 2:
      if (run) cmd_stop(); else cmd_run();
      break;
    case 3:
      cmd_stp(String(step_us));
      break;
    case 6:
      cmd_adj(0); //slower
      break;
    case 7:
      cmd_adj(1); //faster
      break;
    }
    LEAF_LEAVE;
  }

  virtual void loop() {
    static uint8_t b_was = 0;
    uint8_t b = lk.readButtons();
    if (b != b_was) {
      LEAF_NOTICE("Button state change 0x%02x", b);
      for (int k=0; k<8;k++) {
	if ((b&(1<<k)) && !(b_was&(1<<k))) {
	  handle_keypress(k);
	}
      }
      b_was=b;
    }
  }

#if 0
  virtual void display() {
    LEAF_ENTER(L_NOTICE);
    if (!screen) {
      LEAF_ALERT("no screen");
      return;
    }
    message(screen, "cmd/clear", "", HERE);

    cmdf(screen, "draw", "{\"font\":10,\"align\":\"left\",\"row\": 1,\"column\": 1,\"text\":\"%s %s %d\"}",[
	 (run?"RUN  ":"STOP"), dir_fwd?"FWD":"REV", spm);
    LEAF_LEAVE;
   }
#endif



};



Leaf *leaves[] = {
  //new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
  //new FSPreferencesLeaf("prefs"),
  new ShellLeaf("shell", "Stacx CLI"),
  //new WireBusLeaf("wire"),
  //new OledLeaf("screen"),

#if APP_USE_WIFI
  (new IpEspLeaf("wifi","fs"))->setTrace(L_NOTICE),
  new PubsubEspAsyncMQTTLeaf(
    "wifimqtt","wifi,fs",
    PUBSUB_SSL_DISABLE,
    PUBSUB_DEVICE_TOPIC_DISABLE
    ),
#else
  new IpNullLeaf("nullip", "fs"),
  new PubsubNullLeaf("nullmqtt", "nullip,fs"),
#endif
#if USE_PWM
  (new PWMLeaf("stp", LEAF_PIN(14/*D5*/)))->setTrace(L_INFO),
#else
  (new ActuatorLeaf("stp", NO_TAPS, LEAF_PIN(14/*D5*/), false, false))->setTrace(L_INFO),
#endif

  new ActuatorLeaf("dir", "", LEAF_PIN(D6), ActuatorLeaf::PERSIST_ON),
  new ActuatorLeaf("ena", "", LEAF_PIN(D7), ActuatorLeaf::PERSIST_OFF),

  (new StepperAppLeaf("app","fs,screen,stp,dir,ena"))->setTrace(L_NOTICE),
  NULL
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
