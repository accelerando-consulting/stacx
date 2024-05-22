#pragma once

#include "Arduino.h"
#include "Client.h"
#include <memory>
#include <cbuf.h>

class IpClientSim7080 : public IpClientLTE, virtual public Debuggable
{
public:
  IpClientSim7080(AbstractIpModemLeaf *modem, int slot)
    : IpClientLTE(modem, slot)
    , Debuggable(String("tcp_")+slot)
  {
  }
  
  virtual int connect(const char *host, uint16_t port)
  {
    LEAF_ENTER(L_NOTICE);
    char cmd[80];
    int result=-1;
    int cid;
    _connected = false;


    if (!modem->modemWaitPortMutex(HERE)) {
      LEAF_ALERT("Cannot obtain modem mutex");
      LEAF_INT_RETURN(0);
    }

    // this might fail if already set
    //modem->modemSendExpectOk("AT+CACFG=\"KEEPALIVE\",1");
    if (!modem->ip_tcp_keepalive_enable) {
      modem->modemSendExpectOk("AT+CACFG=\"KEEPALIVE\",0");
    }
    else {
      snprintf(cmd, sizeof(cmd), "AT+CACFG=\"KEEPALIVE\",1,%d,%d,%d",
	       modem->ip_tcp_keepalive_idle,
	       modem->ip_tcp_keepalive_interval,
	       modem->ip_tcp_keepalive_count);
      modem->modemSendExpectOk(cmd);
    }
    
    
    snprintf(cmd, sizeof(cmd), "AT+CAOPEN=%d,0,\"TCP\",\"%s\",%d", slot, host, port);
    if (!modem->modemSendExpectIntPair(cmd, "+CAOPEN: ", &cid, &result, connect_timeout_ms, 2, HERE)) {
      // one cause of error might be a dangling connection, try closing and retry
      char cmd2[20];
      snprintf(cmd2, sizeof(cmd2), "AT+CACLOSE=%d", slot);
      if (modem->modemSendExpectOk(cmd2, HERE)) {
	LEAF_WARN("Successfully closed a dangling connection");
	if (modem->modemSendExpectIntPair(cmd, "+CAOPEN: ", &cid, &result, connect_timeout_ms, 2, HERE)) {
	  _connected = true;
	}
      }
      if (!_connected) {
	LEAF_ALERT("CAOPEN failed: %d", result);
	modem->modemReleasePortMutex(HERE);
	LEAF_INT_RETURN(0);
      }
    }
    LEAF_NOTICE("CAOPEN succeeded %d,%d", cid, result);
    if (cid != slot) {
      LEAF_ALERT("Wrong CID (%d) for connect result on slot %d", cid, slot);
    }
    modem->modemReleasePortMutex(HERE);
    
    modem->ipCommsState(ONLINE,HERE);
    _connected = true;
    LEAF_INT_RETURN(1);
  }
  
  virtual size_t write(const uint8_t *buf, size_t size)
  {
    char cmd[40];
    modem->ipCommsState(TRANSACTION, HERE);
    snprintf(cmd, sizeof(cmd), "AT+CASEND=%d,%d", slot, size);
    LEAF_NOTICE("write: %s", cmd);
    if (!modem->modemSendExpectPrompt(cmd, 2000, HERE)) {
      LEAF_ALERT("AT+CASEND failed");
      modem->ipCommsState(REVERT,HERE);
      disconnectIndication();
      return 0;
    }
    DumpHex(L_NOTICE, "write", buf, size);
    size_t result = modem->modemSendRaw(buf, size, HERE);

    cmd[0]='\0';
    if (!modem->modemSendExpect("", "OK", cmd, sizeof(cmd), -1, 1, HERE, false)) {
      LEAF_WARN("Did not get OK after +CASEND (got [%s])", cmd);
    }
    LEAF_NOTICE("write complete");
    modem->ipCommsState(REVERT,HERE);
    return result;
  }
  
  virtual void stop()
  {
    if (!modem->modemSendCmd(HERE, "AT+CACLOSE=%d", slot)) {
      LEAF_ALERT("CACLOSE failed");
    }
    _connected = false;
  }

  virtual void disconnectIndication() 
  {
    LEAF_NOTICE("Socket disconnected by peer");
    stop();
  }

  virtual void dataIndication(int count) 
  {
    // this is called from URC handler which will be holding the port mutex
    
    LEAF_ENTER_INT(L_NOTICE, count);
    char cmd[40];
    int room = rx_buffer->room();
    if (room == 0) {
      LEAF_WARN("RX buffer full");
      LEAF_VOID_RETURN;
    }

    // sim7080 chokes if buffer is over 1460
    if (room > 1460) room=1460;

    modem->ipCommsState(TRANSACTION, HERE);
    snprintf(cmd, sizeof(cmd), "AT+CARECV=%d,%d", slot, room);
    // Response will be +CARECV: <len>,<data>
    int len = 0;
    
    if (modem->modemSendExpectInlineInt(cmd, "+CARECV: ", &len, ',', -1, HERE)) {
      LEAF_NOTICE("length = %d", len);
      int got = modem->modemReadToBuffer(rx_buffer, len, HERE);
      if (got != len) {
	LEAF_WARN("CARECV short read");
      }
    }
    else {
      LEAF_WARN("CARECV error");
    }
    modem->ipCommsState(REVERT, HERE);
  }

};



// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:


