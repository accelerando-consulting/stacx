#pragma once

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <AsyncTCP.h>
#else
#error I dont know who I am!
#endif
#include <cbuf.h>

class IpClientWifi : public Client, virtual public Debuggable
{
protected:
  int slot;
  int timeout = 2;
  bool ip_client_wifi_connected = false;
  AsyncClient *async_client = NULL;
  cbuf *rx_buffer = NULL;
  
public:
IpClientWifi(int slot, int timeout=2)
  : Debuggable(String("tcp_")+slot)
  {
	  this->slot = slot;
	  this->timeout = timeout;
#ifdef ESP8266
	  this->setTimeout(timeout);
#endif
	  this->rx_buffer = new cbuf(512);
  }
  ~IpClientWifi() 
  {
    delete this->rx_buffer;
    if (async_client) {
      delete this->async_client;
    }
  }
  int getSlot() { return slot; }
  void setConnected() {
    LEAF_NOTICE("AsyncTCP client connected");
    ip_client_wifi_connected=true;
  }
  void setDisconnected() {
    LEAF_NOTICE("AsyncTCP client disconnected");
    ip_client_wifi_connected=false;
  }
  virtual uint8_t connected() 
  {
    return ip_client_wifi_connected?1:0;
  }

  virtual int connect(IPAddress ip, uint16_t port)
  {
    return this->connect(ip.toString().c_str(), (int)port);
  }

  virtual void onData(uint8_t *data, size_t len) 
  {
    rx_buffer->write((const char *)data, len);
  }
  
  virtual int connect(const char *host, uint16_t port)
  { 
    LEAF_ENTER(L_NOTICE);
    async_client = new AsyncClient();
    if (async_client) {
      async_client->setRxTimeout(timeout*1000);
    }
    if (async_client && async_client->connect(host, port)) {
      LEAF_NOTICE("Connection pending");
      async_client->onConnect([](void *arg, AsyncClient *client)
      {
	((IpClientWifi *)arg)->setConnected();
      }, this);
      async_client->onDisconnect([](void *arg, AsyncClient *client)
			  {
			    ((IpClientWifi *)arg)->setDisconnected();
			  }, this);
      async_client->onData([](void *arg, AsyncClient *client, void *data, size_t len)
      {
	               NOTICE("Received AsyncTCP client data");
		      ((IpClientWifi *)arg)->onData((uint8_t *)data,len);
		    }, this);
      LEAF_INT_RETURN(2); // pending
    }
    else {
      LEAF_ALERT("Connection failed");
    }
   
    LEAF_INT_RETURN(0); 
  }

  

  virtual size_t write(uint8_t c) 
  {
    return async_client?async_client->write((const char *)&c,1):0;
  }
  virtual size_t write(const uint8_t *buf, size_t size) 
  {
    return async_client?async_client->write((const char *)buf,size):0;
  }
  virtual int available() 
  {
    return rx_buffer->available();
  }
  virtual int read() 
  {
    return rx_buffer->read();
  }
  virtual int read(uint8_t *buf, size_t size) 
  {
    return rx_buffer->read((char *)buf, size);
  }
  virtual int peek() 
  {
    return rx_buffer->peek();
  }
  virtual void flush() 
  {
  }
  virtual void stop() 
  {
    if (async_client) {
      async_client->stop();
      delete async_client;
      async_client=NULL;
    }
  }
    
  virtual operator bool() 
  {
    return connected();
  }
  
};


// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:


