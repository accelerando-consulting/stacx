#pragma once

#include "Arduino.h"
#include "Client.h"
#include <memory>
#include <cbuf.h>

class IpClientLTE : public Client, virtual public TraitDebuggable
{
protected:
  TraitModem *modem = NULL;
  bool _connected;
  cbuf *rx_buffer = NULL;
  int connect_timeout_ms = 10000;
  int slot;
  
public:
  IpClientLTE(AbstractIpModemLeaf *modem, int slot) 
    : TraitDebuggable(String("tcp_")+slot)
  {
    this->modem = modem;
    this->slot = slot;
    this->_connected = false;
    this->rx_buffer = new cbuf(4096);
  }
  ~IpClientLTE() 
  {
    delete this->rx_buffer;
  }

  int getSlot() { return slot; }
  virtual void disconnectIndication() 
  {
    _connected=false;
  }

  virtual void dataIndication(int count=0) {}

  int readToBuffer(size_t size) 
  {
    return modem->modemReadToBuffer(rx_buffer, size, HERE);
  }

  virtual int connect(IPAddress ip, uint16_t port)
  {
    return this->connect(ip.toString().c_str(), (int)port);
  }
  
  virtual int connect(const char *host, uint16_t port)
  {
    if (!modem->modemSendCmd(HERE,
			     "AT+CIPSTART=%d,\"TCP\",\"%s\",%d",
			     slot, host, (int)port)) {
      ALERT("CIPSTART failed");
      return 1;
    }
    _connected = true;
    return 0;
  }
  
  virtual size_t write(uint8_t c)
  {
    return write(&c, 1);
  }
  
  virtual size_t write(const uint8_t *buf, size_t size)
  {
    int len;
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d,%d", slot, size);
    if (!modem->modemSendExpectInt(cmd,"DATA ACCEPT: ", &len, -1, HERE)) {
      ALERT("CIPSEND failed");
      return 0;
    }
    if (len < size) {
      return modem->modemSendRaw(buf, len, HERE);
    }
    return modem->modemSendRaw(buf, size, HERE);
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
    if (!modem->modemSendCmd(HERE, "AT+CIPCLOSE=%d", slot)) {
      ALERT("CIPCLOSE failed");
    }
    else {
      _connected = false;
    }
  }
  
  virtual uint8_t connected()
  {
    return _connected;
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


