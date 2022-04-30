#pragma once
#pragma STACX_LIB Bounce2
#pragma STACX_LIB Adafruit_NeoPixel

#include <Bounce2.h>
#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel strip = Adafruit_NeoPixel(16, 4, NEO_GRB + NEO_KHZ800);

class ChaserLeaf : public Leaf
{
public:
  // 
  // Declare your leaf-specific instance data here
  // 
  int count;
  int pattern;
  uint32_t color;
protected:
  int switch_led;
  int button_pin;
  int max_pattern;
  int step;
  int max_step;
  uint32_t last_step;
  int rate;
  Bounce button = Bounce(); // Instantiate a Bounce object

public:
  // 
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  ChaserLeaf(String name, pinmask_t pins, int pxcount, uint32_t initial_color) : Leaf("chaser", name, pins){
    count = pxcount;
    color =   initial_color;

    int pixelPin;
    FOR_PINS({pixelPin=pin;});
    LEAF_INFO("%s claims pin %d as NeoPixel", base_topic.c_str(), pixelPin);
    switch_led = 2;
    button_pin = 5;
    max_pattern = 3;
    last_step = 0;
    
    button.attach(button_pin, INPUT_PULLUP); 
    button.interval(25); 

  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();

      pinMode(switch_led, OUTPUT);
      pinMode(button_pin, INPUT_PULLUP);
      strip.begin();
      strip.show();
      digitalWrite(switch_led, HIGH);
      set_pattern(0);
  }

  void status_pub() {
    mqtt_publish("status/pattern", String(pattern, DEC));
    mqtt_publish("status/color", String(color, HEX));
  }
  
  void mqtt_do_subscribe() {
    LEAF_ENTER(L_NOTICE);
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("set/pattern");
    mqtt_subscribe("set/color");
    mqtt_subscribe("status/pattern");
    mqtt_subscribe("status/color");
    LEAF_LEAVE;
  }

  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    bool lit = false;

    WHEN("set/pattern",{
	payload.toLowerCase();
	if (payload.equals("chaser")) {
	  set_pattern(0);
	}
	else if (payload.equals("rainbow")) {
	  set_pattern(1);
	}
	else {
	  set_pattern(payload.toInt());
	}
	LEAF_INFO("Updating pattern via set operation <= %d", pattern);
      })
    ELSEWHEN("set/color",{
	color = strtoul(payload.c_str(), NULL, 16);
	LEAF_NOTICE("Updating chaser color via set operation <= %x", color);
      })
  }
	
  // 
  // Arduino loop function
  // (Superclass function will take care of heartbeats)
  // 
  void loop(void) {
    Leaf::loop();
    button.update();
    
    if (button.fell()) {
      pattern++;
      if (pattern > max_pattern) pattern=0;
      set_pattern(pattern);
      mqtt_publish("status/pattern", String(pattern, DEC));
    }

    uint32_t now = millis();

    //DEBUG("last_step=%lu now=%lu step=%d rate=%d", last_step, now, step, rate);
    if (now < (last_step + rate)) {
      //DEBUG("no step");
      return;
    }
    last_step = now;
    step++;
    //DEBUG("Pattern=%d Step=%d", pattern, step);
    if (step > max_step) step = 0;
    
    //
    // Time for the next step
    //
    switch (pattern) {
    case 0: // chaser
	strip.clear();
	strip.setPixelColor(2*step, (color>>16)&0xFF, (color>>8)&0xFF, color&0xFF);
	strip.setPixelColor(count-1-(2*step), (color>>16)&0xFF, (color>>8)&0xFF, color&0xFF);
	strip.show();
	break;
    case 1: // rainbow
      for(int i=0; i<strip.numPixels(); i++) {
	strip.setPixelColor(i, Wheel((i+step) & 255));
      }
      strip.show();
      break;
    case 2: // rainbow cycle
      for(int i=0; i< strip.numPixels(); i++) {
	strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + step) & 255));
      }
      strip.show();
      break;
    case 3: // theater chase
      {
	int q = step %8;
	int j = step / 8;
	if (step%2) {
	  for (int i=0; i < strip.numPixels(); i=i+4) {
	    strip.setPixelColor(i+q, 0);        //turn every third pixel off
	  }
	}
	else {
	  for (int i=0; i < strip.numPixels(); i=i+4) {
	    strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
	  }
	}
      }// end case 3
      break;
    default:
      LEAF_ALERT("Unhandled pattern %d", pattern);
    }// end switch
  }// end loop

  private:
    void set_pattern(int p) {
      LEAF_NOTICE("%d", p);
      
      pattern = p;
      step = last_step = 0;
      
      switch(pattern) {
      case 0: // chaser
	rate = 250;
	max_step = 8;
	break;
      case 1: // rainbow
	rate = 20;
	max_step = 256;
	break;
      case 2: // rainbow cycle
	rate = 20;
	max_step = 256*5;
	break;
      case 3: // theaterchase rainbow
	rate = 50;
	max_step = 256*4*2;
	break;
      default:
	LEAF_ALERT("Unsupported pattern %d", p);
      }
    }
    

    // Input a value 0 to 255 to get a color value.
    // The colours are a transition r - g - b - back to r.
    uint32_t Wheel(byte WheelPos) {
      WheelPos = 255 - WheelPos;
      if(WheelPos < 85) {
	return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
      }
      if(WheelPos < 170) {
	WheelPos -= 85;
	return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
      }
      WheelPos -= 170;
      return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    }

    
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
