//
//@**************************** class PWMSensorLeaf ******************************
//
// This class encapsulates an interrupt driven pulse-width sensor
// (where a sensor communicates by varying the duty cycle of a pulse)
//

void ARDUINO_ISR_ATTR pwmSensorISR(void *arg);

class PWMSensorLeaf : public Leaf
{
protected:
  unsigned long lastFallMicro = 0;
  unsigned long lastRiseMicro = 0;

  int pin = -1;
  bool pullup = false;

  int pulse_us = 0;
  int interval_us = 0;
  
public:
  PWMSensorLeaf(String name, pinmask_t pins, bool pullup=false) : Leaf("pwmsensor", name, pins) {
    LEAF_ENTER(L_DEBUG);
    FOR_PINS({this->pin=pin;});
    this->pullup = pullup;
    this->do_heartbeat = false;
    LEAF_LEAVE;
  }

  virtual void setup(void) {
      
    LEAF_ENTER(L_INFO);
    Leaf::setup();

    LEAF_NOTICE("%s claims pin %d as INPUT ", base_topic.c_str(), pin);
    pinMode(pin, pullup?INPUT_PULLUP:INPUT);
    attachInterruptArg(pin, pwmSensorISR, this, CHANGE);

    LEAF_LEAVE;
  }

  void pulse(void) {

    unsigned long unow = micros();
    unsigned long pulse_width_us, pulse_interval_us;
    bool newLevel;

    newLevel = digitalRead(pin);

    if (newLevel == LOW) {
      lastFallMicro = unow;
      return;
    }
    
    if (lastRiseMicro == 0) {
      // this is our first rising edge, we cannot make any decisions yet
      lastRiseMicro = unow;
      return;
    }

    pulse_interval_us = unow - lastRiseMicro;
    pulse_width_us = lastFallMicro - lastRiseMicro;
    lastRiseMicro = unow;

    if (pulse_width_us !=  pulse_us) {
      LEAF_NOTICE("pulse_width_us=%lu pulse_interval_us=%lu", pulse_width_us, pulse_interval_us);
      pulse_us = pulse_width_us;
      interval_us = pulse_interval_us;
      status_pub();
    }
      
  }


  
  virtual void status_pub()
  {
    //LEAF_NOTICE("count=%lu", count);
    mqtt_publish("status/pulse", String(pulse_us)+","+String(interval_us));
  }

};

void ARDUINO_ISR_ATTR pwmSensorISR(void *arg) {
  PWMSensorLeaf *leaf = (PWMSensorLeaf *)arg;
  leaf->pulse();
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
