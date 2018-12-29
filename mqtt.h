//
//@******************************* constants *********************************

const String _BASE_TOPIC = "devices/backplane/";

// How often do we wait between MQTT reconnection attemps
#define MQTT_CONNECT_RETRY 20000


//@******************************* variables *********************************

// 
// Network resources
//
PubSubClient mqttClient(espClient);
unsigned long lastMqttConnectAttempt = 0;
unsigned long lastHeartbeat = 0;

//
//@******************************* functions *********************************

// 
// Initiate connection to MQTT server
// 
bool _mqtt_connect() {
  INFO("Connecting to MQTT");
  String mytopic = _BASE_TOPIC+device_id;

  if (mqttClient.connect("arduinoClient", mytopic.c_str(), 0, true, "offline")) {
    INFO("Connected to MQTT");
    
    // Once connected, publish an announcement...
    _mqtt_publish(mytopic, String("online"), 0, true);
    for (int i=0; pods[i]; i++) {
      pods[i]->mqtt_connect();
    }

    // ... and resubscribe
    for (int i=0; pods[i]; i++) {
      pods[i]->mqtt_subscribe();
    }
  }
  else{
    ALERT("MQTT connection failed: %d", mqttClient.state());
  }
  return mqttClient.connected();
}

bool _mqtt_publish(String topic, String payload, int qos, bool retain) 
{
  INFO("PUB %s => [%s]", topic.c_str(), payload.c_str());
  if (mqttClient.connected()) {
    mqttClient.publish(topic.c_str(), (const unsigned char *)payload.c_str(), qos, retain);
  }
  else {
    ALERT("Warning: Publish attempt while MQTT connection is down: %s=>%s", topic.c_str(), payload.c_str());
  }
}

bool _mqtt_subscribe(String topic) 
{
  INFO("SUB %s", topic.c_str());
  if (mqttClient.connected()) {
    mqttClient.subscribe(topic.c_str());
  }
  else {
    ALERT("Warning: Subscription attempted while MQTT connection is down");
  }
}

void _mqtt_receive(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  String Topic(topic);
  payload[length]='\0';
  String Payload((char*)payload);
  INFO("MQTT message from server %s <= [%s]", Topic.c_str(), Payload.c_str());

  bool handled = false;

  while (1) {
    int pos=0, lastPos = 0;

    pos = Topic.indexOf('/', lastPos);
    if (pos < 0) {
      ALERT("Cannot find device type in topic %s", topic);
      break;
    }
    String device_type = Topic.substring(lastPos, pos-lastPos+1);

    pos = Topic.indexOf('/', lastPos = pos);
    if (pos < 0) {
      ALERT("Cannot find device name in topic %s", topic);
      break;
    }
    
    String device_name = Topic.substring(lastPos, pos-lastPos+1);
    String device_topic = Topic.substring(pos);

    for (int i=0; pods[i]; i++) {
      handled |= pods[i]->mqtt_receive(device_type, device_name, device_topic, Payload);
    }
    break;
  }

  if (!handled) {
    ALERT("No handler for topic [%s]", topic);
  }
}

void mqtt_setup() 
{
  // 
  // Set up the MQTT Client
  //
  ALERT("MQTT Setup [%s:%s]", mqtt_server, mqtt_port);
  mqttClient.setServer(mqtt_server, atoi(mqtt_port));
  mqttClient.setCallback(_mqtt_receive);
}

void mqtt_loop() 
{
  // 
  // Handle MQTT Events
  // 
  if (mqttClient.connected()) {
    // 
    // MQTT is active, process any pending events
    // 
    mqttClient.loop();
  }
  else {
    // 
    // MQTT is not active.
    // 
    // It is time to re-try connecting to MQTT
    //
    unsigned long now = millis();
    if ( wifiConnected &&
	 ( (lastMqttConnectAttempt == 0) ||
	   ((now - lastMqttConnectAttempt) > MQTT_CONNECT_RETRY))
      ) {
      lastMqttConnectAttempt = now;
      _mqtt_connect();
    }
  }
}


// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
