//
//@**************************** class TCPClientLeaf *****************************
// 
// Make an outbound connection via TCP
// 
#include <rBase64.h>

class TCPClientLeaf : public Leaf
{
public:
  // 
  // Declare your leaf-specific instance data here
  // 
  String target;
  String host;
  int port;
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

  // 
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  TCPClientLeaf(String name, String target, String host="", int port=0) : Leaf("tcpclient", name, NO_PINS){
    this->target=target;
    this->host = host;
    this->port = port;
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

    registerValue(HERE, "tcp_host", VALUE_KIND_STR, &host, "Hostname to which TCP client connects");
    registerValue(HERE, "tcp_port", VALUE_KIND_INT, &port, "Port number to which TCP client connects");
    registerValue(HERE, "tcp_reconnect_sec", VALUE_KIND_INT, &reconnect_sec, "Seconds after which to retry TCP connection (0=off)", ACL_GET_SET);

    if (host.length()) {
      LEAF_NOTICE("TCP client will connect to server at %s:%d", host.c_str(),port);
    }
    
    LEAF_LEAVE;
  }

  void start(void) 
  {
    Leaf::start();
    LEAF_ENTER(L_NOTICE);
    if (!connected && host.length() && port>0 && ipLeaf && ipLeaf->isConnected()) {
      LEAF_INFO("IP is already online, initiate connect");
      connect();
    }
    LEAF_LEAVE;
  }

  void status_pub() 
  {
    mqtt_publish("connected", TRUTH_lc(connected));
    mqtt_publish("sent_count", String(sent_count));
    mqtt_publish("rcvd_count", String(rcvd_count));
    mqtt_publish("sent_total", String(sent_total));
    mqtt_publish("rcvd_total", String(rcvd_total));
    mqtt_publish("conn_count", String(conn_count));
    mqtt_publish("fail_count", String(fail_count));
  }

  void onTcpConnect() 
  {
    LEAF_ENTER(L_NOTICE);
    ++conn_count;
    connected = true;
    connected_at = millis();
    publish("_tcp_connect", String(client_slot));
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
    ipLeaf->tcpRelease(client);
    client = NULL;
    scheduleReconnect();
    LEAF_LEAVE;
  }
  
  
  void scheduleReconnect() 
  {
    LEAF_ENTER(L_NOTICE);
    if (reconnect_sec > 0) {
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
      LEAF_NOTICE("Client %d became connected", client_slot);
      onTcpConnect();
    }
    else if (connected && client && !client->connected()) {
      LEAF_NOTICE("Client %d was disconnected", client_slot);
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
      LEAF_NOTICE("Got %d of %d bytes from client", got, avail);
      rx_buf[got]='\0';
      rcvd_count += got;
      this->publish("rcvd", String(rx_buf,got));
    }
  }
  
  // 
  // MQTT message callback
  //
  bool mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_INFO);
    bool handled = false;

    if (topic.startsWith("send")) {
      LEAF_NOTICE("%s/%s: %s, [%d bytes]", type.c_str(), name.c_str(), topic.c_str(), payload.length());
    }
    else {
      LEAF_NOTICE("%s/%s: %s <= %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }
    
    WHEN("_ip_connect", {
	if (!connected) {
	  LEAF_NOTICE("IP is online, initiate TCP connect");
	  connect();
	}
	handled = true;
      })
    ELSEWHEN("_ip_disconnect", {
	if (connected) {
	  LEAF_NOTICE("IP is offline, shut down TCP connection");
	  disconnect();
	}
	handled = true;
      })
    ELSEWHEN("cmd/connect",{
	LEAF_NOTICE("Instructed by %s to (re)connect", name);
	if (!connected) {
	  connect();
	}
      })
    ELSEWHEN("cmd/disconnect",{
	LEAF_WARN("Instructed by %s to disconnect", name);
	disconnect();
	int retry = payload.toInt();
	if (retry) {
	  LEAF_NOTICE("Will retry in %dsec", retry);
	  reconnect_at = millis() + (retry*1000);
	}
      })
    ELSEWHEN("cmd/send",{
	if (connected) {
	  int wrote = client->write((uint8_t *)payload.c_str(), payload.length());
	  LEAF_DEBUG("Wrote %d bytes to socket", wrote);
	  DumpHex(L_NOTICE, "send", payload.c_str(), payload.length());
	  sent_count += wrote;
	}
	else {
	  LEAF_ALERT("send handler: not connected");
	}
	handled=true;
      })
    ELSEWHEN("cmd/sendline",{
	if (connected) {
	  int len = snprintf(tx_buf, buffer_size, "%s\r\n", payload.c_str());
	  int wrote = client->write((uint8_t *)tx_buf, len);
	  LEAF_DEBUG("Wrote %d/%d bytes to socket", wrote, len);
	  DumpHex(L_NOTICE, "sendline", tx_buf, wrote);
	  sent_count += wrote;
	}
	else {
	  LEAF_ALERT("sendline handler: not connected");
	}
	handled=true;
      });

    if (!handled) {
      // pass to superclass
      handled = Leaf::mqtt_receive(type, name, topic, payload);
    }
    
    return handled;
  }
    
};



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
