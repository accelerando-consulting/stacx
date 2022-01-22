#ifndef _LEAF_STORAGE_ABSTRACT_H
#define _LEAF_STORAGE_ABSTRACT_H

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
  SimpleMap<String,String> *values;


public:
  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  StorageLeaf(String name, String defaults="") : Leaf("storage", name, 0) {
    values = new SimpleMap<String,String>(_compareStringKeys);
    int pos ;
    String key;

    if (defaults.length() > 0) {
      //LEAF_DEBUG("Parsing dfeault preference specifier %s", defaults.c_str());
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
	//LEAF_DEBUG("Parsing preference instance %s", pref_name.c_str());

	if ((pos = pref_name.indexOf('=')) > 0) {
	  pref_value = pref_name.substring(0, pos);
	  pref_name.remove(0,pos+1);
	}
	else {
	  pref_value = "";
	}
	LEAF_INFO("Default preference value %s => %s", pref_name.c_str(), pref_value.c_str());

	this->put(pref_name, pref_value);

      } while (p.length() > 0);
    }
  }

  virtual void load(void) {};
  virtual void save(bool force=false) {};

  virtual bool has(String name)
  {
    return values->has(name);
  }

  virtual String get(String name, String defaultValue="")
  {
    if (values->has(name)) {
      return values->get(name);
    }
    return defaultValue;
  }

  virtual int getInt(String name, int defaultValue=0)
  {
    String s = get(name);
    if (s.length()) return s.toInt();
    return defaultValue;
  }

  virtual float getFloat(String name, float defaultValue=0)
  {
    String s = get(name);
    if (s.length()) return s.toFloat();
    return defaultValue;
  }

  virtual double getDouble(String name, double defaultValue=0)
  {
    String s = get(name);
    if (s.length()) return strtod(s.c_str(),NULL);
    return defaultValue;
  }

  virtual void put(String name, String value)
  {
    values->put(name, value);
  }

  virtual void putInt(String name, int value)
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

  void setup(void) {
    Leaf::setup();
    this->load();
  }

  virtual void mqtt_do_subscribe() {
    LEAF_ENTER(L_DEBUG);
    Leaf::mqtt_do_subscribe();

#if 0
      // Don't need this, the pubsub leaf does a blanket sub
    for (int i=0; i<values->size(); i++) {
      mqtt_subscribe("set/"+values->getKey(i));
      mqtt_subscribe("status/"+values->getKey(i));
    }
#endif

    LEAF_LEAVE;
  }

  void status_pub()
  {
    for (int i=0; i<values->size(); i++) {
      //pubsubLeaf->_mqtt_publish("status/"+values->getKey(i), values->getData(i));
      mqtt_publish("status/"+values->getKey(i), values->getData(i), 0, false);
    }
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    //LEAF_DEBUG("storage mqtt_receive %s %s => %s", type.c_str(), name.c_str(), topic.c_str());

    if (topic.startsWith("set/pref/")) {
      String name = topic.substring(9);
      LEAF_NOTICE("prefs set/pref %s <= %s", name.c_str(), payload.c_str());
      this->put(name, payload);
      mqtt_publish("status/pref/"+name, payload,0);
      handled = true;
    }
    else if (topic.startsWith("get/pref/")) {
      String name = topic.substring(9);
      String value = this->get(name);
      if (value.length()==0) {
	value = "[empty]";
      }
      LEAF_NOTICE("prefs get/pref/%s => %s", payload.c_str(), value.c_str());
      mqtt_publish("status/pref/"+payload, value,0);
      handled = true;
    }
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
      for (int i=0; i < values->size(); i++) {
	String key = values->getKey(i);
	String value = values->getData(i);
	if (value.length()==0) {
	  value = "[empty]";
	}
	LEAF_NOTICE("Print preference value [%s] <= [%s]", key.c_str(), value.c_str());
	mqtt_publish("status/pref/"+key, value, 0);
      }
      })
      ELSEWHEN("cmd/load",{
	  load();
	})
      ELSEWHEN("cmd/save",{
	  save((payload=="force"));
	}
	);
    LEAF_LEAVE;
    return handled;
  };

};

#endif
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
