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
  PubsubNullLeaf(String name, String target) :
    AbstractPubsubLeaf(name, target)
  {
  }

  virtual void setup() {
	  AbstractPubsubLeaf::setup();
	  LEAF_NOTICE("NULL PUBSUB - local comms only");
	  pubsub_connected=true;
  }
  virtual bool pubsubConnect() { 
	  AbstractPubsubLeaf::pubsubConnect();
	  pubsubOnConnect();
	  return true;
  }
  virtual bool pubsubDisconnectConnect() { 
	  AbstractPubsubLeaf::pubsubDisconnect();
	  pubsubOnDisconnect();
	  return true;
  }
  
  virtual void _mqtt_subscribe(String topic, int qos=0, codepoint_t where=undisclosed_location)
  {
	  LEAF_NOTICE_AT(CODEPOINT(where), "MQTT SUB %s", topic.c_str());
  }
    
  virtual void _mqtt_unsubscribe(String topic) {
	  LEAF_NOTICE("MQTT UNSUB %s", topic.c_str());
  }

  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false){
	  idle_state(TRANSACTION, HERE);
	  LEAF_NOTICE("(NULL) PUB %s => [%s]", topic.c_str(), payload.c_str());

	  if (pubsub_loopback) {
		  LEAF_INFO("LOOPBACK PUB %s => %s", topic.c_str(), payload.c_str());
		  pubsub_loopback_buffer += topic + ' ' + payload + '\n';
	  }
	  delay(20);
	  idle_state(ONLINE,HERE);
	  return 0;
  }
    
};

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End: