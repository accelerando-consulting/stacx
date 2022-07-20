#pragma once

//@***************************** class AbstractDisplayLeaf ******************************

class AbstractDisplayLeaf : public Leaf
{
protected:
  int width = -1;
  int height = -1;
	
  int row;
  int column;
  int w,h; // element width/height
  int textheight=10;
  uint8_t rotation = 0;
  uint32_t color = 0;

public:
  AbstractDisplayLeaf(String name, uint8_t rotation=0)
    : Leaf("display", name, (pinmask_t)0) {
    this->rotation = rotation;
  }

  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
    row=0;
    column=0;
    LEAF_LEAVE;
  }

protected:
  
  virtual void setAlignment(String payload){}
  virtual void setFont(int font){}
  virtual void setRotation(uint8_t rotation){}
  virtual void setTextColor(uint32_t color){}
  virtual void clearScreen(){}

  virtual void drawLine(int x1, int y1, int x2, int y2, uint32_t color){}
  virtual void drawString(const char *text, int column, int row){};
  virtual void drawRect(int x1, int y1, int x2, int y2, uint32_t color){}
  virtual void drawFilledRect(int x1, int y1, int x2, int y2, uint32_t color){}

  virtual void draw(const JsonObject &obj) {
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
      drawLine(coords[0],coords[1],coords[2],coords[3], color);
    }
    if (obj.containsKey("rect") || obj.containsKey("r")) {
      JsonArray coords = obj.containsKey("rect")?obj["rect"]:obj["r"];
      drawRect(coords[0],coords[1],coords[2],coords[3], color);
    }
    if (obj.containsKey("filledrect") || obj.containsKey("f")) {
      JsonArray coords = obj.containsKey("filledrect")?obj["filledrect"]:obj["f"];
      drawFilledRect(coords[0],coords[1],coords[2],coords[3], color);
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
      setTextColor(color);
      drawString(txt, column, row);
    }
    if (obj.containsKey("sparkline") || obj.containsKey("s")) {
      
    }

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
      setRotation(rotation);
    })
    ELSEWHEN("set/alignment",{
      LEAF_DEBUG("Updating alignment via set operation");
      payload.toLowerCase();
      setAlignment(payload);
    })
    ELSEWHEN("cmd/clear",{
	clearScreen();
    })
    ELSEWHEN("cmd/print",{
	drawString(payload.c_str(), column, row);
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
