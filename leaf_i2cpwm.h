//
//@**************************** class I2cpwmLeaf *****************************
// Accelerando I2C PWM output (used for variable speed fan controllers)
//

class I2cpwmLeaf : public Leaf
{
public:
  //
  // Declare your leaf-specific instance data here
  //
  byte addr;
  byte speed;

  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  I2cpwmLeaf(String name)
    : Leaf("i2cpwm", name, addr=0x26)
    , Debuggable(name)
  {
    speed = 255;
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    set_speed(speed);
    registerCommand(HERE, "stop", "stop the fan");
    registerValue(HERE, "speed", VALUE_TYPE_INT, &speed, "set fan speed (percent)");

  }

  //
  // MQTT message callback
  // (Use the superclass callback to ignore messages not addressed to this leaf)
  //
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_INFO);
    bool handled = false;

    WHEN("cmd/stop",{set_speed(0);})
    ELSEWHEN("set/speed",{set_speed(payload.toInt());});
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    return handled;
  }

private:
  char set_speed(byte speed)
  {
    ENTER(L_NOTICE);

    LEAF_NOTICE("set PWM at I2C 0x%02X to %d", (int)addr, (int)speed);
    char c;

    this->speed = speed;
    Wire.beginTransmission(addr); // transmit to device #8
    Wire.write(speed);              // sends one byte
    Wire.endTransmission();    // stop transmitting

    Wire.requestFrom(addr, (byte)1);
    while (Wire.available()) { // slave may send less than requested
      c = Wire.read(); // receive a byte as character
    }
    RETURN(c);
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
