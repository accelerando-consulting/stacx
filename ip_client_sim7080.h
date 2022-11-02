#pragma once

#include "Arduino.h"
#include "Client.h"
#include <memory>
#include <cbuf.h>

class IpClientSim7080 : public IpClientLTE, virtual public TraitDebuggable
{
public:
  IpClientSim7080(AbstractIpModemLeaf *modem, int slot) :
    IpClientLTE(modem, slot)
  {
  }
  
  int connect(const char *host, uint16_t port)
  {
    char cmd[80];
    snprintf(cmd, sizeof(cmd), "AT+CAOPEN=0,%d,\"TCP\",\"%s\",%d",
	     slot, host, port);
    int result;
    int cid;
    if (!modem->modemSendExpectIntPair(cmd, "+CAOPEN: ", &cid, &result, connect_timeout_ms, 2, HERE)) {
      ALERT("CAOPEN failed: %d", result);
      return 1;
    }
    if (cid != slot) {
      ALERT("Wrong CID (%d) for connect result on slot %d", cid, slot);
    }
    _connected = true;
    return 0;
  }
  
  size_t write(const uint8_t *buf, size_t size)
  {
    int len;
    char cmd[40];
    snprintf(cmd, sizeof(cmd), "AT+CASEND=%d,%d", slot, size);
    if (!modem->modemSendExpectInt(cmd,"DATA ACCEPT: ", &len, -1, HERE)) {
      ALERT("CIPSEND failed");
      return 0;
    }
    if (len < size) {
      return modem->modemSendRaw(buf, len, HERE);
    }
    return modem->modemSendRaw(buf, size, HERE);
  }
  
  void stop()
  {
    if (!modem->modemSendCmd(HERE, "AT+CACLOSE=%d", slot)) {
      ALERT("CACLOSE failed");
    }
    else {
      _connected = false;
    }
  }

  void dataIndication(int count) 
  {
    LEAF_ENTER(L_NOTICE);
    char cmd[40];
    int room = rx_buffer->room();
    if (room == 0) {
      LEAF_NOTICE("RX buffer full");
      LEAF_VOID_RETURN;
    }
    
    snprintf(cmd, sizeof(cmd), "AT+CARECV=%d,%d", slot, room);
    // Response will be +CARECV: <len>,<data>
    int len = 0;
    
    if (modem->modemSendExpectInlineInt(cmd, "+CARECV: ", &len, ',', -1, HERE)) {
      int got = modem->modemReadToBuffer(rx_buffer, len);
      if (got != len) {
	LEAF_WARN("CARECV short read");
      }
    }
    else {
      LEAF_WARN("CARECV error");
    }
  }
    

};



// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:


