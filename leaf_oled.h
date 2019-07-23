#include "SSD1306Wire.h"

//@***************************** class OLEDLeaf ******************************

class OledLeaf : public Leaf
{
  SSD1306Wire *oled;
  int width;
  int height;
  int row;
  int column;
  int textheight=10;

public:
  OledLeaf(String name, pinmask_t pins=0, uint8_t _addr=0x3c, uint8_t _sda=21, uint8_t _scl=22, OLEDDISPLAY_GEOMETRY = GEOMETRY_128_64) : Leaf("oled", name, pins) {
    oled = new SSD1306Wire(_addr, _sda, _scl);
  }

  void setup(void) {
    Leaf::setup();
    oled->init();
    Wire.setClock(100000);
    width = oled->getWidth();
    height = oled->getHeight();
    row=0;
    column=0;

    oled->flipScreenVertically();
    oled->setFont(ArialMT_Plain_10);
    oled->setTextAlignment(TEXT_ALIGN_LEFT);
    String msg = String("stacx s/n ")+mac_short;
    oled->drawString(0, 0, msg);
#if 0
    oled->drawString(0, 10, "yo!");
    oled->drawString(64, 10, "bo");
    oled->drawString(0, 20, "go");
    oled->drawString(64, 20, "ro");
    oled->drawString(0, 30, "sho");
    oled->drawString(64, 30, "ko");
    oled->drawString(0, 40, "jo");
    oled->drawString(64, 40, "no");
    oled->drawString(0, 50, "lo");
    oled->drawString(64, 50, "po");
#endif
    oled->display();
  }

protected:

  void setAlignment(String payload)
  {
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
    ENTER(L_DEBUG);

    if (obj.containsKey("row")) row = obj["row"];
    if (obj.containsKey("r")) row = obj["r"];
    if (obj.containsKey("column")) column = obj["column"];
    if (obj.containsKey("c")) column = obj["c"];
    if (obj.containsKey("font")) setFont(obj["font"]);
    if (obj.containsKey("f")) setFont(obj["f"]);
    if (obj.containsKey("align")) setAlignment(obj["align"]);
    if (obj.containsKey("a")) setAlignment(obj["a"]);
    if (obj.containsKey("text") || obj.containsKey("t")) {
      const char *txt;
      if (obj.containsKey("t")) {
	 txt = obj["t"].as<char*>();
      }
      else {
	 txt = obj["text"].as<char*>();
      }
      DEBUG("TEXT @[%d,%d]: %s", row, column, txt);
      OLEDDISPLAY_COLOR textcolor = oled->getColor();
      oled->setColor(BLACK);
      int textwidth = 64;//oled->getStringWidth(txt);
      //DEBUG("blanking %d,%d %d,%d for %s", column, row, textheight+1, textwidth, txt);
      oled->fillRect(column, row, textwidth, textheight);
      oled->setColor(textcolor);
      oled->drawString(column, row, txt);
    }
    // TODO: implement line, rect, filledrect
    LEAVE;
  }

public:

  void mqtt_subscribe() {
    ENTER(L_NOTICE);
    Leaf::mqtt_subscribe();
    _mqtt_subscribe(base_topic+"/set/row");
    _mqtt_subscribe(base_topic+"/set/column");
    _mqtt_subscribe(base_topic+"/set/font");
    _mqtt_subscribe(base_topic+"/cmd/clear");
    _mqtt_subscribe(base_topic+"/cmd/print");
    _mqtt_subscribe(base_topic+"/cmd/draw");
    LEAVE;
  }

  void status_pub()
  {
    mqtt_publish("status/size", String(width,DEC)+"x"+String(height,DEC));
  }

  bool mqtt_receive(String type, String name, String topic, String payload) {
    ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);
    static DynamicJsonDocument doc(1024);

    WHEN("set/row",{
      DEBUG("Updating row via set operation");
      row = payload.toInt();
    })
    ELSEWHEN("set/column",{
      DEBUG("Updating column via set operation");
      column = payload.toInt();
      status_pub();
    })
    ELSEWHEN("set/font",{
      DEBUG("Updating font via set operation");
      setFont(payload.toInt());
    })
    ELSEWHEN("set/alignment",{
      DEBUG("Updating alignment via set operation");
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
	DEBUG("Draw command: %s", payload.c_str());

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
	      ALERT("cmd/draw payload element %d is not valid", i);
	    }
	  }
	  oled->display();
	}
	else {
	  ALERT("cmd/draw payload is neither array nor object");
	}
    })

    LEAVE;
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
