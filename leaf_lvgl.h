
#include <SPI.h>
#include <TFT_eSPI.h>
#include "lvgl.h"

//@**************************** class LVGLLeaf ******************************

TFT_eSPI tftObj = TFT_eSPI();

class TFTLeaf : public Leaf
{
  TFT_eSPI *tft = NULL;
  int width = TFT_WIDTH; // screen width/height
  int height = TFT_HEIGHT;
  int row;
  int column;
  int w,h; // element width/height
  int textheight=10;
  uint32_t color = TFT_WHITE;

public:
  TFTLeaf(String name)
    : Leaf("tft", name, (pinmask_t)0) {
  }

  void setup(void) {
    Leaf::setup();
    debug_flush=true;
    LEAF_ENTER(L_INFO);
    //this->tft = new TFT_eSPI();
    this->tft = &tftObj;
    LEAF_NOTICE("tft=%p", tft);
    
    row=0;
    column=0;

    LEAF_NOTICE("tft init");
    tft->init();
    LEAF_NOTICE("tft clear");
    tft->fillScreen(TFT_BLACK);
    LEAF_NOTICE("tft home");
    tft->setCursor(0, 0);
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
    case 24:
      tft->setFreeFont(&FreeMono24pt7b);
      textheight=24;
      break;
    case 18: 
    case 16: 
      tft->setFreeFont(&FreeMono18pt7b);
      textheight=18;
      break;
    case 10:
    case 9:
    default:
      tft->setFreeFont(&FreeMono9pt7b);
      textheight=9;
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
      //LEAF_DEBUG("TEXT @[%d,%d]: %s", row, column, txt);
      tft->drawString(txt, column, row);
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
    mqtt_subscribe("set/row", HERE);
    mqtt_subscribe("set/column", HERE);
    mqtt_subscribe("set/font", HERE);
    mqtt_subscribe("cmd/clear", HERE);
    mqtt_subscribe("cmd/print", HERE);
    mqtt_subscribe("cmd/draw", HERE);
    LEAF_LEAVE;
  }

  void status_pub()
  {
    LEAF_ENTER(L_DEBUG);
    mqtt_publish("status/size", String(width,DEC)+"x"+String(height,DEC));
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    static DynamicJsonDocument doc(1024);

    /*
    if (type=="app") {
      LEAF_NOTICE("RECV %s/%s => [%s <= %s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }
    */
    
    WHEN("set/row",{
	//LEAF_DEBUG("Updating row via set operation");
      row = payload.toInt();
    })
    ELSEWHEN("set/column",{
	//LEAF_DEBUG("Updating column via set operation");
      column = payload.toInt();
      status_pub();
    })
    ELSEWHEN("set/font",{
	//LEAF_DEBUG("Updating font via set operation");
      setFont(payload.toInt());
    })
    ELSEWHEN("set/alignment",{
	//LEAF_DEBUG("Updating alignment via set operation");
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
	//LEAF_DEBUG("Draw command: %s", payload.c_str());

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
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

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
