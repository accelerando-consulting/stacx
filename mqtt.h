//
//@******************************* constants *********************************

String deviceTopic="";


//@******************************* variables *********************************
//
// Network resources
//
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
bool mqttConfigured = false;
bool mqttConnected = false;
uint16_t sleep_pub_id = 0;
int sleep_duration_ms = 0;
SimpleMap<String,int> *mqttSubscriptions = NULL;

//
//@******************************* functions *********************************
uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false);

//
// Initiate connection to MQTT server
//
void _mqtt_connect() {
  ENTER(L_INFO);

  if (wifiConnected && mqttConfigured) {
    NOTICE("Connecting to MQTT...");
    mqttClient.connect();
    ALERT("MQTT Connection initiated");
  }
  else {
    DEBUG("MQTT not configured yet.  Retry in %d sec.", MQTT_RECONNECT_SECONDS);
    mqttReconnectTimer.once(MQTT_RECONNECT_SECONDS, _mqtt_connect);
  }

  LEAVE;
}

void _mqtt_connect_callback(bool sessionPresent) {
  ENTER(L_NOTICE);

  ALERT("Connected to MQTT.  sessionPresent=%s", TRUTH(sessionPresent));


  // Once connected, publish an announcement...
  mqttConnected = true;
  _mqtt_publish(deviceTopic, "online", 0, true);
  if (wake_reason) {
    _mqtt_publish(deviceTopic+"/status/wake", wake_reason, 0, true);
    wake_reason = NULL;
  }
  _mqtt_publish(deviceTopic+"/status/ip", ip_addr_str, 0, true);
  for (int i=0; leaves[i]; i++) {
    leaves[i]->mqtt_connect();
  }

  // ... and resubscribe
  _mqtt_subscribe(deviceTopic+"/cmd/restart");
  _mqtt_subscribe(deviceTopic+"/cmd/setup");
#ifdef _OTA_OPS_H
  _mqtt_subscribe(deviceTopic+"/cmd/update");
  _mqtt_subscribe(deviceTopic+"/cmd/rollback");
  _mqtt_subscribe(deviceTopic+"/cmd/bootpartition");
  _mqtt_subscribe(deviceTopic+"/cmd/nextpartition");
#endif
  _mqtt_subscribe(deviceTopic+"/cmd/ping");
  _mqtt_subscribe(deviceTopic+"/cmd/leaves");
  _mqtt_subscribe(deviceTopic+"/cmd/format");
  _mqtt_subscribe(deviceTopic+"/cmd/status");
  _mqtt_subscribe(deviceTopic+"/cmd/subscriptions");
  _mqtt_subscribe(deviceTopic+"/set/name");
  _mqtt_subscribe(deviceTopic+"/set/debug");
  _mqtt_subscribe(deviceTopic+"/set/debug_wait");
  _mqtt_subscribe(deviceTopic+"/set/debug_lines");
  _mqtt_subscribe(deviceTopic+"/set/debug_flush");


  INFO("Set up leaf subscriptions");
  _mqtt_subscribe("devices/*/+/#");
  _mqtt_subscribe("devices/+/*/#");
  for (int i=0; leaves[i]; i++) {
      Leaf *leaf = leaves[i];
      INFO("Initiate subscriptions for %s", leaf->get_name().c_str());
      leaf->mqtt_subscribe();
  }

  INFO("MQTT Connection setup complete");

  blink_rate = 2000;
  blink_duty = 80;

  LEAVE;
}

void _mqtt_disconnect_callback(AsyncMqttClientDisconnectReason reason) {
  ENTER(L_INFO);
  ALERT("Disconnected from MQTT (%d)", (int)reason);

  mqttConnected = false;
  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(MQTT_RECONNECT_SECONDS, _mqtt_connect);
  }

  for (int i=0; leaves[i]; i++) {
    leaves[i]->mqtt_disconnect();
  }
  LEAVE;
}

uint16_t _mqtt_publish(String topic, String payload, int qos, bool retain)
{
  uint16_t packetId = 0;
  ENTER(L_DEBUG);
  INFO("PUB %s => [%s]", topic.c_str(), payload.c_str());

  if (mqttConnected) {
    packetId = mqttClient.publish(topic.c_str(), qos, retain, payload.c_str());
    DEBUG("Publish initiated, ID=%d", packetId);
  }
  else {
    DEBUG("Publish skipped while MQTT connection is down: %s=>%s", topic.c_str(), payload.c_str());
  }
  LEAVE;
  return packetId;
}

void _mqtt_publish_callback(uint16_t packetId) {
  DEBUG("Publish acknowledged %d", (int)packetId);
  if (packetId == sleep_pub_id) {
    NOTICE("Going to sleep for %d ms", sleep_duration_ms);
#ifdef ESP8266
    ESP.deepSleep(1000*sleep_duration_ms, WAKE_RF_DEFAULT);
#else
    esp_sleep_enable_timer_wakeup(sleep_duration_ms * 1000);
    Serial.flush();
    esp_deep_sleep_start();
#endif
  }
}

void _mqtt_subscribe(String topic)
{
  ENTER(L_INFO);
  NOTICE("MQTT SUB %s", topic.c_str());
  if (mqttConnected) {
    uint16_t packetIdSub = mqttClient.subscribe(topic.c_str(), 0);
    if (packetIdSub == 0) {
      INFO("Subscription FAILED for topic=%s", topic.c_str());
    }
    else {
      INFO("Subscription initiated id=%d topic=%s", (int)packetIdSub, topic.c_str());
    }

#if 0
    if (mqttSubscriptions) {
      mqttSubscriptions->put(topic, 0);
    }
#endif
  }
  else {
    ALERT("Warning: Subscription attempted while MQTT connection is down (%s)", topic.c_str());
  }
  LEAVE;
}

void _mqtt_unsubscribe(String topic)
{
  ENTER(L_DEBUG);
  NOTICE("MQTT UNSUB %s", topic.c_str());
  if (mqttConnected) {
    uint16_t packetIdSub = mqttClient.unsubscribe(topic.c_str());
    DEBUG("UNSUBSCRIPTION initiated id=%d topic=%s", (int)packetIdSub, topic.c_str());
    if (mqttSubscriptions) {
      mqttSubscriptions->remove(topic);
    }
  }
  else {
    ALERT("Warning: Unsubscription attempted while MQTT connection is down (%s)", topic.c_str());
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

  NOTICE("MQTT message from server %s <= [%s] (q%d%s)",
       topic, payload_buf, (int)properties.qos, properties.retain?" retain":"");
  String Topic(topic);
  String Payload(payload_buf);
  bool handled = false;

  while (1) {
    int pos, lastPos;

    if (Topic.startsWith(_ROOT_TOPIC+"devices/")) {
      lastPos = _ROOT_TOPIC.length()+strlen("devices/");
    }
    else {
      // the topic does not begin with "devices/"
      DEBUG("Cannot find device header in topic %s", topic);
      // it might be an external topic subscribed to by a leaf
      break;
    }

    pos = Topic.indexOf('/', lastPos);
    if (pos < 0) {
      ALERT("Cannot find device type in topic %s", topic);
      break;
    }
    String device_type = Topic.substring(lastPos, pos);
    NOTICE("Parsed device type [%s] from topic at %d:%d", device_type.c_str(), lastPos, pos);

    lastPos = pos+1;
    pos = Topic.indexOf('/', lastPos);
    if (pos < 0) {
      ALERT("Cannot find device name in topic %s", topic);
      break;
    }

    String device_name = Topic.substring(lastPos, pos);
    NOTICE("Parsed device name [%s] from topic at %d:%d", device_name.c_str(), lastPos, pos);

    String device_topic = Topic.substring(pos+1);
    NOTICE("Parsed device topic [%s] from topic", device_topic.c_str());

    if ( ((device_type=="*") || (device_type == "backplane")) &&
	 ((device_name == "*") || (device_name == device_id))
      )
      {
      if (device_topic == "cmd/restart") {
#ifdef ESP8266
	ESP.reset();
#else
	ESP.restart();
#endif
      }
      else if (device_topic == "cmd/setup") {
	ALERT("Opening WIFI setup portal");
	_wifiMgr_setup(true);
	ALERT("WIFI setup portal done");
      }
#ifdef _OTA_OPS_H
      else if (device_topic == "cmd/update") {
	ALERT("Doing HTTP OTA update from %s", Payload.c_str());
	pull_update(Payload);  // reboots if success
	ALERT("HTTP OTA update failed");
      }
      else if (device_topic == "cmd/rollback") {
	ALERT("Doing HTTP OTA update from %s", Payload.c_str());
	rollback_update(Payload);  // reboots if success
	ALERT("HTTP OTA rollback failed");
      }
      else if (device_topic == "cmd/bootpartition") {
	const esp_partition_t *part = esp_ota_get_boot_partition();
	/*
	StaticJsonDocument<255> doc;
	JsonObject root = doc.to<JsonObject>();
	*/
	_mqtt_publish(deviceTopic+"/status/bootpartition", part->label);
      }
      else if (device_topic == "cmd/nextpartition") {
	const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
	_mqtt_publish(deviceTopic+"/status/nextpartition", part->label);
      }
#endif
      else if (device_topic == "cmd/ping") {
	INFO("RCVD PING %s", Payload.c_str());
	_mqtt_publish(deviceTopic+"/status/ack", Payload);
      }
      else if (device_topic == "cmd/status") {
	INFO("RCVD STATUS %s", Payload.c_str());
	_mqtt_publish(deviceTopic+"/status/ip", ip_addr_str);
      }
      else if (device_topic == "cmd/subscriptions") {
	INFO("RCVD SUBSCRIPTIONS %s", Payload.c_str());
	String subs = "[\n    ";
	for (int s = 0; s < mqttSubscriptions->size(); s++) {
	  if (s) {
	    subs += ",\n    ";
	  }
	  subs += '"';
	  subs += mqttSubscriptions->getKey(s);
	  subs += '"';
	  DEBUG("Subscriptions [%s]", subs.c_str());
	}
	subs += "\n]";
	_mqtt_publish(deviceTopic+"/status/subscriptions", subs);
      }
      else if (device_topic == "cmd/format") {
	_writeConfig(true);
      }
      else if (device_topic == "cmd/leaves") {
	INFO("Leaf inventory");
	String inv = "[\n    ";
	for (int i=0; leaves[i]; i++) {
	  if (i) {
	    inv += ",\n    ";
	  }
	  inv += '"';
	  inv += leaves[i]->describe();
	  inv += '"';
	  DEBUG("Leaf inventory [%s]", inv.c_str());
	}
	inv += "\n]";
	_mqtt_publish(deviceTopic+"/status/leaves", inv);

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
#ifdef ESP8266
	ESP.reset();
#else
	ESP.restart();
#endif
      }
      else if (device_topic == "set/debug") {
	NOTICE("Set DEBUG");
	if (Payload == "more") {
	  debug++;
	}
	else if (Payload == "less" && (debug > 0)) {
	  debug--;
	}
	else {
	  debug = Payload.toInt();
	}
	_mqtt_publish(deviceTopic+"/status/debug", String(debug, DEC));
      }
      else if (device_topic == "set/debug_wait") {
	NOTICE("Set DBGWAIT");
	DBGWAIT = Payload.toInt();
	_mqtt_publish(deviceTopic+"/status/debug_wait", String(debug, DEC));
      }
      else if (device_topic == "set/debug_lines") {
	NOTICE("Set debug_lines");
	bool lines = false;
	if (payload == "on") lines=true;
	else if (payload == "true") lines=true;
	else if (payload == "lines") lines=true;
	else if (payload == "1") lines=true;
	debug_lines = lines;
	_mqtt_publish(deviceTopic+"/status/debug_lines", TRUTH(debug_lines));
      }
      else if (device_topic == "set/debug_flush") {
	NOTICE("Set debug_flush");
	bool flush = false;
	if (payload == "on") flush=true;
	else if (payload == "true") flush=true;
	else if (payload == "flush") flush=true;
	else if (payload == "1") flush=true;
	debug_flush = flush;
	_mqtt_publish(deviceTopic+"/status/debug_flush", TRUTH(debug_flush));
      }
      else {
	ALERT("No handler for backplane %s topic [%s]", device_id, topic);
      }
    }

    for (int i=0; leaves[i]; i++) {
      Leaf *leaf = leaves[i];
      if (leaf->wants_topic(device_type, device_name, device_topic)) {
	handled |= leaf->mqtt_receive(device_type, device_name, device_topic, Payload);
      }
    }
    if (!handled) {
      DEBUG("No leaf handled topic [%s]", topic);
    }

    break;
  }

  if (!handled) {
    // Leaves can also subscribe to raw topics, so try that
    for (int i=0; leaves[i]; i++) {
      Leaf *leaf = leaves[i];
      if (leaf->wants_raw_topic(Topic)) {
	handled |= leaf->mqtt_receive_raw(Topic, Payload);
      }
    }
  }

  LEAVE;
}

void mqtt_setup()
{
  ENTER(L_INFO);

  //
  // Set up the MQTT Client
  //
  deviceTopic = _ROOT_TOPIC + "devices/backplane/"+device_id;
  uint16_t portno = atoi(mqtt_port);

  ALERT("MQTT Setup [%s:%hu] %s", mqtt_server, portno, deviceTopic.c_str());
  mqttClient.setServer(mqtt_server, portno);
  mqttClient.setClientId(deviceTopic.c_str());
  mqttClient.onConnect(_mqtt_connect_callback);
  mqttClient.onDisconnect(_mqtt_disconnect_callback);
  mqttClient.onSubscribe(_mqtt_subscribe_callback);
  mqttClient.onUnsubscribe(_mqtt_unsubscribe_callback);
  mqttClient.onPublish(_mqtt_publish_callback);
  mqttClient.onMessage(_mqtt_receive_callback);
  mqttClient.setWill(deviceTopic.c_str(), 0, true, "offline");

  mqttSubscriptions = new SimpleMap<String,int>(_compareStringKeys);

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

void initiate_sleep_ms(int ms)
{
  ENTER(L_NOTICE);
  sleep_duration_ms = ms;
  sleep_pub_id = _mqtt_publish(deviceTopic, String("sleep"), 1, true);
  LEAVE;
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
