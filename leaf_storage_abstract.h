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
  StorageLeaf(String name, String keys) : Leaf("storage", name, 0) {
    values = new SimpleMap<String,String>(_compareStringKeys);
    int pos ;
    String key;

    do {
      if ((pos = keys.indexOf(',')) > 0) {
	key = keys.substring(0, pos);
	keys.remove(0,pos+1);
      }
      else {
	key = keys;
	keys="";
      }

      values->put(key, "");

    } while (keys.length() > 0);

  }
  
  void setup(void) {
    Leaf::setup();
  }

  virtual void mqtt_subscribe() {
    LEAF_ENTER(L_NOTICE);
    Leaf::mqtt_subscribe();

    for (int i=0; i<values->size(); i++) {
      _mqtt_subscribe(base_topic+"/set/"+values->getKey(i));
      _mqtt_subscribe(base_topic+"/status/"+values->getKey(i));
    }
    
    LEAF_LEAVE;
  }

  void status_pub()
  {
    for (int i=0; i<values->size(); i++) {
      _mqtt_publish("status/"+values->getKey(i), values->getData(i));
    }
  }

  virtual bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    LEAF_NOTICE("%s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    for (int i=0; i<values->size(); i++) {
      String key = values->getKey(i);

      if ((topic == "set/"+key) || (topic == "status/"+key)) {
	handled = true;
	String old_value = values->getData(i);
	if (payload != old_value) {
	  values->put(key, payload);
	  if (topic == "set/"+key) {
	    // acknowledge the set
	    mqtt_publish((String)("status/"+key), payload, false);
	  }
	  else {
	    // publish the value to other leaves (but not back to MQTT)
	    publish((String)("status/"+key), payload);
	  }
	  
	}
      }
    }

    RETURN(handled);
  };

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
