#pragma once
#pragma STACX_LIB Adafruit_NeoPixel

#include <Adafruit_NeoPixel.h>

struct flashRestoreContext 
{
  Adafruit_NeoPixel *pixels;
  int pos;
  uint32_t color;
} pixel_restore_context;

Adafruit_NeoPixel string(1, 5, NEO_GRB + NEO_KHZ800);// evil hack

class PixelLeaf : public Leaf
{
public:
  // 
  // Declare your leaf-specific instance data here
  // 
  int pixelPin;
  int count;
  int flash_duration;

  uint32_t color;
  
  uint8_t sat;
  uint8_t val;
  Adafruit_NeoPixel *pixels;
  Ticker flashRestoreTimer;
  
public:
  // 
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  PixelLeaf(String name, pinmask_t pins, int pxcount=1, uint32_t initial_color=0) : Leaf("pixel", name, pins){

    FOR_PINS({pixelPin=pin;});
    pixels = &string;//new Adafruit_NeoPixel(count, pixelPin, NEO_GRB + NEO_KHZ400);
    flash_duration = 100;
    count = pxcount;
    color = initial_color;
    sat = 255;
    val = 255;
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    LEAF_NOTICE("%s claims pin %d as %dxNeoPixel", base_topic.c_str(), count, pixelPin);
    pixels->begin();
    pixels->clear();
    if (color) {
      LEAF_NOTICE("Set initial color 0x%06X", color);
      for (int i=0; i<count;i++) {
	pixels->setPixelColor(i, color);
      }
      pixels->show();
    }
  }

  void start(void) {
    Leaf::start();
    pixels->show();
  }

  void status_pub() {
    if (count == 1) {
      mqtt_publish("status/color", String(pixels->getPixelColor(0), HEX));
    }
    else {
      LEAF_ALERT("TODO multi-item status not implemented");
    }
  }
  
  void mqtt_do_subscribe() {
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("set/color");
    mqtt_subscribe("set/colors");
  }

  void setPixelRGB(int pos, uint32_t rgb)
  {
    if (count < pos) return;
    uint32_t color = pixels->Color((rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
    LEAF_DEBUG("%d <= 0x%06X", pos, color);
    pixels->setPixelColor(pos, color);
  }
  
  void setPixelRGB(int pos, String hex) 
  {
    if (count < pos) return;
    uint32_t rgb = strtoul(hex.c_str(), NULL, 16);
    LEAF_DEBUG("Parsed %s as 0x%06X", hex.c_str(), rgb);
    setPixelRGB(pos, rgb);
  }

  void flashPixelRGB(int pos, String hex, int duration=0) 
  {
    LEAF_DEBUG("flashPixelRGB %d,%d,%d", pos, hex.c_str(),duration);

    if (count < pos) return;
    if (duration == 0) {
      int comma = hex.indexOf(",");
      if (comma >= 0) {
	duration = hex.substring(comma+1).toInt();
	hex.remove(comma);
      }
      else {
	duration = flash_duration;
      }
    }
    if (duration < 1) return;
    pixel_restore_context.color = pixels->getPixelColor(pos);
    LEAF_INFO("Flash %s@%d for %dms (then restore 0x%06X)", hex, pos, duration, pixel_restore_context.color);
    pixel_restore_context.pos = pos;
    pixel_restore_context.pixels = pixels;
    uint32_t rgb = strtoul(hex.c_str(), NULL, 16);    
    setPixelRGB(pos, rgb);
    pixels->show();
    flashRestoreTimer.once_ms(duration, [](){
      Adafruit_NeoPixel *pixels = pixel_restore_context.pixels;
      int pos = pixel_restore_context.pos;
      uint32_t color = pixel_restore_context.color;
      DEBUG("Restore pixel % <= 0x%06X", pos, color);
      pixels->setPixelColor(pos,color);
      pixels->show();
    }
      );
  }

  void setPixelHSV(int pos, String hue) 
  {
    if (count < pos) return;
    pixels->setPixelColor(pos, pixels->ColorHSV(hue.toInt(), sat, val));
    pixels->show();
  }
  
  bool mqtt_receive(String type, String name, String topic, String payload) {
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    if ((type == "app") || (type=="shell")) {
      LEAF_DEBUG("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }

    WHEN("cmd/flash",{
      flashPixelRGB(0, payload);
      })
    ELSEWHEN("set/hue",{
	setPixelHSV(0, payload);
	pixels->show();
      })
    ELSEWHEN("set/value",{
	val = payload.toInt();
      })
    ELSEWHEN("set/saturation",{
	sat = payload.toInt();
      })
    ELSEWHEN("set/color",{
	setPixelRGB(0, payload);
	pixels->show();
      })
    ELSEWHEN("set/colors",{
	LEAF_ALERT("TODO Not implemented yet");
      })
    else {
      if ((type == "app") || (type=="shell")) {
	LEAF_NOTICE("pixel leaf did not handle [%s]", topic.c_str());
      }
    }
    return 0;
  }
	
  // 
  // Arduino loop function
  // (Superclass function will take care of heartbeats)
  // 
  void loop(void) {
    Leaf::loop();
  }
  
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End: