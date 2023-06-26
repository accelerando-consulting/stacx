//
//@**************************** class TCPClientLeaf *****************************
//
// Make an outbound connection via TCP
//
#include <rBase64.h>

#ifndef TCP_LOG_FILE
#define TCP_LOG_FILE STACX_LOG_FILE
#endif


class TCPClientLeaf : public Leaf
{
public:
  //
  // Declare your leaf-specific instance data here
  //
  String target;
  String host;
  int port;
  bool do_log = false;
  Client *client=NULL;
  int client_slot = -1;
  int timeout = 2;
  int buffer_size=256;
  char *rx_buf=NULL;
  char *tx_buf=NULL;
  bool connected = false;
  int reconnect_sec = 30;
  time_t connected_at =0;
  time_t reconnect_at =0;
  unsigned long sent_count=0;
  unsigned long rcvd_count=0;
  unsigned long sent_total=0;
  unsigned long rcvd_total=0;
  unsigned long conn_count=0;
  unsigned long fail_count=0;
  unsigned long status_sec=60;

  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  TCPClientLeaf(String name, String target="", String host="", int port=0)
    : Leaf("tcpclient", name, NO_PINS)
    , Debuggable(name)
  {
    this->target=target;
    this->host = host;
    this->port = port;
    do_heartbeat = (status_sec>0);
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_NOTICE);
    this->install_taps(target);
    rx_buf = (char *)calloc(buffer_size+1, 1);
    tx_buf = (char *)calloc(buffer_size+1, 1);

    registerLeafBoolValue("log", &do_log, "Log TCP transactions to flash for diagnostics");
    registerValue(HERE, "tcp_host", VALUE_KIND_STR, &host, "Hostname to which TCP client connects");
    registerValue(HERE, "tcp_port", VALUE_KIND_INT, &port, "Port number to which TCP client connects");
    registerValue(HERE, "tcp_reconnect_sec", VALUE_KIND_INT, &reconnect_sec, "Seconds after which to retry TCP connection (0=off)", ACL_GET_SET);
    registerValue(HERE, "tcp_status_sec", VALUE_KIND_INT, &status_sec, "Seconds after which to publish stats", ACL_GET_SET);

    registerCommand(HERE, "connect", "Initiate TCP connection");
    registerCommand(HERE, "disconnect", "Terminate TCP connection");
    registerCommand(HERE, "send", "Send data over TCP connection");
    registerCommand(HERE, "sendline", "Send a line of data over TCP connection");

    if (host.length()) {
      LEAF_NOTICE("TCP client will connect to server at %s:%d", host.c_str(),port);
    }

    LEAF_LEAVE;
  }

  void start(void)
  {
    Leaf::start();
    LEAF_ENTER(L_NOTICE);
    LEAF_NOTICE("IP transport is %s", ipLeaf->describe().c_str());
    LEAF_NOTICE("connected=%s", TRUTH(connected));
    LEAF_NOTICE("host=%d:[%s] port=%d", host.length(), host.c_str(), port);
    LEAF_NOTICE("ipLeaf=%p (%s) connected=%s",
		ipLeaf,ipLeaf?ipLeaf->describe().c_str():"NULL", ipLeaf?TRUTH(ipLeaf->isConnected()):"NULL");
    if (!connected && host.length() && (port>0) && ipLeaf && ipLeaf->isConnected()) {
      LEAF_NOTICE("IP is already online, initiate connect now");
      connect();
    }
    else {
      LEAF_NOTICE("Awaiting IP layer connection");
    }

    LEAF_LEAVE;
  }

  void stop(void)
  {
    LEAF_ENTER(L_WARN);
    disconnect();
    loop(); // process result of disconnect
    Leaf::stop();
    LEAF_LEAVE;
  }

  void status_pub(String prefix="status/")
  {
    if (prefix=="status/") {
      prefix = getName()+"/"+prefix;
    }
    mqtt_publish(prefix+"connected", TRUTH_lc(connected));
    mqtt_publish(prefix+"sent_count", String(sent_count));
    mqtt_publish(prefix+"rcvd_count", String(rcvd_count));
    mqtt_publish(prefix+"sent_total", String(sent_total));
    mqtt_publish(prefix+"rcvd_total", String(rcvd_total));
    mqtt_publish(prefix+"conn_count", String(conn_count));
    mqtt_publish(prefix+"fail_count", String(fail_count));
  }

  void heartbeat(unsigned long uptime)
  {
    // deliberately do not call the superclass method
    status_pub();
  }


  void onTcpConnect()
  {
    LEAF_ENTER(L_NOTICE);
    ++conn_count;
    connected = true;
    connected_at = millis();
    if (do_log) {
      char buf[80];
      snprintf(buf, sizeof(buf), "%s connect %d %s:%d", getNameStr(), conn_count, host.c_str(), port);
      message("fs", "cmd/log/" TCP_LOG_FILE, buf);
    }
    publish("_tcp_connect", String(client_slot), L_NOTICE, HERE);
    LEAF_LEAVE;
  }

  void onTcpDisconnect()
  {
    LEAF_ENTER(L_NOTICE);
    int duration = (millis() - connected_at)/1000;
    LEAF_NOTICE("Duration for TCP connection %d to %s was %ds", conn_count, host.c_str(), duration);
    connected = false;
    sent_total += sent_count;
    sent_count = 0;
    rcvd_total += rcvd_count;
    rcvd_count = 0;
    connected = false;
    client = NULL;
    if (do_log) {
      char buf[80];
      snprintf(buf, sizeof(buf), "%s disconnect %d %s:%d", getNameStr(), conn_count, host.c_str(), port);
      message("fs", "cmd/log/" TCP_LOG_FILE, buf);
    }
    ipLeaf->tcpRelease(client);
    scheduleReconnect();
    publish("_tcp_disconnect", String(client_slot), L_NOTICE, HERE);
    LEAF_LEAVE;
  }


  void scheduleReconnect()
  {
    LEAF_ENTER(L_INFO);
    if (reconnect_sec > 0) {
      LEAF_WARN("Sheduling reconnect in %d sec", reconnect_sec);
      reconnect_at = millis()+(reconnect_sec * 1000);
    }
    LEAF_LEAVE;
  }

  bool connect()
  {
    LEAF_ENTER(L_NOTICE);
    LEAF_NOTICE("Initiating TCP connection to %s:%d", host.c_str(), port);
    client = ipLeaf->tcpConnect(host.c_str(), port, &client_slot);
    if (client && client->connected()) {
      LEAF_NOTICE("Connection %d ready", client_slot);
      onTcpConnect();
    }
    else if (client) {
      LEAF_NOTICE("Connection %d pending", client_slot);
    }
    else {
      LEAF_ALERT("Connection failed");
      ++fail_count;
      connected = false;
      scheduleReconnect();
    }
    LEAF_RETURN(true);
  }

  bool disconnect()
  {
    LEAF_ENTER(L_NOTICE);
    client->stop();
    // loop will pick up the change of state and invoke onTcpDisconnect()
    LEAF_RETURN(true);
  }

  //
  // Arduino loop function
  // (Superclass function will take care of heartbeats)
  //
  void loop(void) {
    Leaf::loop();
    LEAF_ENTER(L_TRACE);

    if (!connected && client && client->connected()) {
      LEAF_WARN("Client %d became connected", client_slot);
      onTcpConnect();
    }
    else if (connected && client && !client->connected()) {
      LEAF_WARN("Client %d was disconnected", client_slot);
      onTcpDisconnect();
    }

    if (!connected && reconnect_at && (millis()>=reconnect_at)) {
      LEAF_ALERT("Initiate reconnect on TCP socket");
      reconnect_at=0;
      connect();
    }
    else if (connected && client && client->available()) {
      int avail=client->available();
      if (avail >= buffer_size) avail=buffer_size-1;
      int got = client->read((uint8_t *)rx_buf, avail);
      LEAF_NOTICE("Got %d of %d bytes from TCP client object", got, avail);
      rx_buf[got]='\0';
      rcvd_count += got;
      this->publish("rcvd", String(rx_buf,got));
    }
  }

  virtual bool valueChangeHandler(String topic, Value *v) {
    LEAF_HANDLER(L_INFO);

    WHEN("tcp_status_sec", {
      do_heartbeat = (status_sec>0);
      })
    else {
      Leaf::valueChangeHandler(topic, v);
    }
    LEAF_HANDLER_END;
  }

  virtual bool commandHandler(String type, String name, String topic, String payload) {
    LEAF_HANDLER(L_INFO);

    WHEN("connect",{
      LEAF_NOTICE("Instructed by %s to (re)connect", name);
      if (!connected) {
	connect();
      }
    })
    ELSEWHEN("disconnect",{
      LEAF_WARN("Instructed by %s to disconnect", name);
      disconnect();
        int retry = payload.toInt();
        if (retry) {
          LEAF_NOTICE("Will retry in %dsec", retry);
          reconnect_at = millis() + (retry*1000);
        }
    })
    ELSEWHEN("send",{
      if (connected) {
        int wrote = client->write((uint8_t *)payload.c_str(), payload.length());
        //LEAF_DEBUG("Wrote %d bytes to socket", wrote);
        //DumpHex(L_NOTICE, "send", payload.c_str(), payload.length());
        sent_count += wrote;
      }
      else {
        LEAF_ALERT("send handler: not connected");
      }
    })
    ELSEWHEN("sendline",{
      if (connected) {
	int len = snprintf(tx_buf, buffer_size, "%s\r\n", payload.c_str());
	int wrote = client->write((uint8_t *)tx_buf, len);
	//LEAF_DEBUG("Wrote %d/%d bytes to socket", wrote, len);
	//DumpHex(L_NOTICE, "sendline", tx_buf, wrote);
	sent_count += wrote;
      }
      else {
	LEAF_ALERT("sendline handler: not connected");
      }
    })
    else {
      Leaf::commandHandler(type, name, topic, payload);
    }

    LEAF_HANDLER_END;
  }
  

  //
  // MQTT message callback
  //
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;

    if (!topic.startsWith("_ip")) {
      // don't care about things other than _ip_*
    }
    else if (topic.startsWith("send")) {
      LEAF_NOTICE("%s/%s: %s, [%d bytes]", type.c_str(), name.c_str(), topic.c_str(), payload.length());
    }
    else {
      LEAF_INFO("%s/%s: %s <= %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }
    String ip_name = ipLeaf?ipLeaf->getName():"nonesuch";
    if (ipLeaf) {
      //LEAF_NOTICE("IP leaf is %p:%s name=[%s] {%s}", ipLeaf, ipLeaf->describe().c_str(), ipLeaf->getName().c_str(), ipLeaf->getNameStr());
    }
    else {
      LEAF_ALERT("ipLeaf is null!");
    }

    WHENFROM(ip_name, "_ip_connect", {
	if (!connected) {
	  LEAF_NOTICE("IP is online, initiate TCP connect");
	  connect();
	}
      })
    ELSEWHEN("_ip_connect", {
	//LEAF_INFO("ignore _ip_connect from %s because it is not expected source [%s]", name.c_str(), ip_name.c_str());
    })
    ELSEWHENFROM(ip_name, "_ip_disconnect", {
	if (connected) {
	  LEAF_NOTICE("IP is offline, shut down TCP connection");
	  disconnect();
	}
      })
    ELSEWHEN("_ip_disconnect", {
	//LEAF_INFO("ignore _ip_disconnect from %s because it is not expected source [%s]", name.c_str(), ip_name.c_str());
    })
    else if (!handled) {
      // pass to superclass
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    return handled;
  }

};



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
