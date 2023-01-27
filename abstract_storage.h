#pragma once
//
//@**************************** class StorageLeaf *****************************
//
// Storage for user setpoints
//
class StorageLeaf : public Leaf
{
protected:
  //
  // Declare your leaf-specific instance data here
  //
  SimpleMap<String,String> *values=NULL;
  SimpleMap<String,String> *pref_defaults=NULL;
  SimpleMap<String,String> *pref_descriptions=NULL;

public:
  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  StorageLeaf(String name, String defaults="")
    : Leaf("storage", name, 0)
    , Debuggable(name)
  {
    values = new SimpleMap<String,String>(_compareStringKeys);
    pref_defaults = new SimpleMap<String,String>(_compareStringKeys);
#ifndef ESP8266
    // save RAM on ESP8266 by not storing help
    pref_descriptions = new SimpleMap<String,String>(_compareStringKeys);
#endif
    int pos ;
    String key;
    impersonate_backplane = true;

    if (defaults.length() > 0) {
      String p = defaults;
      int pos ;
      do {
	String pref_name;
	String pref_value;

	if ((pos = p.indexOf(',')) > 0) {
	  pref_name = p.substring(0, pos);
	  p.remove(0,pos+1);
	}
	else {
	  pref_name = p;
	  p="";
	}

	if ((pos = pref_name.indexOf('=')) > 0) {
	  pref_value = pref_name.substring(0, pos);
	  pref_name.remove(0,pos+1);
	}
	else {
	  pref_value = "";
	}
	//LEAF_INFO("Default preference value %s => %s", pref_name.c_str(), pref_value.c_str());

	this->put(pref_name, pref_value);

      } while (p.length() > 0);
    }
  }

  virtual void load(String name="") {};
  virtual void save(String name="", bool force_format=false) {};

  virtual bool has(String name)
  {
    return values->has(name);
  }
  virtual void remove(String name, bool no_save=false)
  {
    pref_defaults->remove(name);
    if (pref_descriptions) {
      pref_descriptions->remove(name);
    }
    values->remove(name);
  }

  virtual String get(String name, String defaultValue="", String description="")
  {
    // auto populate default and help string tables
    if (defaultValue.length() && !pref_defaults->has(name)) {
      pref_defaults->put(name, defaultValue);
    }
    if (description.length() && pref_descriptions && !pref_descriptions->has(name)) {
      pref_descriptions->put(name, description);
    }

    if (values->has(name)) {
      String result = values->get(name);
      LEAF_INFO("getPref [%s] <= [%s]", name.c_str(), result.c_str());
      return result;
    }
    else {
      if (defaultValue != "") {
	LEAF_DEBUG("getPref [%s] <= [%s] (default)", name.c_str(), defaultValue.c_str());
      }
      return defaultValue;
    }
  }

  virtual byte getByte(String name, byte defaultValue=0, String description="")
  {
    String s = get(name, String(defaultValue), description);
    if (s.length()) return s.toInt();
    //LEAF_INFO("getByte [%s] <= DEFAULT (%d)", name.c_str(), defaultValue);
    return defaultValue;
  }

  virtual int getInt(String name, int defaultValue=0, String description="")
  {
    String s = get(name, String(defaultValue), description);
    if (s.length()) return s.toInt();
    //LEAF_INFO("getInt [%s] <= DEFAULT (%d)", name.c_str(), defaultValue);
    return defaultValue;
  }

  virtual int getBool(String name, bool defaultValue=false, String description="")
  {
    String s = get(name, String(defaultValue), description);
    bool valid = false;
    if (s.length()) {
      bool result = parseBool(s, defaultValue, &valid);
      if (!valid) result = defaultValue;
      return result;
    }

    //LEAF_INFO("getBool [%s] <= DEFAULT (%s)", name.c_str(), ABILITY(defaultValue));
    return defaultValue;
  }

  virtual unsigned long getULong(String name, unsigned long defaultValue=0, String description="")
  {
    String s = get(name, String(defaultValue), description);
    if (s.length()) {
      char buf[32];
      strncpy(buf, s.c_str(), sizeof(buf));
      return strtoul(buf, NULL, 10);
    }
    //LEAF_INFO("getULong [%s] <= DEFAULT (%lu)", name.c_str(), defaultValue);
    return defaultValue;
  }

  virtual float getFloat(String name, float defaultValue=0, String description="")
  {
    String s = get(name, String(defaultValue), description);
    if (s.length()) return s.toFloat();
    //LEAF_INFO("getFloat [%s] <= DEFAULT (%f)", name.c_str(), defaultValue);
    return defaultValue;
  }

  virtual double getDouble(String name, double defaultValue=0, String description="")
  {
    String s = get(name, String(defaultValue), description);
    if (s.length()) return strtod(s.c_str(),NULL);
    //LEAF_INFO("getDouble [%s] <= DEFAULT (%lf)", name.c_str(), defaultValue);
    return defaultValue;
  }

  virtual void put(String name, String value, bool no_save=false)
  {
    values->put(name, value);
  }
  virtual void set_description(String name, String value)
  {
    if (pref_descriptions && (value.length()>0)) {
      pref_descriptions->put(name, value);
    }
  }
  virtual void set_default(String name, String value)
  {
    pref_defaults->put(name, value);
  }

  virtual void putByte(String name, byte value)
  {
    put(name, String((int)value));
  }

  virtual void putInt(String name, int value)
  {
    put(name, String(value));
  }

  virtual void putULong(String name, unsigned long value)
  {
    put(name, String(value));
  }

  virtual void putFloat(String name, float value)
  {
    put(name, String(value));
  }

  virtual void putDouble(String name, double value)
  {
    put(name, String(value));
  }

  virtual void setup(void) {
    if (!setup_done) Leaf::setup();
    registerCommand(HERE,"save", "save preferences to non-volatile memory");
    registerCommand(HERE,"load", "load preferences from non-volatile memory");
    registerCommand(HERE,"prefs", "List all non-default preference values");
    registerStrValue("pref/+", NULL, "Get/Set a preference value (name in topic)");
    registerStrValue("pref", NULL, "Get/Set a preference value (name in payload)");
    this->load();
  }

  virtual bool wants_topic(String type, String name, String topic)
  {
    //LEAF_NOTICE("topic=%s", topic.c_str());
    if (topic.startsWith("set/pref")) return true;
    if (topic.startsWith("get/pref")) return true;
    if (topic=="cmd/save") return true;
    if (topic=="cmd/load") return true;
    if (topic=="cmd/prefs") return true;
    if (topic=="cmd/help") return true;
    return false;
  }

  virtual void status_pub()
  {
    for (int i=0; i<values->size(); i++) {
      //pubsubLeaf->_mqtt_publish("status/"+values->getKey(i), values->getData(i));
      mqtt_publish("status/"+values->getKey(i), values->getData(i), 0, false);
    }
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    String key,desc,value,dfl;
    static char help_buf[256];

    //LEAF_DEBUG("storage mqtt_receive %s %s => %s [%s]", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHENPREFIX("set/pref/",{
      LEAF_INFO("prefs set/pref [%s] <= [%s]", topic.c_str(), payload.c_str());
      if (payload.length()) {
	this->put(topic, payload);
	mqtt_publish("status/pref/"+topic, payload,0);
      }
      else {
	LEAF_INFO("Remove preference [%s]", topic.c_str());
	this->remove(topic);
      }
      handled = true;
      })
    ELSEWHEN("set/pref",{
      // canonical syntax is "set/pref/foo bar" but "set/pref foo bar" is a
      // common typo
      int split = payload.indexOf(' ');
      if (split > 0) {
	topic = payload.substring(0,split);
	payload.remove(0, split+1);
      }
      LEAF_INFO("prefs set/pref [%s] <= [%s]", topic.c_str(), payload.c_str());

      if (payload.length()) {
	this->put(topic, payload);
	mqtt_publish("status/pref/"+topic, payload,0);
      }
      else {
	LEAF_INFO("Remove preference [%s]", topic.c_str());
	this->remove(topic);
      }
      handled = true;
      })
    ELSEWHENPREFIX("get/pref/",{
      String value = this->get(topic);
      if (value.length()==0) {
	value = "[empty]";
      }
      LEAF_INFO("prefs get/pref/%s => %s", payload.c_str(), value.c_str());
      mqtt_publish("status/pref/"+topic, value,0);
      handled = true;
    })
    ELSEWHEN("get/pref", {
      String value = this->get(payload);
      LEAF_INFO("prefs get/pref %s => %s", payload.c_str(), value.c_str());
      if (value.length()==0) {
	value = "[empty]";
      }
      mqtt_publish("status/pref/"+payload, value,0);
      handled = true;
      })
    ELSEWHEN("cmd/prefs", {
	//LEAF_INFO("Listing prefs");
      String filter="";
      if ((payload != "") && (payload != "1")) {
	filter=payload;
      }
      for (int i=0; i < values->size(); i++) {
	key = values->getKey(i);
	if (filter.length() && (key.indexOf(filter)<0)) {
	  // this key does not match filter
	  continue;
	}

	value = values->getData(i);
	if (value.length()==0) {
	  value = "[empty]";
	}
	//LEAF_INFO("Print preference value [%s] <= [%s]", key.c_str(), value.c_str());
	mqtt_publish("status/pref/"+key, value, 0);
      }
      })
    ELSEWHEN("cmd/help", {
	String filter = "";
	int pos;
	if ((pos=payload.indexOf(" ")) > 0) {
	  filter = payload.substring(pos+1);
	  payload.remove(pos);
	}
	if (pref_descriptions && (payload == "" || (payload == "pref") || (payload == "prefs"))) {
	  int count = pref_descriptions->size();
	  // descriptions stored in the pref leaf are DEPRECATED, use registerValue() instead
	  if (count) {
	    LEAF_WARN("There are %d legacy non-Value preferences", count);
	  }
	  for (int i=0; i < count; i++) {
	    key = pref_descriptions->getKey(i);
	    if (filter.length() && (key.indexOf(filter)<0)) {
	      // this key does not match filter
	      continue;
	    }
	    desc = pref_descriptions->getData(i);
	    value = values->get(key);
	    dfl = pref_defaults->get(key);
	    int sz = snprintf(help_buf, sizeof(help_buf), "%s (default=[%s] stored=[%s])", desc.c_str(), dfl.c_str(), value.c_str());
	    //LEAF_INFO("Help string of size %d", sz);
	    mqtt_publish("status/help/pref/"+key, String(help_buf), 0);
	  }
	}
      })
      ELSEWHEN("cmd/load",{
	  load(payload);
	})
      ELSEWHEN("cmd/save",{
	  save(payload);
	})
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    LEAF_BOOL_RETURN_SLOW(1000, handled);
  };

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
