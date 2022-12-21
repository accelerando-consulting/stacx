#pragma once
#pragma STACX_LIB Adafruit_NeoPixel

#include <Adafruit_NeoPixel.h>

struct flashRestoreContext 
{
  Adafruit_NeoPixel *pixels;
  int pos;
  uint32_t color;
  SemaphoreHandle_t pixel_sem;
} pixel_restore_context;

class PixelLeaf : public Leaf
{
public:
  // 
  // Declare your leaf-specific instance data here
  // 
  int pixelPin;
  int count;
  int flash_duration;
  int brightness=255;
  int refresh_sec=2;
  unsigned long last_refresh=0;
  bool do_check=false;

  uint32_t color;
  
  uint8_t sat;
  uint8_t val;
  Adafruit_NeoPixel *pixels;  
  Ticker flashRestoreTimer;
  SemaphoreHandle_t pixel_sem=NULL;
  StaticSemaphore_t pixel_sem_buf;
  
public:
  // 
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  PixelLeaf(String name, pinmask_t pins, int pxcount=1, uint32_t initial_color=0, Adafruit_NeoPixel *pixels=NULL)
    : Leaf("pixel", name, pins)
    , TraitDebuggable(name)
  {
    FOR_PINS({pixelPin=pin;});
    this->pixels = pixels; // null means create in setup
    flash_duration = 100;
    count = pxcount;
    color = initial_color;
    sat = 255;
    val = 255;
    pixel_restore_context.pos = -1;

    pixel_sem = xSemaphoreCreateBinaryStatic(&pixel_sem_buf); // can't fail
    xSemaphoreGive(pixel_sem);
  }

  void show() 
  {
    if (!pixels) return;
    
    if (xSemaphoreTake(pixel_sem, (TickType_t)100) != pdTRUE) {
      LEAF_ALERT("Pixel semaphore blocked");
      return;
    }
    pixels->show();
    xSemaphoreGive(pixel_sem);
  }
  

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
    
    getIntPref("pixel_brightness", &brightness, "NeoPixel brightness adjustment (0-255)");
    getIntPref("pixel_refresh_sec", &refresh_sec, "NeoPixel refresh interval in seconds (0=off)");

    if (!pixels) {
      pixels = new Adafruit_NeoPixel(count, pixelPin, NEO_GRB + NEO_KHZ800);
    }

    if (!pixels) {
      LEAF_ALERT("Pixel create failed");
      LEAF_VOID_RETURN;
    }

    LEAF_NOTICE("%s claims pin %d as %d x NeoPixel, initial color %08X", describe().c_str(), pixelPin, count, color);

    pixels->begin();
    pixels->setBrightness(brightness);
    pixels->clear();
    if (do_check) {
      ::stacx_pixel_check(pixels, 8, 250);
    }
    if (color) {
      for (int i=0; i<count;i++) {
	LEAF_NOTICE("    initial pixel color %d <= 0x%06X", i, color);
	pixels->setPixelColor(i, color);
      }
      show();
    }
    LEAF_VOID_RETURN;
  }

  void start(void) {
    Leaf::start();
    LEAF_ENTER(L_NOTICE);
    if (pixels) {
      LEAF_NOTICE("Show pixels on pin %d", pixelPin);
      show();
    }
    LEAF_VOID_RETURN;
  }

  void pre_sleep(int duration) 
  {
    LEAF_ENTER(L_NOTICE);
    pixels->clear();
    show();
    LEAF_LEAVE;
  }

  void status_pub() {
    LEAF_ENTER(L_NOTICE);
    if (count == 1) {
      mqtt_publish("status/color", String(pixels->getPixelColor(0), HEX));
    }
    else {
      DynamicJsonDocument doc(512);
      char msg[512];
      JsonArray arr = doc.to<JsonArray>();
      for (int i=0; i<count; i++) {
	arr[i] = String(pixels->getPixelColor(i), HEX);
      }
      serializeJson(doc, msg, sizeof(msg)-2);
      mqtt_publish("status/color", msg);
    }
    LEAF_VOID_RETURN;
  }
  
  void mqtt_do_subscribe() {
    Leaf::mqtt_do_subscribe();
    register_mqtt_cmd("flash", "Flash the first LED (or any if topic followed by /n)",HERE);
    register_mqtt_value("color", "Set the color of the first LED (or any if topic followed by /n)", ACL_SET_ONLY, HERE);
    register_mqtt_value("brightness", "Set the brightness correct of the LED string", ACL_SET_ONLY, HERE);
    register_mqtt_value("refresh", "Set the refresh interval in seconds (0=off)", ACL_SET_ONLY, HERE);
    if (!leaf_mute) {
      mqtt_subscribe("set/color/+", HERE);
    }
  }

  void setPixelRGB(int pos, uint32_t rgb)
  {
    if (count < pos) return;
    if (!pixels) return;
    uint32_t color = pixels->Color((rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
    LEAF_DEBUG("%d <= 0x%06X", pos, color);
    pixels->setPixelColor(pos, color);
  }
  
  void setPixelRGB(int pos, String hex) 
  {
    if (count < pos) return;
    uint32_t rgb;
    
    if (hex == "red") {
      rgb = pixels->Color(150,0,0);
    }
    else if (hex == "brown") {
      rgb = pixels->Color(191,121,39);
    }
    else if (hex == "green") {
      rgb = pixels->Color(0,150,0);
    }
    else if (hex == "blue") {
      rgb = pixels->Color(0,0,150);
    }
    else if (hex == "cyan") {
      rgb = pixels->Color(0,120,120);
    }
    else if (hex == "yellow") {
      rgb = pixels->Color(120,120,0);
    }
    else if (hex == "orange") {
      rgb = pixels->Color(120,60,0);
    }
    else if (hex == "magenta") {
      rgb = pixels->Color(120,0,120);
      
    }
    else if (hex == "purple") {
      rgb = pixels->Color(60,0,120);
    }
    else if (hex == "pink") {
      rgb = pixels->Color(153,102,255);
    }
    else if (hex == "white") {
      rgb = pixels->Color(100,100,100);
    }
    else if (hex == "black") {
      rgb = pixels->Color(0,0,0);
    }
    else {
      rgb = strtoul(hex.c_str(), NULL, 16);
    }
    LEAF_NOTICE("%d  <= 0x%06X (%s)", pos, rgb, hex.c_str());
    setPixelRGB(pos, rgb);
  }

  void flashPixelRGB(int pos, String hex, int duration=0) 
  {
    LEAF_DEBUG("flashPixelRGB %d,%d,%d", pos, hex.c_str(),duration);

    if (count < pos) return;
    if (!pixels) return;

    if (pixel_restore_context.pos >= 0) {
      // already doing a flash, skip this one
      return;
    }
    
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
    //LEAF_DEBUG("Flash %s@%d for %dms (then restore 0x%06X)", hex, pos, duration, pixel_restore_context.color);
    pixel_restore_context.pos = pos;
    pixel_restore_context.pixels = pixels;
    pixel_restore_context.pixel_sem = pixel_sem;
    uint32_t rgb = strtoul(hex.c_str(), NULL, 16);    
    setPixelRGB(pos, rgb);
    show();
    flashRestoreTimer.once_ms(duration, [](){
      Adafruit_NeoPixel *pixels = pixel_restore_context.pixels;
      int pos = pixel_restore_context.pos;
      uint32_t color = pixel_restore_context.color;
      DEBUG("Restore pixel % <= 0x%06X", pos, color);
      pixels->setPixelColor(pos,color);
      if (xSemaphoreTake(pixel_restore_context.pixel_sem, 0) != pdTRUE) {      
	return;
      }
      pixels->show();
      xSemaphoreGive(pixel_restore_context.pixel_sem);
      pixel_restore_context.pos = -1;
    }
      );
  }

  void setPixelHSV(int pos, String hue) 
  {
    if (count < pos) return;
    if (!pixels) return;
    pixels->setPixelColor(pos, pixels->ColorHSV(hue.toInt(), sat, val));
    show();
  }
  
  bool mqtt_receive(String type, String name, String topic, String payload) {
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    if (true/*(type == "app") || (type=="shell")*/) {
      LEAF_INFO("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }

    WHEN("cmd/flash",{
      flashPixelRGB(0, payload);
      })
    ELSEWHENPREFIX("cmd/flash/",{
      flashPixelRGB(topic.toInt(), payload);
      })
    ELSEWHEN("set/refresh",{
	refresh_sec = payload.toInt();
	last_refresh = millis();
	show();
      })
    ELSEWHEN("set/hue",{
	setPixelHSV(0, payload);
	show();
      })
    ELSEWHEN("set/brightness",{
	brightness = payload.toInt();
	if (brightness < 0) brightness=0;
	if (brightness > 255) brightness=255;
	setIntPref("pixel_brightness", brightness);
	pixels->setBrightness(brightness);
	show();
      })
    ELSEWHEN("set/value",{
	val = payload.toInt();
      })
    ELSEWHEN("set/saturation",{
	sat = payload.toInt();
      })
    ELSEWHEN("set/color",{
	setPixelRGB(0, payload);
	show();
      })
    ELSEWHENPREFIX("set/color/",{
	setPixelRGB(topic.toInt(), payload);
	show();
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
    unsigned long now = millis();
    if (pixels && refresh_sec && (now > (last_refresh + refresh_sec*1000))) {
      last_refresh = now;
      LEAF_INFO("pixel refresh");
      show();
    }
  }
  
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
