#pragma once

#include "Arduino.h"
#include "Client.h"
#include <memory>
#include <cbuf.h>

class IpClientLTE : public Client 
{
protected:
  TraitModem *modem = NULL;
  int slot;
  bool _connected;
  cbuf *rx_buffer = NULL;
  int connect_timeout_ms = 10000;
  
public:
  IpClientLTE(AbstractIpModemLeaf *modem, int slot) 
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

  virtual const char *get_name_str() { return modem?modem->get_name_str():""; }
  
  int getSlot() { return slot; }

  virtual void dataIndication(int count=0) {}

  int readToBuffer(size_t size) 
  {
    return modem->modemReadToBuffer(rx_buffer, size);
  }

  int connect(IPAddress ip, uint16_t port)
  {
    return this->connect(ip.toString().c_str(), (int)port);
  }
  
  int connect(const char *host, uint16_t port)
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
  
  size_t write(uint8_t c)
  {
    return write(&c, 1);
  }
  
  size_t write(const uint8_t *buf, size_t size)
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
  
  int available()
  {
    return rx_buffer->available();
  }
  
  int read()
  {
    return rx_buffer->read();
  }
  
  int read(uint8_t *buf, size_t size)
  {
    return rx_buffer->read((char *)buf, size);
  }
  
  int peek()
  {
    return rx_buffer->peek();
  }
  
  void flush()
  {
  }
  
  void stop()
  {
    if (!modem->modemSendCmd(HERE, "AT+CIPCLOSE=%d", slot)) {
      ALERT("CIPCLOSE failed");
    }
    else {
      _connected = false;
    }
  }
  
  uint8_t connected()
  {
    return _connected;
  }
  
  operator bool() 
  {
    return connected();
  }
  
};


// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:


