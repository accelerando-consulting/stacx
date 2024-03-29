#pragma once
#include "abstract_pubsub.h"
//
//@********************** class PubsubNullLeaf ***********************
//
// This class encapsulates a pseudo-pubsub connection for devices that do not
// communicate except locally.
//

class PubsubNullLeaf : public AbstractPubsubLeaf
{
public:
  PubsubNullLeaf(String name="nullmqtt", String target="")
    : AbstractPubsubLeaf(name, target)
    , Debuggable(name)
  {
    pubsub_use_device_topic = false;
  }

  virtual void setup() {
    AbstractPubsubLeaf::setup();
    LEAF_NOTICE("NULL PUBSUB - local comms only");
  }
  virtual void loop()
  {
    last_broker_heartbeat = millis();
    AbstractPubsubLeaf::loop();
  }
  virtual bool pubsubConnect() {
    AbstractPubsubLeaf::pubsubConnect();
    pubsubOnConnect();
    return true;
  }
  virtual bool pubsubDisconnect() {
    AbstractPubsubLeaf::pubsubDisconnect(true);
    pubsubOnDisconnect();
    return true;
  }

  virtual void _mqtt_subscribe(String topic, int qos=0, codepoint_t where=undisclosed_location)
  {
    LEAF_NOTICE_AT(CODEPOINT(where), "MQTT SUB %s", topic.c_str());
  }

  virtual void _mqtt_unsubscribe(String topic, int level=L_NOTICE) {
    __LEAF_DEBUG__(level, "MQTT UNSUB %s", topic.c_str());
  }

  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false){
    //ipLeaf->ipCommsState(TRANSACTION, HERE);
    LEAF_INFO("(NULL) PUB %s => [%s]", topic.c_str(), payload.c_str());

    if (pubsub_loopback) {
      sendLoopback(topic, payload);
      return 0;
    }

    //delay(20);
    //ipLeaf->ipCommsState(ONLINE,HERE);
    return 0;
  }

};

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
