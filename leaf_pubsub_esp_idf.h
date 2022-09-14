#pragma once
#include "abstract_pubsub.h"
#include "abstract_ip.h"

//
//@********************** class PubsubESPIDFLeaf ***********************
//
// This class encapsulates an Mqtt connection that utilises an Espressif's
// MQTT stack
//
//

class PubsubMQTTEspIdfLeaf : public AbstractPubsubLeaf
{
public:
  PubsubMQTTEspIdfLeaf(String name, String target, bool use_ssl=true, bool use_device_topic=true, bool run = true) :
    AbstractPubsubLeaf(name, target, use_ssl, use_device_topic)
  {
    LEAF_ENTER(L_INFO);
    this->run = run;
    // further the setup happens in the superclass
    LEAF_LEAVE;
  }

  virtual void setup();
  virtual void loop(void);
  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false);
  virtual void _mqtt_subscribe(String topic, int qos=0, codepoint_t where=undisclosed_location);
  virtual void _mqtt_unsubscribe(String topic);
  virtual bool mqtt_receive(String type, String name, String topic, String payload);
  virtual bool pubsubConnect(void);
  virtual bool pubsubConnectStatus(void);
  virtual void pubsubDisconnect(bool deliberate=true);
  void pubsubOnConnect(bool do_subscribe);
  virtual void initiate_sleep_ms(int ms);
  virtual void pre_sleep(int duration=0);


protected:
  //
  // Network resources
  //
  AbstractIpSimcomLeaf *modem_leaf = NULL;


  bool install_cert();

  bool netStatus()
  {
    return modem_leaf?modem_leaf->netStatus():false;
  }

  bool connStatus()
  {
    return modem_leaf?modem_leaf->connStatus():false;
  }

};

void AbstractPubsubSimcomLeaf::setup()
{
  AbstractPubsubLeaf::setup();
  LEAF_ENTER(L_INFO);
  pubsub_connected = false;

  //
  // Set up the MQTT Client
  //
  modem_leaf = (AbstractIpSimcomLeaf *)ipLeaf;
  if (modem_leaf == NULL) {
    LEAF_ALERT("Modem leaf not found");
  }

  LEAF_VOID_RETURN;
}

bool AbstractPubsubSimcomLeaf::pubsubConnectStatus() 
{
  int i;
  if (!modem_leaf->modemSendExpectInt("AT+SMSTATE?","+SMSTATE: ", &i, -1, HERE)) 
  {
    LEAF_ALERT("Cannot get connected status");
    i = 0;
  }
  return (i!=0);
}


bool AbstractPubsubSimcomLeaf::mqtt_receive(String type, String name, String topic, String payload)
{
  LEAF_ENTER(L_DEBUG);
  bool handled = Leaf::mqtt_receive(type, name, topic, payload);
  LEAF_INFO("%s, %s", topic.c_str(), payload.c_str());

  WHEN("_ip_connect",{
    if (ipLeaf) {
      if (pubsub_autoconnect) {
	LEAF_NOTICE("IP is online, autoconnecting MQTT");
	pubsubConnect();
      }
    }
    })
    ELSEWHEN("_ip_disconnect",{
    if (pubsub_connected) {
      pubsubDisconnect();
    }
      })
    ELSEWHEN("cmd/pubsub_status",{
    char status[32];
    uint32_t secs;
    if (pubsub_connected) {
      secs = (millis() - pubsub_connect_time)/1000;
      snprintf(status, sizeof(status), "online %d:%02d", secs/60, secs%60);
    }
    else {
      secs = (millis() - pubsub_disconnect_time)/1000;
      snprintf(status, sizeof(status), "offline %d:%02d", secs/60, secs%60);
    }
    mqtt_publish("status/pubsub_status", status);
      })

  return handled;
}

void AbstractPubsubSimcomLeaf::loop()
{
  AbstractPubsubLeaf::loop();

  unsigned long now = millis();

  if (pubsub_reconnect_due) {
    LEAF_NOTICE("Attempting reconenct");
    pubsub_reconnect_due=false;
    pubsubConnect();
  }

  if (!pubsub_connect_notified && pubsub_connected) {
    LEAF_NOTICE("Notifying of pubsub connection");
    pubsubOnConnect(true);
    publish("_pubsub_connect",String(1));
    pubsub_connect_notified = pubsub_connected;
  }
  else if (pubsub_connect_notified && !pubsub_connected) {
    LEAF_NOTICE("Notifying of pubsub disconnection");
    publish("_pubsub_disconnect", String(1));
    pubsub_connect_notified = pubsub_connected;
  }

}

void AbstractPubsubSimcomLeaf::pre_sleep(int duration)
{
  pubsubDisconnect(true);
}

void AbstractPubsubSimcomLeaf::pubsubDisconnect(bool deliberate) {
  LEAF_ENTER(L_NOTICE);
  AbstractPubsubLeaf::pubsubDisconnect(deliberate);
  if (modem_leaf->modemSendCmd(25000, HERE, "AT+SMDISC")) {
      LEAF_NOTICE("Disconnect command sent");
      if (!pubsubConnectStatus()) {
	LEAF_NOTICE("State is now disconnected");
	pubsubOnDisconnect();
      }
      else {
	LEAF_ALERT("Disconnect failed");
      }
    }
  else {
    LEAF_ALERT("Disconnect command not accepted");
  }

  LEAF_LEAVE;
}

//
// Initiate connection to MQTT server
//
bool AbstractPubsubSimcomLeaf::pubsubConnect() {
  LEAF_ENTER(L_INFO);
  static char buf[2048];

  if (!modem_leaf) {
    LEAF_ALERT("Modem leaf not found");
    LEAF_RETURN(false);
  }

  if (!modem_leaf->isConnected()) {
    LEAF_ALERT("Not connected to cell network, wait till later");
    LEAF_RETURN(false);
  }

  if (!modem_leaf->modemWaitPortMutex(HERE)) {
    LEAF_ALERT("Could not acquire port mutex");
    LEAF_RETURN(false);
  }
  
  // If not already connected, connect to MQTT
  if (pubsubConnectStatus()) {
    LEAF_NOTICE("Already connected to MQTT broker.");
    pubsub_connected = true;
    modem_leaf->modemReleasePortMutex(HERE);
    pubsubOnConnect(false);
    LEAF_RETURN(true);
  }

  LEAF_NOTICE("Establishing connection to MQTT broker %s => %s:%d",
	      device_id, pubsub_host.c_str(), pubsub_port);
  pubsub_connected = false;
  idle_state(TRY_PUBSUB, HERE);

  modem_leaf->modemSetParameter("SMCONF", "CLEANSS", String(pubsub_use_clean_session?1:0), HERE);
  modem_leaf->modemSetParameterQuoted("SMCONF", "CLIENTID", String(device_id),HERE);
  if ((pubsub_port == 0) || (pubsub_port==1883)) {
    modem_leaf->modemSetParameterQuoted("SMCONF", "URL", pubsub_host,HERE);
  }
  else {
    modem_leaf->modemSetParameterQQ("SMCONF", "URL", pubsub_host, String(pubsub_port),HERE);
  }
  if (pubsub_user && pubsub_user.length() && (pubsub_user!="[none]")) {
    modem_leaf->modemSetParameter("SMCONF", "USERNAME", pubsub_user,HERE);
    modem_leaf->modemSetParameter("SMCONF", "PASSWORD", pubsub_pass,HERE);
  }
  
  if (pubsub_keepalive_sec > 0) {
    modem_leaf->modemSetParameter("SMCONF", "KEEPTIME", String(pubsub_keepalive_sec),HERE);
  }

  if (pubsub_use_status && pubsub_lwt_topic) {
    pubsub_lwt_topic = base_topic + "status/presence";
    modem_leaf->modemSetParameterQuoted("SMCONF", "TOPIC", pubsub_lwt_topic,HERE);
    modem_leaf->modemSetParameterQuoted("SMCONF", "MESSAGE", "offline",HERE);
    modem_leaf->modemSetParameter("SMCONF", "RETAIN", "1",HERE);
  }

  if (pubsub_use_ssl) {

    LEAF_INFO("Configuring MQTT for SSL...");
    modem_leaf->modemSetParameter("SSLCFG", "ctxindex", "0",HERE); // use index 1
    //modem_leaf->modemSetParameter("SSLCFG", "ignorertctime", "1",HERE);
    modem_leaf->modemSetParameterPair("SSLCFG", "sslversion","0","3",HERE);
    modem_leaf->modemSetParameterUQ("SSLCFG", "convert","2","cacert.pem",HERE);
    if (pubsub_use_ssl_client_cert) {
      modem_leaf->modemSetParameterUQQ("SSLCFG", "convert", "1", "client.crt", "client.key",HERE);
      modem_leaf->modemSendCmd(HERE, "AT+SMSSL=1,cacert.pem,client.crt");
    }
    else {
      modem_leaf->modemSendCmd(HERE, "AT+SMSSL=0,cacert.pem");
    }
    modem_leaf->modemSendExpectOk("AT+SMSSL?", HERE);
  }
  else {
    modem_leaf->modemSendCmd(HERE, "AT+SMSSL=0");
  }

  char cmdbuffer[80];
  char replybuffer[80];
#if 0
  modem->sendExpectStringReply("AT+CDNSCFG?","PrimaryDns: ", replybuffer, 30000, sizeof(replybuffer),2, HERE);
  //if (strcmp(replybuffer,"0.0.0.0")==0) {
  //LEAF_NOTICE("Modem does not have DNS.  Let's use teh googles");
  //modem->sendCheckReply("AT+CDNSCFG=8.8.8.8","OK");
  //}

  snprintf(cmdbuffer, sizeof(cmdbuffer), "AT+CDNSGIP=%s", pubsub_host.c_str());
  modem->sendExpectStringReply(cmdbuffer,"+CDNSGIP: ", replybuffer, 30000, sizeof(replybuffer),2, HERE);
#endif

  LEAF_INFO("Initiating MQTT connect");
  int retry = 1;
  const int max_retries = 1;
  int initialState;

  while (!pubsubConnectStatus() && (retry <= max_retries)) {

    if (modem_leaf->modemSendCmd(25000, HERE, "AT+SMCONN")) {
      LEAF_NOTICE("Connection succeeded");
      pubsubSetConnected();
      break;
    }
    
    LEAF_ALERT("ERROR: Failed to connect to broker.");
    post_error(POST_ERROR_PUBSUB, 3);
    ERROR("MQTT connect fail");
    ++retry;

  }

  modem_leaf->modemReleasePortMutex(HERE);
  if (pubsub_connected) {
    LEAF_NOTICE("Connected to broker.");
    pubsubOnConnect(true);
    pubsub_connect_notified = true;
  }
  LEAF_RETURN(pubsub_connected);
}

void AbstractPubsubSimcomLeaf::pubsubOnConnect(bool do_subscribe)
{
  LEAF_ENTER(L_INFO);
  LEAF_INFO("Connected to MQTT");
  AbstractPubsubLeaf::pubsubOnConnect(do_subscribe);

  // Once connected, publish an announcement...
  mqtt_publish("status/presence", "online", 0, true);
  if ((pubsub_connect_count == 1) && wake_reason) {
    mqtt_publish("status/wake", wake_reason, 0, true);
  }
  mqtt_publish("status/ip", ip_addr_str, 0, true);
  for (int i=0; leaves[i]; i++) {
    leaves[i]->mqtt_connect();
  }

  if (do_subscribe) {
    // we skip this if the modem told us "already connected, dude", which
    // can happen after sleep.

    //_mqtt_subscribe("ping", 0, HERE);
    if (pubsub_use_wildcard_topic) {
      _mqtt_subscribe(base_topic+"cmd/#", 0, HERE);
      _mqtt_subscribe(base_topic+"get/#", 0, HERE);
      _mqtt_subscribe(base_topic+"set/#", 0, HERE);
      if (!pubsub_use_flat_topic) {
	_mqtt_subscribe(base_topic+"set/pref/+", 0, HERE);
      }
      if (hasPriority()) {
	_mqtt_subscribe(base_topic+"+/cmd/#", 0, HERE);
      }
    }

    mqtt_subscribe("cmd/restart", HERE);
    mqtt_subscribe("cmd/setup", HERE);
#ifdef _OTA_OPS_H
    mqtt_subscribe("cmd/update", HERE);
    mqtt_subscribe("cmd/rollback", HERE);
    mqtt_subscribe("cmd/bootpartition", HERE);
    mqtt_subscribe("cmd/nextpartition", HERE);
#endif
    mqtt_subscribe("cmd/ping", HERE);
    mqtt_subscribe("cmd/leaves", HERE);
    mqtt_subscribe("cmd/format", HERE);
    mqtt_subscribe("cmd/status", HERE);
    mqtt_subscribe("cmd/subscriptions", HERE);
    mqtt_subscribe("set/name", HERE);
    mqtt_subscribe("set/debug", HERE);
    mqtt_subscribe("set/debug_wait", HERE);
    mqtt_subscribe("set/debug_lines", HERE);
    mqtt_subscribe("set/debug_flush", HERE);

    LEAF_INFO("Set up leaf subscriptions");

    for (int i=0; leaves[i]; i++) {
      Leaf *leaf = leaves[i];
      LEAF_INFO("Initiate subscriptions for %s", leaf->get_name().c_str());
      leaf->mqtt_do_subscribe();
    }
  }
  LEAF_INFO("MQTT Connection setup complete");

  publish("_pubsub_connect", pubsub_host);
  idle_state(ONLINE, HERE);
  last_external_input = millis();

  LEAF_LEAVE;
}


uint16_t AbstractPubsubSimcomLeaf::_mqtt_publish(String topic, String payload, int qos, bool retain)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_INFO("PUB %s => [%s]", topic.c_str(), payload.c_str());
  const char *t = topic.c_str();
  const char *p = payload.c_str();
  int i;

  if (pubsub_loopback) {
    LEAF_INFO("LOOPBACK PUB %s => %s", t, p);
    pubsub_loopback_buffer += topic + ' ' + payload + '\n';
    return 0;
  }

  if (pubsub_connected) {
    if (!pubsubConnectStatus()) {
      LEAF_ALERT("Lost MQTT connection");
      if (ipLeaf->isConnected()) {
	LEAF_ALERT("Try MQTT reconnection");
	if (!modem_leaf->modemSendCmd(30000, HERE, "AT+SMCONN")) {
	  post_error(POST_ERROR_LTE, 3);
	  ALERT("Unable to reconnect");
	  ERROR("MQTT reconn fail");
	  return 0;
	}
      }
    }

    char smpub_cmd[512+64];
    snprintf(smpub_cmd, sizeof(smpub_cmd), "AT+SMPUB=\"%s\",%d,%d,%d",
	     t, payload.length(), (int)qos, (int)retain);
    if (!modem_leaf->modemSendExpectPrompt(smpub_cmd, 10000, HERE)) {
      LEAF_ALERT("publish prompt not seen");
      return 0;
    }

    if (!modem_leaf->modemSendCmd(20000, HERE, payload.c_str())) {
      LEAF_ALERT("publish response not seen");
      return 0;
    }
    // fall thru
  }
  else {
    LEAF_ALERT("Publish skipped while MQTT connection is down: %s=>%s", t, p);
  }
  LEAF_RETURN(1);
}

void AbstractPubsubSimcomLeaf::_mqtt_subscribe(String topic, int qos, codepoint_t where)
{
  LEAF_ENTER(L_INFO);
  const char *t = topic.c_str();

  LEAF_NOTICE_AT(CODEPOINT(where), "MQTT SUB %s", t);
  if (pubsub_connected) {

    if (modem_leaf->modemSendCmd(HERE, "AT+SMSUB=\"%s\",%d", topic.c_str(), qos)) {
      LEAF_INFO("Subscription initiated for topic=%s", t);
      if (pubsub_subscriptions) {
	pubsub_subscriptions->put(topic, qos);
      }

    }
    else {
      LEAF_ALERT("Subscription FAILED for topic=%s (maybe already subscribed?)", t);
    }
  }
  else {
    LEAF_ALERT("Warning: Subscription attempted while MQTT connection is down (%s)", t);
  }
  LEAF_LEAVE;
}

void AbstractPubsubSimcomLeaf::_mqtt_unsubscribe(String topic)
{
  const char *t = topic.c_str();
  LEAF_NOTICE("MQTT UNSUB %s", t);

  if (pubsub_connected) {
    if (modem_leaf->modemSendCmd(10000, HERE, "AT+SMUNSUB=\"%s\"", topic.c_str())) {
      LEAF_DEBUG("UNSUBSCRIPTION initiated topic=%s", t);
      pubsub_subscriptions->remove(topic);
    }
    else {
      LEAF_ALERT("Unsubscription FAILED for topic=%s", t);
    }
  }
  else {
    LEAF_ALERT("Warning: Unsubscription attempted while MQTT connection is down (%s)", t);
  }
}

void AbstractPubsubSimcomLeaf::initiate_sleep_ms(int ms)
{
  LEAF_NOTICE("Prepare for deep sleep");

  mqtt_publish("event/sleep",String(millis()/1000,10));

  // Apply sleep in reverse order, highest level leaf first
  int leaf_index;
  for (leaf_index=0; leaves[leaf_index]; leaf_index++);
  for (leaf_index--; leaf_index<=0; leaf_index--) {
    leaves[leaf_index]->pre_sleep(ms/1000);
  }
  
  if (ms == 0) {
    LEAF_ALERT("Initiating indefinite deep sleep (wake source GPIO0)");
  }
  else {
    LEAF_ALERT("Initiating deep sleep (wake sources GPIO0 plus timer %dms)", ms);
  }

  Serial.flush();
  if (ms != 0) {
    // zero means forever
    esp_sleep_enable_timer_wakeup(ms * 1000ULL);
  }
  esp_sleep_enable_ext0_wakeup((gpio_num_t)0, 0);

  esp_deep_sleep_start();
}
// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
