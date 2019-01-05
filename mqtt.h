//
//@******************************* constants *********************************

const String _BASE_TOPIC = "devices/backplane/";
String deviceTopic;


//@******************************* variables *********************************

// 
// Network resources
//
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
bool mqttConfigured = false;
bool mqttConnected = false;

//
//@******************************* functions *********************************

// 
// Initiate connection to MQTT server
// 
void _mqtt_connect() {
  ENTER(L_INFO);

  if (mqttConfigured) {
    NOTICE("Connecting to MQTT...");
    mqttClient.connect();
    INFO("MQTT Connection initiated");
  }
  else {
    NOTICE("MQTT not configured yet.  Waiting...");
    mqttReconnectTimer.once(MQTT_RECONNECT_SECONDS, _mqtt_connect);
  }
  
  LEAVE;
}

void _mqtt_connect_callback(bool sessionPresent) {
  ENTER(L_INFO);
  NOTICE("Connected to MQTT.  sessionPresent=%s", TRUTH(sessionPresent));


  // Once connected, publish an announcement...
  mqttConnected = true;
  _mqtt_publish(deviceTopic, "online", 0, true);
  for (int i=0; pods[i]; i++) {
    pods[i]->mqtt_connect();
  }

  // ... and resubscribe
  _mqtt_subscribe(deviceTopic+"/cmd/restart");
  _mqtt_subscribe(deviceTopic+"/cmd/ping");
  _mqtt_subscribe(deviceTopic+"/cmd/pods");
  _mqtt_subscribe(deviceTopic+"/cmd/format");
  _mqtt_subscribe(deviceTopic+"/cmd/status");
  _mqtt_subscribe(deviceTopic+"/set/name");
  _mqtt_subscribe(deviceTopic+"/set/debug");


  INFO("Set up pod subscriptions");
  _mqtt_subscribe("devices/*/+/#");
  _mqtt_subscribe("devices/+/*/#");
  for (int i=0; pods[i]; i++) {
      Pod *pod = pods[i];
      pod->mqtt_subscribe();
  }

  INFO("MQTT Connection setup complete");

  blink_rate = 2000;
  blink_duty = 5;
  
  LEAVE;
}

void _mqtt_disconnect_callback(AsyncMqttClientDisconnectReason reason) {
  ENTER(L_INFO);
  ALERT("Disconnected from MQTT (%d)", (int)reason);

  mqttConnected = false;
  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(MQTT_RECONNECT_SECONDS, _mqtt_connect);
  }

  for (int i=0; pods[i]; i++) {
    pods[i]->mqtt_disconnect();
  }
  LEAVE;
}


void _mqtt_publish(String topic, String payload, int qos, bool retain) 
{
  ENTER(L_DEBUG);
  INFO("PUB %s => [%s]", topic.c_str(), payload.c_str());
  if (mqttConnected) {
    uint16_t packetId = mqttClient.publish(topic.c_str(), qos, retain, payload.c_str());
    DEBUG("Publish initiated, ID=%d", packetId);
  }
  else {
    ALERT("Warning: Publish attempt while MQTT connection is down: %s=>%s", topic.c_str(), payload.c_str());
  }
  LEAVE;
}

void _mqtt_publish_callback(uint16_t packetId) {
  DEBUG("Publish acknowledged %d", (int)packetId);
}

void _mqtt_subscribe(String topic) 
{
  ENTER(L_DEBUG);
  INFO("SUB %s", topic.c_str());
  if (mqttConnected) {
    uint16_t packetIdSub = mqttClient.subscribe(topic.c_str(), 0);
    DEBUG("Subscription initiated id=%d topic=%s", (int)packetIdSub, topic.c_str());
  }
  else {
    ALERT("Warning: Subscription attempted while MQTT connection is down (%s)", topic.c_str());
  }
  LEAVE;
}

void _mqtt_subscribe_callback(uint16_t packetId, uint8_t qos) {
  DEBUG("Subscribe acknowledged %d QOS %d", (int)packetId, (int)qos);
}

void _mqtt_unsubscribe_callback(uint16_t packetId) {
  DEBUG("Unsubscribe acknowledged %d", (int)packetId);
}

void _mqtt_receive_callback(char* topic,
		   char* payload,
		   AsyncMqttClientMessageProperties properties,
		   size_t len,
		   size_t index,
		   size_t total) {
  ENTER(L_DEBUG);
  
  // handle message arrived
  char payload_buf[256];
  if (len > sizeof(payload_buf)-1) len=sizeof(payload_buf)-1;
  memcpy(payload_buf, payload, len);
  payload_buf[len]='\0';
  (void)index;
  (void)total;
 
  INFO("MQTT message from server %s <= [%s] (q%d%s)",
       topic, payload_buf, (int)properties.qos, properties.retain?" retain":"");
  String Topic(topic);
  String Payload(payload_buf);

  while (1) {
    int pos, lastPos;

    if (!Topic.startsWith("devices/")) {
      // the topic does not begin with "devices/"
      ALERT("Cannot find device header in topic %s", topic);
      break;
    }
    lastPos = strlen("devices/");

    pos = Topic.indexOf('/', lastPos);
    if (pos < 0) {
      ALERT("Cannot find device type in topic %s", topic);
      break;
    }
    String device_type = Topic.substring(lastPos, pos);
    //DEBUG("Parsed device type [%s] from topic at %d:%d", device_type.c_str(), lastPos, pos);

    lastPos = pos+1;
    pos = Topic.indexOf('/', lastPos);
    if (pos < 0) {
      ALERT("Cannot find device name in topic %s", topic);
      break;
    }
    
    String device_name = Topic.substring(lastPos, pos);
    //DEBUG("Parsed device name [%s] from topic at %d:%d", device_name.c_str(), lastPos, pos);

    String device_topic = Topic.substring(pos+1);
    //DEBUG("Parsed device topic [%s] from topic", device_topic.c_str());

    if ( ((device_type=="*") || (device_type == "backplane")) &&
	 ((device_name == "*") || (device_name == device_id))
      ) {
      if (device_topic == "cmd/restart") {
	ESP.reset();
      }
      else if (device_topic == "cmd/ping") {
	INFO("RCVD PING %s", Payload.c_str());
	_mqtt_publish(deviceTopic+"/status/ack", Payload);
      }
      else if (device_topic == "cmd/status") {
	INFO("RCVD PING %s", Payload.c_str());
	_mqtt_publish(deviceTopic+"/status/ip", ip_addr_str);
      }
      else if (device_topic == "cmd/format") {
	_writeConfig(true);
      }
      else if (device_topic == "cmd/pods") {
	INFO("Pod inventory");
	String inv = "[\n    ";
	for (int i=0; pods[i]; i++) {
	  if (i) {
	    inv += ",\n    ";
	  }
	  inv += '"';
	  inv += pods[i]->describe();
	  inv += '"';
	  DEBUG("Pod inventory [%s]", inv.c_str());
	}
	inv += "\n]";
	_mqtt_publish(deviceTopic+"/status/pods", inv);

      }
      else if (device_topic == "set/name") {
	NOTICE("Updating device ID [%s]", payload);
	_readConfig();
	if (Payload.length() > 0) {
	  strlcpy(device_id, Payload.c_str(), sizeof(device_id));
	}
	_writeConfig();
	ALERT("Rebooting for new config");
	delay(2000);
	ESP.reset();
      }
      else if (device_topic == "set/debug") {
	debug = Payload.toInt();
	_mqtt_publish(deviceTopic+"/status/debug", String(debug, DEC));
      }
      else {
	ALERT("No handler for backplane topic [%s]", topic);
      }
    }

    bool handled = false;
    for (int i=0; pods[i]; i++) {
      Pod *pod = pods[i];
      if (pod->wants_topic(device_type, device_name, device_topic)) {
	handled |= pod->mqtt_receive(device_type, device_name, device_topic, Payload);
      }
    }
    if (!handled) {
      ALERT("No handler for pod topic [%s]", topic);
    }

    break;
  }
  LEAVE;
}

void mqtt_setup() 
{
  ENTER(L_INFO);
  
  // 
  // Set up the MQTT Client
  //
  deviceTopic = _BASE_TOPIC+device_id;
  uint16_t portno = atoi(mqtt_port);
  
  ALERT("MQTT Setup [%s:%hu] %s", mqtt_server, portno, deviceTopic.c_str());
  mqttClient.setServer(mqtt_server, portno);
  mqttClient.onConnect(_mqtt_connect_callback);
  mqttClient.onDisconnect(_mqtt_disconnect_callback);
  mqttClient.onSubscribe(_mqtt_subscribe_callback);
  mqttClient.onUnsubscribe(_mqtt_unsubscribe_callback);
  mqttClient.onPublish(_mqtt_publish_callback);
  mqttClient.onMessage(_mqtt_receive_callback);
  mqttClient.setWill(deviceTopic.c_str(), 0, true, "offline");

  mqttConfigured = true;
  LEAVE;
}

void mqtt_loop() 
{
  //ENTER(L_DEBUG);
  
  static unsigned long lastHeartbeat = 0;
  unsigned long now = millis();
  // 
  // Handle MQTT Events
  // 
  if (mqttConnected) {
    // 
    // MQTT is active, process any pending events
    //
    if (now > (lastHeartbeat + HEARTBEAT_INTERVAL_SECONDS*1000)) {
      lastHeartbeat = now;
      _mqtt_publish(deviceTopic+"/status/heartbeat", String(now/1000, DEC));
    }
  }
  //LEAVE;
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
