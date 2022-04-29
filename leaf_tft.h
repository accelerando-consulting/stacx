
#include <TFT_eSPI.h>

//@***************************** class TFTLeaf ******************************

TFT_eSPI tftObj = TFT_eSPI(TFT_WIDTH,TFT_HEIGHT);

class TFTLeaf : public Leaf
{
  TFT_eSPI *tft = NULL;
  int width = TFT_WIDTH; // screen width/height
  int height = TFT_HEIGHT;
  int row;
  int column;
  int w,h; // element width/height
  int textheight=10;
  uint8_t rotation = 0;
  uint16_t color = TFT_WHITE;

public:
  TFTLeaf(String name, uint8_t rotation=0)
    : Leaf("tft", name, (pinmask_t)0) {
    this->rotation = rotation;
  }

  void setup(void) {
    Leaf::setup();
    debug_flush=true;
    LEAF_ENTER(L_NOTICE);
    //this->tft = new TFT_eSPI();
    this->tft = &tftObj;
    LEAF_NOTICE("tft=%p", tft);
    
    row=0;
    column=0;

    LEAF_NOTICE("tft init");
    tft->init();
    //LEAF_NOTICE("tft invert");
    //tft->invertDisplay(1);
    LEAF_NOTICE("tft setrotation %d", rotation);
    tft->setRotation(rotation);
    LEAF_NOTICE("tft clear");
    tft->fillScreen(TFT_BLACK);
    LEAF_NOTICE("tft home");
    tft->setCursor(5, 5);
    LEAF_NOTICE("tft setcolor");
    tft->setTextColor(color);
    LEAF_NOTICE("tft setwrap");
    tft->setTextWrap(true);
    LEAF_NOTICE("tft print");
    tft->print(String("Stacx ")+mac_short);
    
    LEAF_NOTICE("%s is %dx%d", base_topic.c_str(), width, height);
    debug_flush = false;
    
    LEAF_LEAVE;
  }

protected:
  
  void setAlignment(String payload)
  {
    LEAF_ENTER(L_DEBUG);

    if (payload.equals("left")) {
      tft->setTextDatum(TL_DATUM);
    }
    else if (payload.equals("right")) {
      tft->setTextDatum(TR_DATUM);
    }
    else if (payload.equals("both")) {
      tft->setTextDatum(MC_DATUM);
    }
    else {
      // default is center
      tft->setTextDatum(TC_DATUM);
    }
  }

  void setFont(int font)
  {
    LEAF_ENTER(L_DEBUG);

    switch (font) {
    case 48:
      tft->setTextFont(6);
      textheight=26;
      break;
    case 24:
    case 26:
      tft->setTextFont(4);
      textheight=26;
      break;
    case 18: 
    case 16: 
      tft->setTextFont(2);
      textheight=16;
      break;
    case 10:
    case 9:
    case 8:
    default:
      tft->setTextFont(1);
      textheight=8;
      break;
    }
  }

  void draw(const JsonObject &obj) {
    LEAF_ENTER(L_DEBUG);

    if (obj.containsKey("row")) row = obj["row"];
    if (obj.containsKey("r")) row = obj["r"];
    if (obj.containsKey("column")) column = obj["column"];
    if (obj.containsKey("c")) column = obj["c"];
    if (obj.containsKey("color")) color = obj["color"];
    if (obj.containsKey("font")) setFont(obj["font"]);
    if (obj.containsKey("f")) setFont(obj["f"]);
    if (obj.containsKey("align")) setAlignment(obj["align"]);
    if (obj.containsKey("a")) setAlignment(obj["a"]);
    if (obj.containsKey("w")) w = obj["w"];
    if (obj.containsKey("h")) w = obj["h"];
    if (obj.containsKey("line") || obj.containsKey("l")) {
      JsonArray coords = obj.containsKey("line")?obj["line"]:obj["l"];
      tft->drawLine(coords[0],coords[1],coords[2],coords[3], color);
    }
    if (obj.containsKey("text") || obj.containsKey("t")) {
      const char *txt;
      if (obj.containsKey("t")) {
	 txt = obj["t"].as<const char*>();
      }
      else {
	 txt = obj["text"].as<const char*>();
      }
      LEAF_INFO("TEXT @[%d,%d]: %s", row, column, txt);
      tft->setTextColor(color);
      tft->drawString(txt, column, row);
      tft->setTextColor(color);
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
    mqtt_subscribe("set/rotation");
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
    ELSEWHEN("set/rotation",{
      LEAF_DEBUG("Updating font via set operation");
      rotation = (uint8_t)payload.toInt();
      tft->setRotation(rotation);
    })
    ELSEWHEN("set/alignment",{
      LEAF_DEBUG("Updating alignment via set operation");
      payload.toLowerCase();
      setAlignment(payload);
    })
    ELSEWHEN("cmd/clear",{
	tft->fillScreen(TFT_BLACK);
    })
    ELSEWHEN("cmd/print",{
	tft->drawString(payload.c_str(), column, row);
    })
    ELSEWHEN("cmd/draw",{
	LEAF_DEBUG("Draw command: %s", payload.c_str());

	//DynamicJsonDocument doc(payload.length()*4);
	deserializeJson(doc, payload);
	if (doc.is<JsonObject>()) {
	  JsonObject obj = doc.as<JsonObject>();
	  draw(obj);
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
