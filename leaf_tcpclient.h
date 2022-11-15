//
//@**************************** class TCPClientLeaf *****************************
// 
// Make an outbound connection via TCP
// 
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#else
#include <WiFi.h>
#include <AsyncTCP.h>
#endif

#define USE_ASYNC 1

class TCPClientLeaf : public Leaf
{
public:
  // 
  // Declare your leaf-specific instance data here
  // 
  String target;
  String host;
  int port;
#ifdef USE_ASYNC
  AsyncClient client;
#else
  WiFiClient client;
#endif
  int timeout = 2;
  int buffer_size=256;
  char *rx_buf=NULL;
  char *tx_buf=NULL;
  bool connected = false;
  time_t reconnect_at =0;

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
#ifndef USE_ASYNC
    client.setTimeout(timeout);
#endif
    rx_buf = (char *)calloc(buffer_size+1, 1);
    tx_buf = (char *)calloc(buffer_size+1, 1);

    getPref("tcp_host", &host, "Hostname to which TCP client connects");
    getIntPref("tcp_port", &port, "Port number to which TCP client connects");

    if (host.length()) {
      LEAF_NOTICE("TCP client will connect to server at %s:%d", host.c_str(),port);
    }
    
    LEAF_LEAVE;
  }

  void scheduleReconnect() 
  {
    LEAF_ENTER(L_NOTICE);
    reconnect_at = millis()+15*1000;
    LEAF_LEAVE;
  }

  void onConnect() 
  {
    LEAF_NOTICE("TCP Client connected");
    connected = true;
  }
  
  void onDisconnect() 
  {
    LEAF_NOTICE("TCP Client disconnected");
    connected = false;
    scheduleReconnect();
  }

  void onData(char *buf, size_t len) 
  {
    LEAF_NOTICE("Got %d bytes from TCP socket", len);
    String payload;
    payload.concat((const char *)buf, len);
    this->publish("rcvd", payload);
  }

  bool connect() 
  {
    LEAF_ENTER(L_NOTICE);
#ifdef USE_ASYNC
    LEAF_NOTICE("Initiating TCP connection to %s:%d", host.c_str(), port);
    if (client.connect(host.c_str(), port)) {
      LEAF_NOTICE("Connection pending");
      client.onConnect([](void *arg, AsyncClient *client)
		       {
			 NOTICE("AsyncTCP client connected");
			 ((TCPClientLeaf *)arg)->onConnect();
		       }, this);
      client.onDisconnect([](void *arg, AsyncClient *client)
			  {
			    NOTICE("AsyncTCP client disconnected");
			    ((TCPClientLeaf *)arg)->onDisconnect();
			  }, this);
      client.onData([](void *arg, AsyncClient *client, void *data, size_t len)
		    {
		      NOTICE("Received AsyncTCP client data");
		      ((TCPClientLeaf *)arg)->onData((char *)data,len);
		    }, this);
    }
    else {
      LEAF_ALERT("Connection failed");
    }
#else
    if (!client.connect(host.c_str(), port)) {
      LEAF_ALERT("Connection failed");
      connected = false;
      LEAF_RETURN(false);
    }
    connected = true;
#endif
    LEAF_RETURN(true);
  }

  bool disconnect() 
  {
    LEAF_ENTER(L_NOTICE);
    client.close();
    LEAF_RETURN(true);
  }
  
  // 
  // Arduino loop function
  // (Superclass function will take care of heartbeats)
  // 
  void loop(void) {
    Leaf::loop();
    static bool was_connected = false;

#ifdef USE_ASYNC
    if (!was_connected && connected) {
      was_connected = connected;
      LEAF_NOTICE("Connection complete.  Publishing TCP connect event.");
      publish("_tcp_connect", String(client.getLocalPort()));
    }
    else if (was_connected && !connected) {
      was_connected = connected;
      LEAF_ALERT("Connection concluded.  Publishing TCP disconnect event.");
      publish("_tcp_disconnect", "");
    }
#else
    if (connected && !client) {
      LEAF_ALERT("TCP client was disconnected");
      connected = false;
      return;
    }
    if (!connected && client) {
      LEAF_ALERT("TCP client is now connected");
      connected = true;
    }
#endif

    if (!connected && reconnect_at && (millis()>=reconnect_at)) {
      LEAF_ALERT("Initiate reconnect");
      reconnect_at=0;
      connect();
    }
    
#ifndef USE_ASYNC
    if (!connected) return;

    int pos = 0;
    while(client.available()){
      rx_buf[pos++]=client.read();
      rx_buf[pos]='\0';
      if (pos >= buffer_size) break;
    }
    if (pos == 0) return;

    onData(buf, pos);
#endif // !USE_ASYNC
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
      LEAF_WARN("%s/%s: %s <= %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }
    
    WHENFROM("wifi", "_ip_connect", {
	if (!connected) {
	  LEAF_NOTICE("IP is online, initiate TCP connect");
	  connect();
	}
	handled = true;
      })
      ELSEWHENFROM("wifi", "_ip_disconnect", {
	if (connected) {
	  LEAF_NOTICE("IP is offline, shut down TCP connection");
	  disconnect();
	}
	handled = true;
      })
    ELSEWHEN("disconnect",{
	LEAF_NOTICE("Instructed by %s to disconnect", name);
	disconnect();
	int retry = payload.toInt();
	if (retry) {
	  LEAF_NOTICE("Will retry in %dsec", retry);
	  reconnect_at = millis() + (retry*1000);
	}
	connected = false;
      })
    ELSEWHEN("send",{
	if (connected) {
	  client.write(payload.c_str(), payload.length());
	  client.flush();
	  LEAF_NOTICE("Wrote %d bytes to socket", payload.length());
	}
	else {
	  LEAF_ALERT("send handler: not connected");
	}
	handled=true;
      })
    ELSEWHEN("sendline",{
	if (connected) {
	  client.add(payload.c_str(), payload.length());
	  client.add("\r\n", 2);
	  client.send();
	  LEAF_NOTICE("Wrote %d bytes to socket", payload.length()+2);
	}
	else {
	  LEAF_ALERT("sendline handler: not connected");
	}
	handled=true;
      });
#if 0
    ELSEWHEN("set/other",{set_other(payload)});
#endif

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
