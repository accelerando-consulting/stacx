//
//@**************************** class PulseCounterLeaf ******************************
//
// This class encapsulates an interrupt driven pulse counter
//
#include <Bounce2.h>

void ARDUINO_ISR_ATTR counterISR(void *arg);


class PulseCounterLeaf : public Leaf
{
protected:
  unsigned long lastCount=0;
public:
  int counterPin = -1;
  int mode = FALLING;
  unsigned long count=0;
  bool level=LOW;
  
  PulseCounterLeaf(String name, pinmask_t pins, int mode=FALLING) : Leaf("pulsecounter", name, pins) {
    LEAF_ENTER(L_DEBUG);
    this->mode = mode;
    this->do_heartbeat = false;
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    static const char *mode_names[]={"DISABLED","RISING","FALLING","CHANGE","ONLOW","ONHIGH"};
    static int mode_name_max = ONHIGH;
      
    LEAF_ENTER(L_INFO);
    Leaf::setup();
    FOR_PINS({counterPin=pin;});
    LEAF_NOTICE("%s claims pin %d as INPUT (mode %d %s)", base_topic.c_str(),
	      counterPin, mode, (mode<=mode_name_max)?mode_names[mode]:"invalid");

    pinMode(counterPin, INPUT_PULLUP);
    attachInterruptArg(counterPin, counterISR, this, mode);

    LEAF_LEAVE;
  }

  void pulse(void) {
    count++;
    level = digitalRead(counterPin);
  }

  void reset(void) {
    count=lastCount =0;
  }
  
  virtual void status_pub()
  {
    mqtt_publish("status/count", String(count));
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    LEAF_NOTICE("%s [%s]", topic.c_str(), payload.c_str());
    WHEN("cmd/pulse", {pulse();})
    ELSEWHEN("cmd/reset",{reset();});
    LEAF_RETURN(handled);
  }
  
  virtual void loop(void) {
    Leaf::loop();
    if (count > lastCount) {
      status_pub();
      lastCount=count;
    }
  }

};

void ARDUINO_ISR_ATTR counterISR(void *arg) {
  PulseCounterLeaf *leaf = (PulseCounterLeaf *)arg;
  leaf->pulse();
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
