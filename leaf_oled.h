#include "SSD1306Wire.h"

//@***************************** class OLEDLeaf ******************************

#ifndef OLED_GEOMETRY
#define OLED_GEOMETRY GEOMETRY_128_32
#endif

class OledLeaf : public Leaf
{
  SSD1306Wire *oled = NULL;
  uint8_t addr;
  int width; // screen width/height
  int height;
  int row;
  int column;
  int w,h; // element width/height
  int textheight=10;

public:
  OledLeaf(String name,
	   uint8_t _addr=0x3c, uint8_t _sda=SDA, uint8_t _scl=SCL,
	   OLEDDISPLAY_GEOMETRY = OLED_GEOMETRY)
    : Leaf("oled", name, (pinmask_t)0) {
    this->addr = addr;
#if !USE_OLED
    this->oled = new SSD1306Wire(_addr, _sda, _scl);
    this->oled->init();
    Wire.setClock(100000);
#endif
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
#if USE_OLED
    this->oled = _oled;
#endif
    LEAF_DEBUG("oled=%p", oled);

    width = 128;//oled->getWidth();
    height = 64;// oled->getHeight();
    row=0;
	    
    column=0;
    if (oled == NULL) {
      LEAF_ALERT("OLED not initialised");
      return;
    }

    oled->clear();
    oled->display();
    oled->flipScreenVertically();
    oled->setFont(ArialMT_Plain_10);
    oled->setTextAlignment(TEXT_ALIGN_LEFT);
    String msg = String("Stacx ")+mac_short;
    oled->drawString(0, 0, msg);
    oled->display();
    LEAF_NOTICE("%s is %dx%d on I2C at address 0x%02X", base_topic.c_str(), width, height, (int)addr);

    LEAF_LEAVE;
  }

protected:
  
  void setAlignment(String payload)
  {
    LEAF_ENTER(L_DEBUG);

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
  }

  void setFont(int font)
  {
    LEAF_ENTER(L_DEBUG);

    switch (font) {
    case 24:
      oled->setFont(ArialMT_Plain_24);
      textheight=24;
      break;
    case 16:
      oled->setFont(ArialMT_Plain_16);
      textheight=16;
      break;
    case 10:
    default:
      oled->setFont(ArialMT_Plain_10);
      textheight=10;
      break;
    }
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
      oled->drawLine(coords[0],coords[1],coords[2],coords[3]);
    }
    if (obj.containsKey("text") || obj.containsKey("t")) {
      const char *txt;
      if (obj.containsKey("t")) {
	 txt = obj["t"].as<const char*>();
      }
      else {
	 txt = obj["text"].as<const char*>();
      }
      LEAF_DEBUG("TEXT @[%d,%d]: %s", row, column, txt);
      OLEDDISPLAY_COLOR textcolor = oled->getColor();
      oled->setColor(BLACK);
      int textwidth = 64;//oled->getStringWidth(txt);
      //LEAF_DEBUG("blanking %d,%d %d,%d for %s", column, row, textheight+1, textwidth, txt);
      oled->fillRect(column, row, textwidth, textheight);
      oled->setColor(textcolor);
      oled->drawString(column, row, txt);
    }
    if (obj.containsKey("sparkline") || obj.containsKey("s")) {
      
    }
    

    // TODO: implement line, rect, filledrect
    LEAF_LEAVE;
  }

public:

  void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();
    mqtt_subscribe("set/row");
    mqtt_subscribe("set/column");
    mqtt_subscribe("set/font");
    mqtt_subscribe("cmd/clear");
    mqtt_subscribe("cmd/print");
    mqtt_subscribe("cmd/draw");
    LEAF_LEAVE;
  }

  void status_pub()
  {
    LEAF_ENTER(L_DEBUG);
    mqtt_publish("status/size", String(width,DEC)+"x"+String(height,DEC));
  }

  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    static DynamicJsonDocument doc(1024);

    /*
    if (type=="app") {
      LEAF_NOTICE("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }
    */
    
    WHEN("set/row",{
      LEAF_DEBUG("Updating row via set operation");
      row = payload.toInt();
    })
    ELSEWHEN("set/column",{
      LEAF_DEBUG("Updating column via set operation");
      column = payload.toInt();
      status_pub();
    })
    ELSEWHEN("set/font",{
      LEAF_DEBUG("Updating font via set operation");
      setFont(payload.toInt());
    })
    ELSEWHEN("set/alignment",{
      LEAF_DEBUG("Updating alignment via set operation");
      payload.toLowerCase();
      setAlignment(payload);
    })
    ELSEWHEN("cmd/clear",{
	oled->clear();
	oled->display();
    })
    ELSEWHEN("cmd/print",{
	oled->drawString(column, row, payload.c_str());
	oled->display();
    })
    ELSEWHEN("cmd/draw",{
	LEAF_DEBUG("Draw command: %s", payload.c_str());

	//DynamicJsonDocument doc(payload.length()*4);
	deserializeJson(doc, payload);
	if (doc.is<JsonObject>()) {
	  JsonObject obj = doc.as<JsonObject>();
	  draw(obj);
	  oled->display();
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
	  oled->display();
	}
	else {
	  LEAF_ALERT("cmd/draw payload is neither array nor object");
	}
    })

    LEAF_LEAVE;
    return handled;
  };

  void loop()
  {
  }



};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
