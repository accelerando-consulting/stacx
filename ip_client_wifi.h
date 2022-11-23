#pragma once

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#else
#include <WiFi.h>
#include <AsyncTCP.h>
#endif

class IpClientWifi : public AsyncClient, virtual public TraitDebuggable
{
protected:
  int slot;
  int timeout=2;
  bool IpClientWifiConnected=fasle;
  
public:
IpClientWifi(int slot, int timeout=2)
  : AsyncClient()
  , TraitDebuggable(String("tcp_")+slot)
  {
	  this->slot = slot;
	  this->timeout = timeout;
	  this->setTimeout(timeout);
  }
  int getSlot() { return slot; }
  void setConnected() {
    LEAF_NOTICE("AsyncTCP client connected");
    IpClientWifiConnected=true;
  }
  void setDisconnected() {
    LEAF_NOTICE("AsyncTCP client disconnected");
    IpClientWifiConnected=false;
  }

  virtual int connect(IPAddress ip, uint16_t port)
  {
    return this->connect(ip.toString().c_str(), (int)port);
  }
  
  virtual int connect(const char *host, uint16_t port)
  { 
    if (AsyncClient::connect(host, port)) {
      LEAF_NOTICE("Connection pending");
      onConnect([](void *arg, AsyncClient *client)
      {
	((IPClientWifi *)arg)->setConnected();
      }, this);
      client.onDisconnect([](void *arg, AsyncClient *client)
			  {
			    ((IpClientWifi *)arg)->setDisconnected();
			  }, this);
/*
      client.onData([](void *arg, AsyncClient *client, void *data, size_t len)
		    {
		      NOTICE("Received AsyncTCP client data");
		      ((TCPClientLeaf *)arg)->onData((char *)data,len);
		    }, this);
*/
    }
    else {
      LEAF_ALERT("Connection failed");
    }
   
    return true;
  }
  
};


// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:


