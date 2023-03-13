#pragma once
//
//@**************************** class WireFollowerLeaf *****************************
//
// Act as an I2C follower (aka sl*v*) device using the ESP-Arduino 2.x
// callback interface (see leaf_wirefollower_lowlevel for an older interface
// that works in ESP-Arduino 1.x)
//
#include "Wire.h"
#include <rBase64.h>

void wireFollowerReceive(int len);
void wireFollowerRequest();
void setWireFollowerSingleton(Leaf *singleton);

class WireFollowerLeaf : public Leaf
{
protected:
  String target;
  TwoWire *i2c_bus;
  int pin_sda;
  int pin_scl;
  byte address;
  size_t buffer_size = 256;
  unsigned long last_receive = 0;
  unsigned long receive_timeout_ms = 50;
  unsigned long dpd_timeout = 5*60*1000;
  size_t message_size = 0;

  uint8_t *tmp_buffer=NULL;
  size_t tmp_size = 0;

  uint8_t *inbound_buffer=NULL;
  size_t inbound_size = 0;

  uint8_t *outbound_buffer=NULL;
  size_t outbound_size = 0;

public:
  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name, pins)
  //
  WireFollowerLeaf(String name, String target, int sda, int scl, int address=0x10, int message_size=0, const char magic[3]="I2C", TwoWire *wire=NULL)
    : Leaf("wirefollower", name, NO_PINS)
    , Debuggable(name)
  {
    this->target = target;
    this->i2c_bus = wire?wire:&Wire;
    this->pin_sda = sda;
    this->pin_scl = scl;

    inbound_buffer = (uint8_t *)calloc(1,this->buffer_size);
    tmp_buffer = (uint8_t *)calloc(1,this->buffer_size);
    outbound_buffer = (uint8_t *)calloc(1,this->buffer_size);
    memset(tmp_buffer, 0, this->buffer_size);

    // Set up for an empty outbound message initially
    outbound_buffer[0] = message_size;
    outbound_buffer[1] = magic[0];
    outbound_buffer[2] = magic[2];
    outbound_buffer[3] = magic[3];
    outbound_size = message_size;

    this->message_size = message_size;
    this->address = address;
    do_heartbeat=false;
  }

  ~WireFollowerLeaf()
  {
    if (tmp_buffer) free(tmp_buffer);
    if (inbound_buffer) free(inbound_buffer);
    if (outbound_buffer) free(outbound_buffer);
  }

  void onReceive(int avail)
  {
    //LEAF_DEBUG("onReceive len=%d", avail);

    int len;
    unsigned long now,elapsed;

    now = millis();

    // If we have received a partial message, but then no subsequent message
    // for a while, clear the receive buffer (i.e. presume that we missed
    // the remainder of a message)
    if (tmp_size > 0) {
      uint8_t len = tmp_buffer[0];
      if (tmp_size < len) {
	unsigned long cutoff = last_receive + receive_timeout_ms;
	if (now > cutoff) {
	  LEAF_ALERT("Clearing receive buffer of partial message (%d) after timeout", (int)tmp_size);
	  tmp_size = 0;
	}
      }
    }

    int space_avail = sizeof(tmp_buffer)-tmp_size;
    if (avail > avail) {
      LEAF_ALERT("Receive buffer overflow (has %d, need %d)", tmp_size, avail);
      avail = space_avail;
    }

    int rcvd = Wire.readBytes(tmp_buffer+tmp_size, avail);

    //
    // It is possible to receive only part of a message from the master, so
    // it might take several callbacks to accumulate a full buffer.
    //
    elapsed = now - last_receive;
    last_receive = now;
    tmp_size += rcvd;
    uint8_t message_size = tmp_buffer[0];

    if (tmp_size < message_size) {
      LEAF_INFO("Partial message of %d bytes received (elapsed %lu), bringing total to %d.  Wait for more.",(int)rcvd, elapsed, (int)tmp_size);
      //DumpHex(L_DEBUG, "RCVD PART", tmp_buffer, tmp_size);
      return;
    }

  }

  void onRequest()
  {
    //LEAF_DEBUG("onRequest, have %d bytes in outbound buffer", (int)outbound_size);
    if (outbound_size) {
      size_t sent = i2c_bus->write(outbound_buffer, outbound_size);
      if (sent == outbound_size) {
	LEAF_INFO("Sent entire outbound buffer of %d bytes", (int)outbound_size);
	outbound_size = 0;
      }
      else if (sent > 0) {
	LEAF_INFO("Sent %d/%d bytes from outbound buffer", (int)sent, (int)outbound_size);
	memmove(outbound_buffer, outbound_buffer+sent, outbound_size-sent);
	outbound_size -= sent;
      }
      else {
	LEAF_DEBUG("Nothing happens");
      }
    }
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_DEBUG);
    this->install_taps(target);

    setWireFollowerSingleton(this) ;
    if ((pin_sda >= 0) || (pin_scl >=0)) {
      i2c_bus->setPins(pin_sda, pin_scl);
    }
    i2c_bus->onReceive(wireFollowerReceive);
    i2c_bus->onReceive(wireFollowerReceive);
    i2c_bus->onRequest(wireFollowerRequest);
    i2c_bus->begin((uint8_t)address);

    LEAF_NOTICE("%s claims pins SCL=%d, SDA=%d as I2C slave device 0x%02X", describe().c_str(),
		pin_scl, pin_sda, address);

    LEAF_LEAVE;
  }

  //
  // Arduino loop function
  // (Superclass function will take care of heartbeats)
  //
  void loop(void) {
    Leaf::loop();

    // Process any complete messages that the receive callback has collated
    // into the input buffer
    int msg_buffer_size = buffer_size * 4 / 3 + 10;
    char msg_buffer[msg_buffer_size];

    bool processed_message = false;
    while ((message_size > 0) && (tmp_size >= message_size)) {
      //LEAF_INFO("loop: processing received messages");

      processed_message = true;

      if ((message_size == inbound_size) &&
	  (tmp_size >= inbound_size) &&
	  (memcmp(inbound_buffer, tmp_buffer, message_size) == 0)) {

	LEAF_DEBUG("Ignore duplicate message (size=%d)", message_size);
	// We have at least one complete message, but its a duplicate.
	//
	// We only treat the received message as NEW if it differs from the last
	// (use a message ID counter if that is a problem for your application)

	if (tmp_size == inbound_size) {
	  //LEAF_DEBUG("Dropped entire inbound buffer");
	  tmp_size = 0; // consider the buffer empty, as we discarded its entire content
	  message_size = 0;
	}
	else {
	  // drop one message from the inbound buffer, shift the buffer content
	  memmove(tmp_buffer, tmp_buffer + message_size, tmp_size - message_size);
	  tmp_size -= message_size;
	  message_size = tmp_buffer[0];
	  //LEAF_DEBUG("Shifted inbound buffer, now have %d left (new message_size=%d)", tmp_size, message_size);
	}
	continue;
      }

      if (tmp_size == message_size) {
	//
	// We have received exactly one complete message
	//
	//
	LEAF_DEBUG("Received %d bytes from I2C leader", (int)tmp_size);
	DumpHex(L_DEBUG, "RCVD", tmp_buffer, tmp_size);

	memcpy(inbound_buffer, tmp_buffer, tmp_size);
	inbound_size = tmp_size;
	tmp_size = 0;
      }
      else {
	// we have more than one complete message, process the first one only on
	// this loop (we'll continue to process all complete messages before
	// leaving this callback)
	if (tmp_size >= buffer_size) {
	  LEAF_NOTICE("Overflow in RX buffer (message_size=%d buffer_size=%d), discard buffer", message_size, tmp_size);
	  tmp_size = 0;
	  continue;
	}
	else {
	  LEAF_NOTICE("Multiple messages in RX buffer (message_size=%d buffer_size=%d), process one message", message_size, tmp_size);

	  memcpy(inbound_buffer, tmp_buffer, message_size);
	  inbound_size = message_size;
	  memmove(tmp_buffer, tmp_buffer + message_size, tmp_size - message_size);
	  tmp_size -= message_size;
	  message_size = tmp_buffer[0];
	  //LEAF_DEBUG("After shifting input buffer, next message size=%d, buffer_size=%d", message_size, tmp_size);
	}
      }

      //
      // We now have exactly one complete new message in inbound_buf
      //
      LEAF_INFO("Received %d bytes from I2C leader", (int)inbound_size);
      //DumpHex(L_DEBUG, "RCVD", inbound_buffer, inbound_size);
      int len = rbase64_encode(msg_buffer, (char *)inbound_buffer, inbound_size);
      msg_buffer[len] = '\0';
      publish("event/i2c_follower_receive", String(msg_buffer));
    }
    if (processed_message) {
      LEAF_INFO("No more complete messages in input buffer (%d)", tmp_size);
    }

    if (last_receive && (millis() > (last_receive + dpd_timeout))) {
      LEAF_ALERT("No messages from I2C master for a while.   Is ESP-IDF broken again?");
      mqtt_publish("alert", "i2c_dpd");
      Leaf::reboot("wirefollower_dpd");
    }
		 
  }

  virtual void pre_sleep(int duration=0)
  {
    LEAF_ENTER(L_NOTICE);
#ifdef BREAK_CAMERA
    i2c_driver_delete(bus);
#endif
    LEAF_LEAVE;
  }

  //
  // MQTT message callback
  // (Use the superclass callback to ignore messages not addressed to this leaf)
  //
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;
    size_t sent;
    //LEAF_NOTICE("MR %s %s", topic.c_str(), payload.c_str());

    WHEN("cmd/i2c_follower_action",{
	// This is a short 32-bit application-agnostic message that
	// can mean whatever the sender wants.
	// Use i2c_follower_write for more complex messages
	uint16_t action = payload.toInt();
	char buf[8];
	outbound_size = 8;
	buf[0] = 2; // first byte is length.  Length <= 4 are system msgs
	buf[1] = 0; // second byte is type = 0 means action word
	buf[2] = action&0xFF; // bytes 3+4 are the action word
	buf[2] = action>>8;
	outbound_size = 4;

	LEAF_NOTICE("Commanded to send ACTION payload %s", payload.c_str());
      })
    ELSEWHEN("cmd/i2c_follower_write",{

	//LEAF_DEBUG("Commanded to send %s", payload.c_str());
	if (payload.length() > (buffer_size * 4 / 3 + 2)) {
	  LEAF_ALERT("i2c_follower_write payload too long");
	}
	else {
	  outbound_size = rbase64_decode((char *)outbound_buffer, (char *)payload.c_str(), payload.length());
	  LEAF_NOTICE("Queueing %d bytes for I2C leader", outbound_size);
	  DumpHex(L_NOTICE, "MESSAGE", outbound_buffer, outbound_size);
	  if ((sent = i2c_bus->slaveWrite(outbound_buffer, outbound_size)) != outbound_size) {
	    LEAF_ALERT("i2c_follower_write unable to queue output, wrote %d of %d", (int)sent, (int)outbound_size);
	  }
	  else {
	    //LEAF_DEBUG("Queued %d bytes successfully", sent);
	  }
	}
      })
      ELSEWHEN("cmd/i2c_follower_flush",{
	  int got = 0;
	  tmp_size = 0;
	})
    else {
      handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
    }

    return handled;
  }
};

WireFollowerLeaf *wireFollowerSingleton=NULL;

void setWireFollowerSingleton(Leaf *singleton)
{
  wireFollowerSingleton = (WireFollowerLeaf *)singleton;
}

void wireFollowerReceive(int avail)
{
  //DEBUG("wireFollowerReceive %d", avail);

  if (wireFollowerSingleton) wireFollowerSingleton->onReceive(avail);
}

void wireFollowerRequest()
{
  //DEBUG("wireFollowerRequest");
  if (wireFollowerSingleton) wireFollowerSingleton->onRequest();
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
