#pragma once
#include "abstract_pubsub.h"
#include "abstract_ip_simcom.h"

//
//@********************** class AbstractPubsubSimcomLeaf ***********************
//
// This class encapsulates an Mqtt connection that utilises an AT-command
// set within a simcom LTE modem module
//
//

class AbstractPubsubSimcomLeaf : public AbstractPubsubLeaf
{
public:
  AbstractPubsubSimcomLeaf(String name, String target, bool use_ssl=true, bool use_device_topic=true, bool run = true)
    : AbstractPubsubLeaf(name, target, use_ssl, use_device_topic)
    , Debuggable(name)
  {
    LEAF_ENTER(L_INFO);
    this->run = run;
    this->pubsub_connect_timeout_ms = 20000;
    // further the setup happens in the superclass
    LEAF_LEAVE;
  }

  virtual void setup();
  virtual void start();
  virtual void loop(void);
  virtual void status_pub(void);
  virtual uint16_t _mqtt_publish(String topic, String payload, int qos=0, bool retain=false);
  virtual void _mqtt_subscribe(String topic, int qos=0, codepoint_t where=undisclosed_location);
  virtual void _mqtt_unsubscribe(String topic);
  virtual void mqtt_do_subscribe();
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false);
  virtual bool pubsubConnect(void);
  virtual bool pubsubConnectStatus(void);
  virtual void pubsubDisconnect(bool deliberate=true);
  void pubsubOnConnect(bool do_subscribe);
  virtual void pre_sleep(int duration=0);


protected:
  //
  // Network resources
  //
  AbstractIpSimcomLeaf *modem_leaf = NULL;
  bool pubsub_reboot_modem = false;
  bool pubsub_onconnect_imei = false;
  bool pubsub_onconnect_iccid = false;
  

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

  registerCommand(HERE,"pubsub_status", "report the status of pubsub connection");

  //
  // Set up the MQTT Client
  //
  modem_leaf = (AbstractIpSimcomLeaf *)ipLeaf;
  if (modem_leaf == NULL) {
    LEAF_ALERT("Modem leaf not found");
  }
  registerValue(HERE, "pubsub_onconnect_iccid", VALUE_KIND_BOOL, &pubsub_onconnect_iccid, "Publish device's ICCID (SIM number) upon connection");
  registerValue(HERE, "pubsub_onconnect_imei", VALUE_KIND_BOOL, &pubsub_onconnect_imei, "Publish device's IMEI (GSM mac) upon connection");

  registerValue(HERE, "pubsub_reboot_modem", VALUE_KIND_BOOL, &pubsub_reboot_modem, "Reboot LTE modem if connect fails");

  LEAF_VOID_RETURN;
}

void AbstractPubsubSimcomLeaf::start()
{
  AbstractPubsubLeaf::start();
  LEAF_ENTER(L_INFO);

  //
  // Set up the MQTT Client
  //
  modem_leaf = (AbstractIpSimcomLeaf *)ipLeaf;
  if (modem_leaf == NULL) {
    LEAF_ALERT("Modem leaf not found");
    run = false;
  }

  LEAF_VOID_RETURN;
}

bool AbstractPubsubSimcomLeaf::pubsubConnectStatus() 
{
  LEAF_ENTER(L_NOTICE);
  int i;

  
  
  if (!modem_leaf->modemSendExpectInt("AT+SMSTATE?","+SMSTATE: ", &i, -1, HERE)) 
  {
    LEAF_ALERT("Cannot get connected status");
    i = 0;
  }
  bool result = (i!=0);
  LEAF_BOOL_RETURN(result);
}

void AbstractPubsubSimcomLeaf::mqtt_do_subscribe() 
{
  AbstractPubsubLeaf::mqtt_do_subscribe();
}

void AbstractPubsubSimcomLeaf::status_pub() 
{
  char status[48];
  uint32_t secs;
  if (pubsub_connected) {
    secs = (millis() - pubsub_connect_time)/1000;
    snprintf(status, sizeof(status), "%s online as %s %d:%02d", getNameStr(), pubsub_client_id.c_str(), secs/60, secs%60);
  }
  else {
    secs = (millis() - pubsub_disconnect_time)/1000;
    snprintf(status, sizeof(status), "%s offline %d:%02d", getNameStr(), secs/60, secs%60);
  }
  mqtt_publish("status/pubsub_status", status);
}

  
bool AbstractPubsubSimcomLeaf::mqtt_receive(String type, String name, String topic, String payload, bool direct)
{
  LEAF_ENTER(L_DEBUG);
  bool handled = false;
  LEAF_NOTICE("%s/%s => %s, %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

  WHENFROM("lte", "_ip_connect",{
      if (pubsub_autoconnect) {
	LEAF_NOTICE("LTE IP is online, autoconnecting MQTT");
	pubsubConnect();
      }
    })
  ELSEWHENFROM("lte", "_ip_disconnect",{
    if (pubsub_connected) {
      LEAF_NOTICE("LTE IP has disconnected, recording MQTT offline");
      pubsubDisconnect();
    }
  })
  ELSEWHEN("cmd/pubsub_status",{
      // this looks like it could be common code in abstract_pubsub, but no.
      // it is potentially going to be handled as a leaf command by multiple pubsub leaves
      status_pub();
  })
  else {
    handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
  }

  return handled;
}

void AbstractPubsubSimcomLeaf::loop()
{
  AbstractPubsubLeaf::loop();

  if (isConnected() &&
      (pubsub_broker_heartbeat_topic.length() > 0) &&
      (pubsub_broker_keepalive_sec > 0) &&
      (last_broker_heartbeat > 0)) {
    int sec_since_last_heartbeat = (millis() - last_broker_heartbeat)/1000;
    if (sec_since_last_heartbeat > pubsub_broker_keepalive_sec) {
      LEAF_ALERT("Declaring pubsub offline due to broker heartbeat timeout (%d > %d)",
		 sec_since_last_heartbeat, pubsub_broker_keepalive_sec);
      pubsubDisconnect(false);
    }
  }
}

void AbstractPubsubSimcomLeaf::pre_sleep(int duration)
{
  LEAF_ENTER_INT(L_NOTICE, duration);
  if (isConnected()) {
    mqtt_publish("event/sleep",String(millis()/1000));
    pubsubDisconnect(true);
  }
  AbstractPubsubLeaf::pre_sleep(duration);
  LEAF_LEAVE;
}

void AbstractPubsubSimcomLeaf::pubsubDisconnect(bool deliberate) {
  AbstractPubsubLeaf::pubsubDisconnect(deliberate);
  LEAF_ENTER_BOOL(L_INFO, deliberate);
  if (modem_leaf->modemSendCmd(25000, HERE, "AT+SMDISC")) {
      LEAF_NOTICE("Pubsub disconnect command sent to modem");
      if (!pubsubConnectStatus()) {
	LEAF_NOTICE("Pubsub connection state is now disconnected");
	pubsubOnDisconnect();
	pubsub_connect_notified = false;
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


  bool result = AbstractPubsubLeaf::pubsubConnect();
  if (result == false) {
    LEAF_ALERT("Superclass denied connect");
    return false;
  }
  
  LEAF_ENTER(L_NOTICE);
  static char buf[2048];

  if (!modem_leaf) {
    LEAF_ALERT("Modem leaf not found");
    LEAF_BOOL_RETURN(false);
  }

  if (!modem_leaf->isConnected()) {
    if ((modem_leaf->getConnectCount() == 0) || !pubsub_ip_autoconnect) {
      // We are not configured to try to connect IP on demand
      LEAF_ALERT("Not connected to cell network, wait till later");
      LEAF_BOOL_RETURN(false);
    }
    LEAF_WARN("Attempting to revive IP Connection");
    ipLeaf->ipConnect();
  }

  if (!modem_leaf->modemWaitPortMutex(HERE)) {
    LEAF_ALERT("Could not acquire port mutex");
    LEAF_BOOL_RETURN(false);
  }

  if (!modem_leaf->modemSendCmd(HERE,"AT")) {
    LEAF_ALERT("Modem is not responsive");
    modem_leaf->modemReleasePortMutex(HERE);
    LEAF_BOOL_RETURN(false);
  }
  
  // If not already connected, connect to MQTT
  bool was_connected = pubsubConnectStatus();
  
  if (was_connected && pubsub_reuse_connection) {
    modem_leaf->modemReleasePortMutex(HERE);
    LEAF_NOTICE("Already connected to MQTT broker.");
    if (!pubsub_connected) {
      pubsubOnConnect(false);
      pubsub_connect_notified = pubsub_connected;
    }
    LEAF_BOOL_RETURN_SLOW(2000, true);
  }
  else if (was_connected && !pubsub_reuse_connection) {
    LEAF_WARN("Pubsub was unexpectedly found connected.  Bounce session");
    pubsubDisconnect();
  }
    

  LEAF_NOTICE("Establishing connection to MQTT broker %s => %s:%d",
	      device_id, pubsub_host.c_str(), pubsub_port);
  pubsub_connected = false;

  modem_leaf->modemSetParameter("SMCONF", "CLEANSS", String(pubsub_use_clean_session?1:0), HERE);
  modem_leaf->modemSetParameterQuoted("SMCONF", "CLIENTID", String(device_id),HERE);
  pubsub_client_id = device_id;

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
    if (hasPriority()) {
      String p = getPriority();
      if (p=="normal") {
	p="admin";
      }
      pubsub_lwt_topic = base_topic + p + "/status/presence";
    }
    else {
      pubsub_lwt_topic = base_topic + "status/presence";
    }
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

  //LEAF_INFO("Initiating MQTT connect");
  int retry = 1;
  const int max_retries = 1;
  int initialState;

  while (!pubsubConnectStatus() && (retry <= max_retries)) {

    if (modem_leaf->modemSendCmd(pubsub_connect_timeout_ms, HERE, "AT+SMCONN")) {
      LEAF_NOTICE("Connection succeeded");
      pubsubSetConnected();
      break;
    }
    
    LEAF_ALERT("ERROR: Failed to connect to broker.");
    ++retry;
  }

  modem_leaf->modemReleasePortMutex(HERE);
  if (pubsub_connected) {
    LEAF_NOTICE("Connected to broker.");
    // let loop do this
    //pubsubOnConnect(true);
    //pubsub_connect_notified = true;
  }
  else {
    LEAF_ALERT("MQTT connect failed");
    if (pubsub_reboot_modem) {
      modem_leaf->ipModemSetNeedsReboot(); // the modem wants a reboot
    }
    post_error(POST_ERROR_PUBSUB, 3);
  }
    
  LEAF_BOOL_RETURN_SLOW(2000, pubsub_connected);
}

void AbstractPubsubSimcomLeaf::pubsubOnConnect(bool do_subscribe)
{
  LEAF_ENTER_BOOL(L_NOTICE,do_subscribe);
  //LEAF_INFO("Connected to MQTT");
  AbstractPubsubLeaf::pubsubOnConnect(do_subscribe);

  // Once connected, publish an announcement...
  if (pubsub_onconnect_iccid) {
    message(ipLeaf, "get/ip_device_iccid", "1");
  }
  if (pubsub_onconnect_imei) {
    message(ipLeaf, "get/ip_device_imei", "1");
  }

  //LEAF_INFO("MQTT Connection setup complete");

  LEAF_LEAVE_SLOW(2000);
}


uint16_t AbstractPubsubSimcomLeaf::_mqtt_publish(String topic, String payload, int qos, bool retain)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_INFO("%sPUB %s => [%s]", pubsub_loopback?"LOOPBACK ":"", topic.c_str(), payload.c_str());

  if (pubsub_loopback) {
    storeLoopback(topic, payload);
    return 0;
  }
  int i;

  if (ipLeaf->getConnectCount() && ipLeaf->canRun() && !ipLeaf->isConnected() && pubsub_ip_autoconnect) {

    // We have been connected, but connection dropped, reconnect
    // Only do this if we've already had a successful connection.
    LEAF_WARN("IP connection is currently down, reconnect");
    if (!ipLeaf->ipConnect("mqtt_publish")) {
      LEAF_WARN("Reconnection failed");
      return 0;
    }
  }

  if (pubsub_connected) {
    if (!pubsubConnectStatus()) {
      LEAF_ALERT("Lost MQTT connection");
      if (ipLeaf->isConnected()) {
	LEAF_ALERT("Try MQTT reconnection");
	if (!modem_leaf->modemSendCmd(30000, HERE, "AT+SMCONN")) {
	  post_error(POST_ERROR_LTE, 3);
	  ALERT("Unable to reconnect");
	  pubsubOnDisconnect(); // it's official now, we're offline
	  ERROR("PUBSUB fail");
	  LEAF_INT_RETURN(0);
	}
      }
    }

    if (!modem_leaf->modemWaitPortMutex(HERE)) {
      LEAF_ALERT("Could not acquire port mutex");
      LEAF_INT_RETURN(0);
    }

    modem_leaf->ipCommsState(TRANSACTION, HERE);
    char smpub_cmd[512+64];
    snprintf(smpub_cmd, sizeof(smpub_cmd), "AT+SMPUB=\"%s\",%d,%d,%d",
	     topic.c_str(), payload.length(), (int)qos, (int)retain);
    if (!modem_leaf->modemSendExpectPrompt(smpub_cmd, 10000, HERE)) {
      LEAF_ALERT("publish prompt not seen");
      modem_leaf->modemReleasePortMutex(HERE);
      modem_leaf->ipCommsState(REVERT, HERE);
      LEAF_INT_RETURN(0);
    }

    if (!modem_leaf->modemSendCmd(20000, HERE, payload.c_str())) {
      LEAF_ALERT("publish response not seen");
      modem_leaf->ipCommsState(REVERT, HERE);
      modem_leaf->modemReleasePortMutex(HERE);
      LEAF_INT_RETURN(0);
    }
    if (isConnected()) {
      modem_leaf->ipCommsState(REVERT, HERE);
    }
    else {
      modem_leaf->ipCommsState(WAIT_PUBSUB, HERE);
    }
    modem_leaf->modemReleasePortMutex(HERE);
    
    // fall thru
  }
  else if (send_queue) {
    _mqtt_queue_publish(topic, payload, qos, retain);
  }
  else if (pubsub_warn_noconn) {
    LEAF_WARN("Publish skipped while MQTT connection is down: %s=>%s", topic.c_str(), payload.c_str());
  }
  LEAF_RETURN_SLOW(2000,1);
}

void AbstractPubsubSimcomLeaf::_mqtt_subscribe(String topic, int qos,codepoint_t where)
{
  LEAF_ENTER(L_INFO);

  LEAF_NOTICE_AT(CODEPOINT(where), "MQTT SUB %s", topic.c_str());
  if (pubsub_connected) {

    if (modem_leaf->modemSendCmd(HERE, "AT+SMSUB=\"%s\",%d", topic.c_str(), qos)) {
      //LEAF_INFO("Subscription initiated for topic=%s", topic.c_str());
      // skip this on esp8266 to save ram
      if (pubsub_subscriptions) {
	pubsub_subscriptions->put(topic, qos);
      }
    }
    else {
      LEAF_NOTICE("Subscription FAILED for topic=%s (maybe already subscribed?)", topic.c_str());
    }
  }
  else {
    LEAF_ALERT("Warning: Subscription attempted while MQTT connection is down (%s)", topic.c_str());
  }
  LEAF_LEAVE;
}

void AbstractPubsubSimcomLeaf::_mqtt_unsubscribe(String topic)
{
  LEAF_NOTICE("MQTT UNSUB %s", topic.c_str());

  if (pubsub_connected) {
    if (modem_leaf->modemSendCmd(10000, HERE, "AT+SMUNSUB=\"%s\"", topic.c_str())) {
      if (pubsub_subscriptions) {
	pubsub_subscriptions->remove(topic);
      }
    }
    else {
      LEAF_ALERT("Unsubscription FAILED for topic=%s", topic.c_str());
    }
  }
  else {
    LEAF_ALERT("Warning: Unsubscription attempted while MQTT connection is down (%s)", topic.c_str());
  }
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
