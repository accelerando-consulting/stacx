#pragma once

//@***************************** class AbstractDisplayLeaf ******************************

class AbstractDisplayLeaf : public Leaf
{
protected:
  int width = -1;
  int height = -1;

  uint8_t rotation = 0;
  int w=-1,h=-1; // element width/height
  int row=0;
  int column=0;
  int textheight=10;
  int font=0;
  uint32_t color = 0;
  String alignment="";
  bool pause_updates = false;

public:
  AbstractDisplayLeaf(String name, uint8_t rotation=0)
    : Leaf("display", name, (pinmask_t)0)
    , Debuggable(name)
  {
    this->rotation = rotation;
  }

  virtual int getWidth()  { return width; }
  virtual int getHeight() { return height; }
  virtual int getRotation() { return rotation; }

  virtual void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);
    row=0;
    column=0;

    registerLeafByteValue("rotation", &rotation, "set the display rotation");
    registerLeafIntValue("row", &row, "set the cursor row");
    registerLeafIntValue("column", &column, "set the cursor column");
    registerLeafIntValue("font", &font, "set the display font");
    registerLeafStrValue("alignment", &alignment, "set the display text aligmnent");

    registerCommand(HERE, "clear", "clear the device display");
    registerCommand(HERE, "print", "print to the device display");
    registerCommand(HERE, "draw", "draw on the device display");

    registerLeafCommand(HERE, "pause", "pause display updates (1=on 0=off)");
    registerLeafCommand(HERE, "update", "manual display update");

    LEAF_LEAVE;
  }

  virtual void loop() {
    Leaf::loop();
    if (!pause_updates) {
      update();
    }
  }

protected:

  virtual void setAlignment(String payload){}
  virtual void setFont(int font){}
  virtual void setRotation(uint8_t rotation){}
  virtual void setTextColor(uint32_t color){}
  virtual void clearScreen(){}
  virtual void update(){}
  virtual void drawLine(int x1, int y1, int x2, int y2, uint32_t color){}
  virtual void drawString(const char *text, int column, int row){};
  virtual void drawRect(int x1, int y1, int x2, int y2, uint32_t color){}
  virtual void drawFilledRect(int x1, int y1, int x2, int y2, uint32_t color){}

  virtual void draw(const JsonObject &obj) {
    LEAF_ENTER(L_DEBUG);

    if (obj["row"].is<int>()) row = obj["row"];
    if (obj["r"].is<int>()) row = obj["r"];
    if (obj["column"].is<int>()) column = obj["column"];
    if (obj["c"].is<int>()) column = obj["c"];
    if (obj["color"].is<int>()) color = obj["color"];
    if (obj["font"].is<int>()) setFont(obj["font"]);
    if (obj["f"].is<const char*>()) setFont(obj["f"]);
    if (obj["align"].is<const char*>()) setAlignment(obj["align"]);
    if (obj["a"].is<const char*>()) setAlignment(obj["a"]);
    if (obj["w"].is<int>()) w = obj["w"];
    if (obj["h"].is<int>()) w = obj["h"];
    if (obj["line"].is<Array>() || obj["l"].is<Array>()) {
      JsonArray coords = (obj["line"].is<Array>())?obj["line"]:obj["l"];
      drawLine(coords[0],coords[1],coords[2],coords[3], color);
    }
    if (obj["rect"].is<Array>() || obj["r"].is<Array>()) {
      JsonArray coords = (obj["rect"].is<Array>())?obj["rect"]:obj["r"];
      drawRect(coords[0],coords[1],coords[2],coords[3], color);
    }
    if (obj["filledrect"].is<Array>() || obj["f"].is<Array>()) {
      JsonArray coords = (obj["filledrect"].is<Array>())?obj["filledrect"]:obj["f"];
      drawFilledRect(coords[0],coords[1],coords[2],coords[3], color);
    }
    if (obj["text"].is<const char*>() || obj["t"].is<const char*>()) {
      const char *txt;
      if (obj["t"].is<const char*>()) {
	 txt = obj["t"].as<const char*>();
      }
      else {
	 txt = obj["text"].as<const char*>();
      }
      //LEAF_INFO("TEXT @[%d,%d]: %s", row, column, txt);
      setTextColor(color);
      drawString(txt, column, row);
    }
#ifdef NOTYET
    if (obj["sparkline"].is<Array>() || obj["s"].is<Array>()) {

    }
#endif

    LEAF_LEAVE;
  }

public:
  virtual void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();

    LEAF_LEAVE;
  }

  virtual void status_pub()
  {
    LEAF_ENTER(L_DEBUG);
    mqtt_publish("status/size", String(width,DEC)+"x"+String(height,DEC));
  }

  virtual bool valueChangeHandler(String topic, Value *val)
  {
    LEAF_HANDLER(L_INFO);

    WHEN("font", setFont(VALUE_AS(int, val)))
    ELSEWHEN("rotation", setRotation(VALUE_AS(int, val)))
    ELSEWHEN("alignment", setAlignment(VALUE_AS(String, val)))
    else handled = Leaf::valueChangeHandler(topic, val);
    
    LEAF_HANDLER_END;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    bool handled = false;
    WHEN("clear",{
	clearScreen();
    })
    ELSEWHEN("print",{
	drawString(payload.c_str(), column, row);
    })
    ELSEWHEN("draw",{
	JsonDocument doc;
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
      })
      ELSEWHEN("pause", {
	  pause_updates=parseBool(payload);
	  LEAF_NOTICE("pause_updates set %s", ABILITY(pause_updates));
      })
      ELSEWHEN("update", {
	  LEAF_NOTICE("manual update");
	  update();
      })
    else handled = Leaf::commandHandler(type, name, topic, payload);

    return handled;
  }
  
};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
