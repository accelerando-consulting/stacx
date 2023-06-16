//
//@**************************** class PinExtender74595Leaf ******************************
//
// This class encapsulates a simple output-only IO extender using 74595 shift register
//
// On ESP32 the SPI or I2S modules can be used to do high speed pin control, but
// YOLO this module just (currently) bit-bangs
//
#pragma once

typedef struct _pin_state_word 
{
  bool value;
  bool invert;
  String name;
} pin_state_word;

typedef std::vector<pin_state_word> pin_vector;

class PinExtender74595Leaf : public Leaf
{
protected:
  uint8_t pin_count;
  pin_vector pins;

  int pin_DS= -1;
  int pin_SHCP = -1;
  int pin_STCP = -1;
  int pin_NMR = -1;
  int pin_NOE = -1;
  
public:
  PinExtender74595Leaf(String name, int npins, String names="",
		       int ds=-1, int shcp=-1, int stcp=-1, int nmr=-1, int noe=-1)
    : Leaf("pinextender", name, NO_PINS)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);

    pin_count = npins;

    for (int i=0; i<pin_count;i++) {
      pin_state_word state = {
	.value = false,
	.invert = false,
	.name = String(i)
      };
      pins.push_back(state);
    }

    if (names.length())
    {
      for (int c=0; (c<pin_count) && names.length(); c++) {
	if (names[0]=='~') {
	  pins[c].invert=true;
	  names.remove(0,1);
	}
	int pos = names.indexOf(',');
	if (pos >= 0) {
	  pins[c].name = names.substring(0,pos);
	  names.remove(0,pos+1);
	}
	else {
	  pins[c].name = names;
	  names = "";
	}
      }
    }
    pin_DS = ds;
    pin_SHCP = shcp;
    pin_STCP = stcp;
    pin_NMR = nmr;
    pin_NOE = noe;

    LEAF_LEAVE;
  }

  virtual void setup(void) {
    Leaf::setup();

    LEAF_ENTER(L_INFO);
    //wire->begin();

    if ((pin_DS<0) || (pin_SHCP<0) || (pin_STCP<0)) {
      LEAF_ALERT("%s pins not configured", describe().c_str());
      stop();
      LEAF_VOID_RETURN;
    }
    
    LEAF_NOTICE("%s claims pins DS=%d SHCP=%d STCP=%d ~MR=%d ~OE=%d",
		describe().c_str(),
		pin_DS, pin_SHCP, pin_STCP, pin_NMR, pin_NOE);
    for (int p=0; p<pin_count; p++) {
      LEAF_NOTICE("   pin %2d (%s)%s", p, pins[p].name, pins[p].invert?" (inverted)":"");
    }
    
    pinMode(pin_DS, OUTPUT);
    digitalWrite(pin_DS, 0);
    pinMode(pin_SHCP, OUTPUT);
    digitalWrite(pin_SHCP, 0);
    pinMode(pin_STCP, OUTPUT);
    digitalWrite(pin_STCP, 0);
    
    if (pin_NOE>=0) {
      pinMode(pin_NOE, OUTPUT);
      digitalWrite(pin_NOE, LOW);
    }
    if (pin_NMR>=0) {
      pinMode(pin_NMR, OUTPUT);
      // reset the latch
      digitalWrite(pin_NMR, LOW);
      delay(1);
      digitalWrite(pin_NMR, HIGH);
    }
    else {
      // no master reset, use the shift input to clear all pins
      write(pins);
    }
      
    registerLeafCommand(HERE, "set", "Set shift register pin");
    registerLeafCommand(HERE, "clear", "Clear shift register pin");
    registerLeafCommand(HERE, "toggle", "Toggle shift register pin");
    
    LEAF_LEAVE;
  }

  void draw_bits(pin_vector &bits, char *buf, int buf_max)
  {
    LEAF_ENTER_INT(L_DEBUG, bits.size());
    int pos = snprintf(buf, buf_max, "0b");
    int b = bits.size();
    
    while (b --> 0) {
      pin_state_word pin = bits[b];
      LEAF_DEBUG("Pin record for pin %d: [v=%s invert=%s name=%s]",
		 b, ABILITY(pin.value), TRUTH_lc(pin.value), pin.name);

      if (b && (b%8 == 0)) {
	// byte separator
	pos+= snprintf(buf+pos, buf_max-pos, ":");
      }
      pos += snprintf(buf+pos, buf_max-pos, pin.value?"1":"0");
      if ((b%8)==4) {
	  // nibble separator
	pos+= snprintf(buf+pos, buf_max-pos, "-");
      }
    }
  }

  void shift_out_one_bit(bool b) 
  {
    digitalWrite(pin_DS, b);
    delayMicroseconds(1); // set up time for data pin, probably unneccessary
    digitalWrite(pin_SHCP, 1);
    delayMicroseconds(1); // datasheet says 5ns is sufficient delay for clock pulse
    digitalWrite(pin_SHCP, 0);
    delayMicroseconds(1);
  }

  void load_shifted_bits() 
  {
    LEAF_INFO("  Pulsing output register load strobe");
    digitalWrite(pin_STCP, 1);
    delayMicroseconds(1);
    digitalWrite(pin_STCP, 0);
    delayMicroseconds(1);
  }

  void set_up_strobes() 
  {
    digitalWrite(pin_STCP, 0);
    digitalWrite(pin_SHCP, 0);
    delayMicroseconds(1);
  }
  
  
  int write(pin_vector &v)
  {
    LEAF_ENTER_INT(L_INFO, v.size());
    char buf[80];
    set_up_strobes();
    
    draw_bits(v, buf, sizeof(buf));
    LEAF_NOTICE("  write %s <= %s", describe().c_str(), buf);

    int n = v.size();
    int b = n;
    while (b%8) b++; // round up to multiple of 8 bits

    while (b --> 0) {
      bool value = (b<n)?v[b].value:false;
      bool invert = (b<n)?v[b].invert:false;
      bool bit = value ^ invert;
      LEAF_INFO("  Shifting out pin %d as bit %d of byte %d: => logical %s electrical %s", b, b%8, b/8,
		STATE(value), STATE(bit));
      shift_out_one_bit(bit);
    }
    load_shifted_bits();

    LEAF_RETURN(0);
  }

  void bits_out_to_hex_buf(pin_vector &v, char *buf, int max) 
  {
    int pos=0;
    int b = v.size();

    byte chunk = 0;
    while (b --> 0) {
      chunk |= (1 << (b%8));
      if ((b%8) == 0) {
	if (pos > 0) {
	  pos+=snprintf(buf+pos, max-pos, ":");
	}
	pos += snprintf(buf+pos, max-pos, "%02x", (int)chunk);
	chunk = 0;
      }
    }
  }
  
  String bit_rep(pin_state_word &p) 
  {
    return p.value?"1":"0";
  }
  
  String bit_rep(pin_vector &pins, int b) 
  {
    if ((b<0) || (b > pins.size())) return "x";
    return pins[b].value?"1":"0";
  }
  
  virtual void status_pub()
  {
    LEAF_ENTER(L_INFO);

    char msg[80];
    bits_out_to_hex_buf(pins, msg, sizeof(msg));
    mqtt_publish("status/bits_out", String(msg));

    for (pin_state_word p : pins) {
      mqtt_publish(String("status/")+p.name, bit_rep(p));
    }

    LEAF_LEAVE;
  }

  int parse_channel(String s) {
    LEAF_ENTER_STR(L_DEBUG, s);
    for (int c=0; c<pin_count; c++) {
      if (pins[c].name.length()) {
	if (s == pins[c].name) {
	  LEAF_INT_RETURN(c);
	}
      }
    }
    LEAF_INT_RETURN(s.toInt());
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    bool val = false;
    if (payload == "on") val=true;
    else if (payload == "true") val=true;
    else if (payload == "high") val=true;
    else if (payload == "1") val=true;
    int bit = payload.toInt();

    LEAF_INFO("RECV %s/%s %s <= [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("get/pin",{
      bit = parse_channel(payload);
      mqtt_publish(String("status/")+payload, bit_rep(pins, bit));
    })
    WHEN("get/pins",{
      char buf[80];
      bits_out_to_hex_buf(pins, buf, sizeof(buf));
      mqtt_publish(String("status/pins"), buf);
    })
    ELSEWHENPREFIX("set/pin/",{
      String topicfrag = topic.substring(payload.lastIndexOf('/')+1);
      bit = parse_channel(topicfrag);
      LEAF_NOTICE("Set pin %d <= %s", bit, STATE(val));
      pins[bit].value=val;
      write(pins);

      publish(String("status/")+topicfrag, String(val?"on":"off"), L_INFO, HERE);
    })
    ELSEWHEN("set/pins",{
	int b = 0;
	for (int pos=0; pos < payload.length(); pos+=2) {
	  byte chunk = (uint8_t)strtoul(payload.substring(pos,pos+2).c_str(), NULL, 16);
	  for (int bit=0; bit<8;bit++) {
	    if (b<pin_count) {
	      pins[pos/2+b].value = chunk&(1<<bit);
	    }
	  }
	}
	write(pins);
    })
    ELSEWHEN("cmd/set",{
      bit = parse_channel(payload);
      if (bit < pin_count) {
	LEAF_NOTICE("Setting pin %d (%s)", bit, payload.c_str());
	pins[bit].value=true;
	write(pins);
	status_pub();
      }
    })
    ELSEWHEN("cmd/toggle",{
      bit = parse_channel(payload);
      if (bit < pin_count) {
	pins[bit].value = !pins[bit].value;
	LEAF_NOTICE("Toggle pin %d (%s)", bit, payload.c_str());
	write(pins);
	status_pub();
      }
    })
    ELSEWHENEITHER("cmd/unset","cmd/clear", {
      bit = parse_channel(payload);
      if (bit < pin_count) {
	LEAF_NOTICE("Clearing pin %d (%s)", bit, payload.c_str());
	pins[bit].value = false;
	write(pins);
	status_pub();
      }
    })
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_BOOL_RETURN(handled);
  };

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
