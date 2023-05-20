#include "SSD1306Wire.h"
//@***************************** class OLEDLeaf ******************************

#ifndef OLED_GEOMETRY
#define OLED_GEOMETRY GEOMETRY_128_32
#endif

class OledLeaf : public Leaf, public WireNode
{
  SSD1306Wire *oled = NULL;
  uint8_t sda;
  uint8_t scl;
  int width; // screen width/height
  int height;
  int row=0;
  int column=0;
  int font=0;
  String alignment="left";
  int w,h; // element width/height
  int textheight=10;

public:
  OledLeaf(String name,
	   uint8_t _addr=0x3c, uint8_t _sda=SDA, uint8_t _scl=SCL,
	   OLEDDISPLAY_GEOMETRY = OLED_GEOMETRY)
    : Leaf("oled", name, (pinmask_t)0)
    , WireNode(name, _addr)
    , Debuggable(name)
  {
    this->scl=_scl;
    this->sda=_sda;
  }

  int getWidth() { return width; }
  int getHeight() { return height; }


  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
#if USE_OLED
    if ((_oled != NULL) && oled_ready) {
      this->oled = _oled;
    }
#endif

    registerLeafByteValue("i2c_addr", &address, "I2C address override for OLED display (decimal)");
    registerIntValue("row", &row, "OLED cursor row");
    registerIntValue("column", &column, "OLED cursor column");
    registerIntValue("font", &font, "OLED font number");
    registerStrValue("alignment", &alignment, "OLED text alignment (left,right,center)");
    registerCommand(HERE, "clear", "clear OLED display");
    registerCommand(HERE, "print", "print text to OLED");
    registerCommand(HERE, "draw", "draw graphics to OLED display");
    
    if (oled == NULL) {
      if (wire != NULL) {
	setWireClock(100000);

	if (!probe(address)) {
	  LEAF_ALERT("OLED display not found at 0x%02x", address);
	  stop();
	  return;
	}
      }
      else {
	LEAF_WARN("WTF no wire bus");
      }

      LEAF_NOTICE("Initialise new OLED handle");
      this->oled = new SSD1306Wire(address, sda, scl);
      this->oled->init();
    }
    //LEAF_DEBUG("oled=%p", oled);

    width = 128;//oled->getWidth();
    height = 32;// oled->getHeight();
    row=0;

    column=0;
    if (oled == NULL) {
      LEAF_ALERT("OLED not initialised");
      stop();
      return;
    }

    oled->clear();
    oled->display();
    oled->flipScreenVertically();
    oled->setFont(ArialMT_Plain_10);
    oled->setTextAlignment(TEXT_ALIGN_LEFT);
    String msg = String("Stacx ")+mac_short;
    oled->drawString(0, 0, msg);
    //oled->drawRect(0,0,width,height);
    oled->display();
    LEAF_NOTICE("%s (%dx%d) claims I2C addr 0x%02x", describe().c_str(), width, height, (int)address);

    LEAF_LEAVE;
  }

protected:

  void setAlignment(String payload)
  {
    LEAF_ENTER(L_DEBUG);
    if (!started) LEAF_VOID_RETURN;

    if (payload.equals("right")) {
      oled->setTextAlignment(TEXT_ALIGN_RIGHT);
    }
    else if (payload.equals("center")) {
      oled->setTextAlignment(TEXT_ALIGN_CENTER);
    }
    else if (payload.equals("both")) {
      oled->setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    }
    else {
      oled->setTextAlignment(TEXT_ALIGN_CENTER);
    }
    LEAF_LEAVE;
  }

  void setFont(int font)
  {
    LEAF_ENTER(L_DEBUG);

    switch (font) {
    case 24:
      if (started) {
	oled->setFont(ArialMT_Plain_24);
      }
      textheight=24;
      break;
    case 16:
      if (started) {
	oled->setFont(ArialMT_Plain_16);
      }
      textheight=16;
      break;
    case 10:
    default:
      if (started) {
	oled->setFont(ArialMT_Plain_10);
      }
      textheight=10;
      break;
    }
    LEAF_LEAVE;
  }

  void draw(const JsonObject &obj) {
    LEAF_ENTER(L_DEBUG);

    if (obj.containsKey("row")) row = obj["row"];
    if (obj.containsKey("r")) row = obj["r"];
    if (obj.containsKey("column")) column = obj["column"];
    if (obj.containsKey("c")) column = obj["c"];
    if (obj.containsKey("font")) setFont(obj["font"]);
    if (obj.containsKey("f")) setFont(obj["f"]);
    if (obj.containsKey("align")) setAlignment(obj["align"]);
    if (obj.containsKey("a")) setAlignment(obj["a"]);
    if (obj.containsKey("w")) w = obj["w"];
    if (obj.containsKey("h")) w = obj["h"];
    if (obj.containsKey("line") || obj.containsKey("l")) {
      JsonArray coords = obj.containsKey("line")?obj["line"]:obj["l"];
      if (started) {
	oled->drawLine(coords[0],coords[1],coords[2],coords[3]);
      }
    }
    if (obj.containsKey("text") || obj.containsKey("t")) {
      const char *txt;
      if (obj.containsKey("t")) {
	 txt = obj["t"].as<const char*>();
      }
      else {
	 txt = obj["text"].as<const char*>();
      }
      //LEAF_DEBUG("TEXT @[%d,%d]: %s", row, column, txt);
      OLEDDISPLAY_COLOR textcolor = started?oled->getColor():WHITE;
      if (started) {
	oled->setColor(BLACK);
	int textwidth = 64;//oled->getStringWidth(txt);
	//LEAF_DEBUG("blanking %d,%d %d,%d for %s", column, row, textheight+1, textwidth, txt);
	oled->fillRect(column, row, textwidth, textheight);
	oled->setColor(textcolor);
	oled->drawString(column, row, txt);
      }
    }
    if (obj.containsKey("rect")) {
      JsonArray coords = obj["rect"];
      LEAF_NOTICE("  rect [%d,%d]:[%d,%d]", coords[0], coords[1],coords[2],coords[3]);
      if (started) {
	oled->drawRect(coords[0],coords[1],coords[2],coords[3]);
      }
    }
    if (obj.containsKey("fill")) {
      JsonArray coords = obj["fill"];
      LEAF_NOTICE("  fill [%d,%d]:[%d,%d]", coords[0], coords[1],coords[2],coords[3]);
      if (started) {
	oled->fillRect(coords[0],coords[1],coords[2],coords[3]);
      }
    }

    if (obj.containsKey("sparkline") || obj.containsKey("s")) {

    }

    LEAF_LEAVE;
  }

public:


  void status_pub()
  {
    LEAF_ENTER(L_DEBUG);
    mqtt_publish("status/size", String(width,DEC)+"x"+String(height,DEC));
  }

  virtual bool valueChangeHandler(String topic, Value *v) {
    LEAF_HANDLER(L_INFO);

    WHEN("font", setFont(font))
    ELSEWHEN("alignment", {alignment.toLowerCase(); setAlignment(alignment);})
    else handled = Leaf::valueChangeHandler(topic, v);
    LEAF_HANDLER_END;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("clear",{
      if (started) {
	  //LEAF_DEBUG("  clear");
	  oled->clear();
	  oled->display();
      }
    })
    ELSEWHEN("print",{
	if (started) {
	  oled->drawString(column, row, payload.c_str());
	  oled->display();
	}
    })
    ELSEWHEN("draw",{
	//LEAF_DEBUG("  draw %s%s", payload.c_str(),started?"":" (NODISP)");

	DynamicJsonDocument doc(payload.length()*4);
	deserializeJson(doc, payload);
	if (doc.is<JsonObject>()) {
	  JsonObject obj = doc.as<JsonObject>();
	  draw(obj);
	  if (started) {
	    oled->display();
	  }
	}
	else if (doc.is<JsonArray>()) {
	  JsonArray arr = doc.as<JsonArray>();
	  int size = arr.size();
	  for (int i = 0; i < size; i++) {
	    if (arr[i].is<JsonObject>()) {
	      draw(arr[i].as<JsonObject>());
	    }
	    else {
	      LEAF_ALERT("cmd/draw payload element %d is not valid", i);
	    }
	  }
	  if (started) {
	    oled->display();
	  }
	}
	else {
	  LEAF_ALERT("cmd/draw payload is neither array nor object");
	}
    })
    else {
      handled = Leaf::commandHandler(type, name, topic, payload);
    }
      
    LEAF_HANDLER_END;
  }

  void loop()
  {
  }



};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
