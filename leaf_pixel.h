#pragma once
#pragma STACX_LIB Adafruit_NeoPixel

#include <Adafruit_NeoPixel.h>

int _compareUshortKeys(uint16_t &a, uint16_t &b)
{
  if (a<b) return -1;
  if (a>b) return 1;
  return 0;
}

class PixelLeaf;

#ifndef PIXEL_CHECK_DELAY
#define PIXEL_CHECK_DELAY 50
#endif

#ifndef PIXEL_CHECK_ITERATIONS
#define PIXEL_CHECK_ITERATIONS 8
#endif


struct flashRestoreContext
{
  PixelLeaf *leaf;
  int pos;
  int clone_pos;
  uint32_t flash_color;
  uint32_t color;
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
  int refresh_sec=5;
  unsigned long last_refresh=0;
  bool do_check=false;
  int check_delay = PIXEL_CHECK_DELAY;
  int check_iterations= PIXEL_CHECK_ITERATIONS;

  uint32_t color;

  int hue;
  int sat;
  int val;

  Adafruit_NeoPixel *pixels;
  Ticker flashRestoreTimer;
  SemaphoreHandle_t pixel_sem=NULL;
  StaticSemaphore_t pixel_sem_buf;
  SimpleMap<uint16_t,uint16_t> *clones=NULL;
  SimpleMap<uint16_t,uint16_t> *moves=NULL;

public:
  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  PixelLeaf(String name, pinmask_t pins, int pxcount=1, uint32_t initial_color=0, Adafruit_NeoPixel *pixels=NULL, bool do_check=false)
    : Leaf("pixel", name, pins)
    , Debuggable(name)
  {
    FOR_PINS({pixelPin=pin;});
    this->pixels = pixels; // null means create in setup
    this->do_check = do_check;
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

    if (!pixels) {
      LEAF_NOTICE("Create pixel string count=%d pin=%d");
      pixels = new Adafruit_NeoPixel(count, pixelPin, NEO_GRB + NEO_KHZ800);
    }

    if (!pixels) {
      LEAF_ALERT("Pixel create failed");
      LEAF_VOID_RETURN;
    }

    LEAF_NOTICE("%s claims pin %d as %d x NeoPixel, initial color %08X", describe().c_str(), pixelPin, count, color);

    registerLeafBoolValue("do_check", &do_check); // undocumented
    registerLeafIntValue("check_delay", &check_delay); // undocumented
    registerLeafIntValue("check_iterations", &check_iterations); // undocumented
    registerLeafIntValue("count", &count, "Number of pixels in string");
    registerLeafIntValue("brightness", &brightness, "NeoPixel brightness adjustment (0-255)");
    registerLeafIntValue("refresh_sec", &refresh_sec, "NeoPixel refresh interval in seconds (0=off)");
    registerLeafIntValue("color", NULL, "Set the color of the first LED", ACL_SET_ONLY);
    registerLeafIntValue("color/+", NULL, "Set the color of a particular (index in topic)", ACL_SET_ONLY);
    registerLeafIntValue("hue",&hue, "Set the HSV hue of first LED (or an y if topic followed by /n)");
    registerLeafIntValue("val", &val, "Set the HSV value of first LED (or an y if topic followed by /n)");
    registerLeafIntValue("sat",&sat, "Set the HSV saturation of first LED (or an y if topic followed by /n)");

    registerCommand(HERE, "flash", "Flash the first LED");
    registerCommand(HERE, "flash/+", "Flash the LED denoted in topic-suffix");
    registerCommand(HERE, "clone", "Nominate a pixel to duplicate another pixel (payload=\"orig=clone\")");
    registerCommand(HERE, "unclone", "Remove the clone of a given pixel (payload=orig)");
    registerCommand(HERE, "map", "Map a pixel lcoation to a different location (payload=\"src=dst\")");
    registerCommand(HERE, "unmap", "Remove the mapping of a given pixel (payload=src)");
    registerCommand(HERE, "list_clones");
    registerCommand(HERE, "check"); // unlisted

#ifdef USE_HELLO_PIXEL
    if (pixels == hello_pixel_string) {
      hello_pixel_sem = pixel_sem;
    }
#endif

    pixels->begin();
    pixels->setBrightness(brightness);
    pixels->clear();
    if (do_check) {
      stacx_pixel_check(pixels, check_iterations, check_delay, true);
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

  void clone(uint16_t src, uint16_t dst)
  {
    LEAF_ENTER_INTPAIR(L_NOTICE, src, dst);
    if (clones == NULL) {
      clones = new SimpleMap<uint16_t,uint16_t>(_compareUshortKeys);
    }
    if (clones) {
      clones->put(src, dst);
      pixels->setPixelColor(dst, pixels->getPixelColor(src));
      show();
    }
    LEAF_LEAVE;
  }

  void unclone(uint16_t src)
  {
    LEAF_ENTER_INT(L_NOTICE, src);
    if (clones) {
      clones->remove(src);
    }
    LEAF_LEAVE;
  }

  void map(uint16_t src, uint16_t dst)
  {
    LEAF_ENTER_INTPAIR(L_NOTICE, src, dst);
    if (moves == NULL) {
      moves = new SimpleMap<uint16_t,uint16_t>(_compareUshortKeys);
    }
    if (moves) {
      moves->put(src, dst);
      pixels->setPixelColor(dst, pixels->getPixelColor(src));
      pixels->setPixelColor(src, 0);
      show();
    }
    LEAF_LEAVE;
  }

  void unmap(uint16_t src)
  {
    LEAF_ENTER_INT(L_NOTICE, src);
    if (moves) {
      moves->remove(src);
    }
    LEAF_LEAVE;
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
  }

  void setPixelRGB(int pos, uint32_t rgb)
  {
    LEAF_ENTER_INT(L_INFO, pos);
    if (count < pos) return;
    if (!pixels) return;
    uint32_t color = pixels->Color((rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
    LEAF_DEBUG("%d <= 0x%06X", pos, color);

    if (moves && moves->has(pos)) {
      int mpos = moves->get(pos);
      //LEAF_INFO("pixel %d is mapped to alternative at %d <= 0x%06X", pos, mpos, color);
      pos = mpos;
    }
    pixels->setPixelColor(pos, color);

    if (clones && clones->has((uint16_t)pos)) {
      uint16_t cpos = clones->get((uint16_t)pos);
      //LEAF_INFO("pixel %d has clone at %d <= 0x%06X", (int)pos, (int)cpos, color);
      pixels->setPixelColor(cpos, color);
    }
    LEAF_LEAVE;
  }

  uint32_t parseColorRGB(String hex) 
  {
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
    return rgb;
  }
  
  void setPixelRGB(int pos, String hex)
  {
    if (count < pos) return;
    setPixelRGB(pos, parseColorRGB(hex));
  }

  void pixelRestoreContext(struct flashRestoreContext *ctx)
  {
    int pos = ctx->pos;
      int clone_pos = ctx->clone_pos;
      uint32_t color = ctx->color;
      pixels->setPixelColor(pos,color);
      if (clone_pos>=0) {
	pixels->setPixelColor(clone_pos,color);
      }
      if (xSemaphoreTake(pixel_sem, 0) != pdTRUE) {
	return;
      }
      pixels->show();
      xSemaphoreGive(pixel_sem);
      ctx->pos = -1;
  }

  void flashPixelRGB(int pos, String hex, int duration=0)
  {
    LEAF_ENTER_INT(L_INFO, pos);

    if (!pixels || (count < pos)) {
      LEAF_VOID_RETURN;
    }

    if (pixel_restore_context.pos >= 0) {
      // already doing a flash, abort previous one
      LEAF_WARN("pixel flash collision: asked to flash %d/%s but already flashing %d/%08x",
		pos, hex.c_str(), pixel_restore_context.pos, pixel_restore_context.flash_color);
      if (flashRestoreTimer.active()) flashRestoreTimer.detach();
      pixelRestoreContext(&pixel_restore_context);
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
    uint32_t flash_color = parseColorRGB(hex);
    pixel_restore_context.leaf = this;
    pixel_restore_context.color = pixels->getPixelColor(pos);
    pixel_restore_context.flash_color = flash_color;
    //LEAF_DEBUG("Flash %s@%d for %dms (then restore 0x%06X)", hex, pos, duration, pixel_restore_context.color);
    pixel_restore_context.pos = pos;
    if (clones && clones->has(pos)) {
      pixel_restore_context.clone_pos = clones->get(pos);
    }
    else {
      pixel_restore_context.clone_pos = -1;
    }
    setPixelRGB(pos, flash_color);
    show();

    flashRestoreTimer.once_ms(duration, [](){
      pixel_restore_context.leaf->pixelRestoreContext(&pixel_restore_context);
    });
    LEAF_LEAVE;
  }

  void setPixelHue(int pos, int hue)
  {
    if (count < pos) return;
    if (!pixels) return;
    pixels->setPixelColor(pos, pixels->ColorHSV(hue, sat, val));
    show();
  }

  virtual bool valueChangeHandler(String topic, Value *v) {
    LEAF_HANDLER(L_NOTICE);

    WHEN("count",{
	LEAF_WARN("Updating pixel count: %d", count);
	pixels->updateLength(count);
	if (do_check) {
	  stacx_pixel_check(pixels, check_iterations, check_delay, true);
	}
    })
    ELSEWHEN("refresh",{last_refresh=millis();show();})
    ELSEWHEN("hue",{setPixelHue(0, VALUE_AS(int,v));show();})
    ELSEWHEN("brightness",{
	if (brightness < 0) brightness=0;
	if (brightness > 255) brightness=255;
	pixels->setBrightness(brightness);
	show();
      })
    else {
      handled = Leaf::valueChangeHandler(topic, v);
    }

    LEAF_HANDLER_END;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("flash",flashPixelRGB(0, payload))
    ELSEWHENPREFIX("flash/",flashPixelRGB(topic.toInt(), payload))
    ELSEWHEN("check",{
      LEAF_NOTICE("Pixel check (count=%d)", count);
      stacx_pixel_check(pixels, check_iterations, check_delay, true);
    })
    ELSEWHEN("clone", {
	int pos=payload.indexOf("=");
	if (pos > 0) {
	  int src=payload.substring(0,pos).toInt();
	  int dst=payload.substring(pos+1).toInt();
	  clone(src,dst);
	}
      })
    ELSEWHEN("unclone",{
      unclone(payload.toInt());
    })
    ELSEWHEN("list_clones",{
	count = clones->size();
	for (int i=0; i<count; i++) {
	  uint16_t a=clones->getKey(i);
	  uint16_t b=clones->getData(i);
	  mqtt_publish("status/clone/"+String((int)a,10), String((int)b,10));
	}
      })

    else handled = Leaf::commandHandler(type, name, topic, payload);

    LEAF_HANDLER_END;
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    bool handled = false;
    LEAF_INFO("PIXEL RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("set/color",{
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
      handled = Leaf::mqtt_receive(type,name,topic,payload, direct);
    }
    return handled;
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
      //LEAF_DEBUG("pixel refresh");
      show();
    }
  }

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
