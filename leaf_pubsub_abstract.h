//
//@**************************** class AbstractPubsub ******************************
// 
// This class encapsulates a publish-subscribe mechanism such as ESP32's EspAsyncMQTT,
// the MQTT AT-command interface provided by a cellular modem, or LoRAWAN.
//

#pragma once

class AbstractPubsubLeaf : public Leaf
{
public:
    
 
  AbstractPubsubLeaf(String name, pinmask_t pins) : Leaf("pubsub", name, pins) {
    LEAF_ENTER(L_INFO);

    LEAF_LEAVE;
  }

  virtual void setup() {}
  
  virtual void loop(void) {
    Leaf::loop();
    //LEAF_ENTER(L_INFO);
    
    //LEAF_LEAVE;
  }
  virtual uint16_t _mqtt_publish(String topic, String payload, int qos, bool retain);
  virtual void _mqtt_subscribe(String topic);
  virtual void _mqtt_unsubscribe(String topic);
};

