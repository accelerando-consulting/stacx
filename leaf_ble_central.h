#pragma once
#include "abstract_storage.h"
#include "BLEDevice.h"

static void dumpString(std::string s, char *buf, int buflen) 
{
  int i=0;
  for (char const &c: s) {
    snprintf(buf+i, buflen-i, "%02x", (int)c);
    i+=2;
    if (i >= buflen) {
      i = buflen-1;
      break;
    }
  }
  buf[i]='\0';
}


class BLECentralLeaf : public StorageLeaf, public Pollable
{
public:
  BLECentralLeaf(String name, String peripheral_name="", String service="", String characteristics="", String target="")
    : StorageLeaf(name)
    , Pollable(10000, 60)
  {
    leaf_type = "ble";
    this->target = target;
    this->peripheral_name = peripheral_name;
    this->service_uuid = service;
    this->characteristic_specifier = characteristics;
    this->userDescDescriptorUUID = BLEUUID((uint16_t)0x2901);
  }
  
  virtual void setup(void);
  virtual void loop(void);
  virtual bool poll(void);
  virtual void status_pub(void);
  virtual void mqtt_do_subscribe();
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false);

  void startScan(void);
	
  void onConnect(BLEClient *client);
  void onDisconnect(BLEClient *client);
  void onScanResult(BLEAdvertisedDevice advertisedDevice);
  bool connectToPeripheral(void);

protected:
  BLEUUID userDescDescriptorUUID; // this should be static but I CBF
				  // fighting the compiler, and the class is
				  // a singleton anyhow.  
  
  String target;
  String peripheral_name = ""; // either name or preference indirection
  String actual_peripheral_name = ""; // actual name (looked up from preferences)
  String service_uuid;
  String characteristic_specifier;
  int scan_interval_sec = 120;
  unsigned long last_scan = 0;
  
  BLEUUID service;
  bool doConnect = false;
  bool connected = false;
  bool doScan = false;
  SimpleMap<String,BLEUUID*> *characteristics; // name -> uuid 
  SimpleMap<String,String> *characteristic_names; // uuid-string -> name
  SimpleMap<String,BLERemoteCharacteristic*> *remote_characteristics; // name -> object 

  BLEScan* scan;
  BLEAdvertisedDevice *device;
  BLEClient *client;

  String readValue(String name);
};


static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  const char *uuid = pBLERemoteCharacteristic->getUUID().toString().c_str();
  NOTICE("Notify callback for characteristic %s len=%d", uuid, length);
}


class BLECentralLeafClientCallbacks : public BLEClientCallbacks {

  BLECentralLeaf *leaf;
public:
  BLECentralLeafClientCallbacks(BLECentralLeaf *leaf) : BLEClientCallbacks()
  {
    this->leaf = leaf;
  }
  
  void onConnect(BLEClient* client) {
    NOTICE("BLE connect");
    leaf->onConnect(client);
  }

  void onDisconnect(BLEClient* client) {
    //ALERT("BLE peripheral disconnect");
    leaf->onDisconnect(client);
  }
};


class BLECentralLeafAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  BLECentralLeaf *leaf;
public:
  BLECentralLeafAdvertisedDeviceCallbacks(BLECentralLeaf *leaf) : BLEAdvertisedDeviceCallbacks() 
  {
    this->leaf = leaf;
  }
  
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    leaf->onScanResult(advertisedDevice);
  }
};

void BLECentralLeaf::onScanResult(BLEAdvertisedDevice advertisedDevice) {
  LEAF_ENTER(L_INFO);

  const char *dev = advertisedDevice.toString().c_str();
  LEAF_INFO("BLE Advertised Device found: %s", dev);

  const char *advertisedName = advertisedDevice.getName().c_str();
  const char *wantName = this->actual_peripheral_name.c_str();
  
  bool name_match =
    (this->actual_peripheral_name == "*") ||  // * means any name 
    (wantName[0] && (strcasecmp(wantName, advertisedName) == 0));

  if (!name_match) {
    //LEAF_INFO("Booo!  Not interested in this device");
    LEAF_LEAVE;
    return;
  }

  //LEAF_INFO("Yay, %s is the droid we are looking for", advertisedName);
  
  // We have found an interesting device, check if it has our service
  if (service_uuid.length()>0 &&
      advertisedDevice.haveServiceUUID() &&
      advertisedDevice.isAdvertisingService(service)) {
    LEAF_NOTICE("Attempting connection to peripheral %s", dev);
    //scan->stop();
    device = new BLEAdvertisedDevice(advertisedDevice);
    doConnect = true;
    doScan = true;
  } else {
    LEAF_NOTICE("Waitwhat, but it does nto have the service we want");
  }
  

  LEAF_LEAVE;
}

void BLECentralLeaf::onConnect(BLEClient *client)
{
  LEAF_ENTER(L_NOTICE);
  connected = true;
  LEAF_LEAVE;
}

void BLECentralLeaf::onDisconnect(BLEClient *client)
{
  //LEAF_ENTER(L_NOTICE);
  connected = false;
  //LEAF_LEAVE;
}

String BLECentralLeaf::readValue(String name)
{
  LEAF_ENTER(L_DEBUG);
  LEAF_NOTICE("readValue %s", name.c_str());
  char hexbuf[80] = "";
  String value = "";
  BLERemoteCharacteristic *characteristic = remote_characteristics->get(name);
  if (characteristic) {
    std::string rawValue = characteristic->readValue();
    LEAF_NOTICE("Got raw value of size %d", rawValue.size());
    dumpString(rawValue, hexbuf, sizeof(hexbuf));
    value = String(hexbuf);
    values->put(name, value);
  }
  else {
    LEAF_ALERT("Read faile for %s", name.c_str());
  }
  
  LEAF_LEAVE;
  return value;
}



bool BLECentralLeaf::connectToPeripheral() {
  LEAF_ENTER(L_NOTICE);
  
  const char *addr = device->getAddress().toString().c_str();
  const char *name = device->getName().c_str();
  LEAF_NOTICE("Connecting to peripheral %s %s", addr, name);
    
  this->client  = BLEDevice::createClient();
  client->setClientCallbacks(new BLECentralLeafClientCallbacks(this));

  // Connect to the remove BLE Server.
  this->client->connect(device);
  LEAF_NOTICE("Connected");

  LEAF_NOTICE("Clear chacateristic cache");
  this->remote_characteristics->clear();

  LEAF_NOTICE("Get service of interest");
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = this->client->getService(service);
  if (pRemoteService == nullptr) {
    //const char *uuid = service->toString().c_str();
    LEAF_ALERT("Failed to find our service");
    this->client->disconnect();
    LEAF_LEAVE;
    return false;
  }
  LEAF_NOTICE("Service found"/*, pRemoteService->toString().c_str()*/);

  for (int i=0; i<characteristic_names->size(); i++) {
    String k = characteristic_names->getKey(i);
    String v = characteristic_names->getData(i);
    LEAF_NOTICE("   wanted characteristic [%s] => [%s]", k.c_str(), v.c_str());
  }

  LEAF_NOTICE("Get characteristics");
  auto cmap = pRemoteService->getCharacteristics();
  for (auto it = cmap->begin(); it != cmap->end(); ++it) {
    BLERemoteCharacteristic *characteristic = it->second;
    String uuid = String(characteristic->getUUID().toString().c_str()); // ugh
    LEAF_NOTICE("Service has characteristic: with UUID %s", uuid.c_str());
    LEAF_NOTICE("    details %s", characteristic->toString().c_str());

#if 0
    BLERemoteDescriptor *userDesc = characteristic->getDescriptor(BLECentralLeaf::userDescDescriptorUUID);
    if (userDesc != NULL) {
      LEAF_NOTICE("    Characteristic has a userDesc descriptor");
      LEAF_NOTICE("    descriptor lookth like thith: %s", userDesc->toString().c_str());
      std::string ud = userDesc->readValue();
      LEAF_NOTICE("    Read the descriptor");
      LEAF_NOTICE("    description %s", ud.c_str());
    }
#endif
    
    if (characteristic_names->has(uuid)) {
      String name = characteristic_names->get(uuid);
      LEAF_NOTICE("Characteristic %s is named %s", uuid.c_str(), name.c_str());
      remote_characteristics->put(name, characteristic);

      if(characteristic->canRead()) {
	String value = this->readValue(name);
	LEAF_NOTICE("Characteristic value is %s", value.c_str());
      }

#if 0
      if(characteristic->canNotify()) {
	pRemoteCharacteristic->registerForNotify(notifyCallback);
      }
#endif
    }
    else {
      LEAF_NOTICE("    this characteristic is not of interest");
    }
  }
  
  

  
  connected = true;
  LEAF_LEAVE;
  return true;
}


void BLECentralLeaf::startScan(void) 
{
  LEAF_ENTER(L_NOTICE);
  
  scan->setAdvertisedDeviceCallbacks(new BLECentralLeafAdvertisedDeviceCallbacks(this));
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  scan->start(5, false);

  LEAF_LEAVE;
}

  
void BLECentralLeaf::setup(void) {
  LEAF_ENTER(L_INFO);
  Leaf::setup();
  this->install_taps(target);

  if (this->peripheral_name.length() > 0) {

    if (this->peripheral_name.startsWith("@")) {
      // Names starting with @ are preference indirection
      StorageLeaf *prefs = (StorageLeaf *)get_tap("prefs");
      this->actual_peripheral_name = prefs->get(this->peripheral_name.substring(1));
    }
    else {
      // ordinary case, no preference indirection
      this->actual_peripheral_name = peripheral_name;
    }

    if (this->actual_peripheral_name.length() > 0) {
      LEAF_INFO("Looking for BLE peripheral named \"%s\"", this->actual_peripheral_name.c_str());
    }
    else {
      //LEAF_INFO("No particular BLE peripheral to seek");
    }
  }

  if (service_uuid.length() > 0) {
    LEAF_INFO("Service of interest \"%s\"", this->service_uuid.c_str());
    this->service = BLEUUID(service_uuid.c_str());
  }

  this->characteristics = new SimpleMap<String,BLEUUID*>(_compareStringKeys);
  this->characteristic_names = new SimpleMap<String,String>(_compareStringKeys);
  this->remote_characteristics = new SimpleMap<String,BLERemoteCharacteristic*>(_compareStringKeys);

  if (characteristic_specifier.length() > 0) {
    //LEAF_DEBUG("Parsing characteristic specifier %s", characteristic_specifier.c_str());
    String c = characteristic_specifier;
    int pos ;
    do {
      String char_uuid;
      String char_alias;

      if ((pos = c.indexOf(',')) > 0) {
	char_uuid = c.substring(0, pos);
	c.remove(0,pos+1);
      }
      else {
	char_uuid = c;
	c="";
      }
      //LEAF_DEBUG("Parsing characteristic instance %s", char_uuid.c_str());

      if ((pos = char_uuid.indexOf('=')) > 0) {
	char_alias = char_uuid.substring(pos+1);
	char_uuid.remove(pos);
      }
      else {
	char_alias = char_uuid;
      }
      LEAF_INFO("Characteristic of interest %s => %s", char_alias.c_str(), char_uuid.c_str());

      this->characteristics->put(char_alias, new BLEUUID(char_uuid.c_str()));
      this->characteristic_names->put(char_uuid, char_alias);
	
    } while (c.length() > 0);
  }

  LEAF_NOTICE("Initialising Bluetooth");
  BLEDevice::init(device_id);
  scan = BLEDevice::getScan(); // get singleton
  
  doScan = true;
  
  LEAF_LEAVE;
}


bool BLECentralLeaf::poll(void) {
  LEAF_ENTER(L_NOTICE);
  // precondition: only call when connected
  bool result = false;

  if (connected) {
    for (int i=0; i<remote_characteristics->size(); i++) {
      String name = remote_characteristics->getKey(i);
      LEAF_NOTICE("Poll characteristic [%s]", name.c_str());
      String oldValue = values->get(name);
      String newValue = readValue(name);
      if (oldValue != newValue) {
	LEAF_NOTICE("    New value for [%s] <= [%s]", name.c_str(), newValue.c_str());
	result = true;
      }
      else {
	LEAF_NOTICE("    no change for [%s]", name.c_str());
      }
      
      
    }
  }

  LEAF_LEAVE;
  return result;
}

void BLECentralLeaf::status_pub(void) {
  LEAF_ENTER(L_NOTICE);
  
  for (int i=0; i<values->size(); i++) {
    String c = values->getKey(i);
    LEAF_NOTICE("Publish characteristic [%s]", c.c_str());
    mqtt_publish("status/"+leaf_name+"/"+c, values->getData(i), false);
  }

  LEAF_LEAVE;
}

void BLECentralLeaf::loop(void) {
  static bool wasConnected = false;
  
  Leaf::loop();

  if (!wasConnected && connected) {
    LEAF_ALERT("Now connected to bluetooth peripheral");
    wasConnected = connected;
  }
  else if (wasConnected && !connected) {
    LEAF_ALERT("We have been disconnected from bluetooth peripheral");
    wasConnected = connected;
  }

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToPeripheral()) {
      LEAF_NOTICE("Connection established.");
    } else {
      LEAF_NOTICE("Connection failed.");
    }
    doConnect = false;
  }

  // If we are connected to a BLE Peripheral, poll the characteristics periodically
  if (connected) {
    pollable_loop();
  } else if(doScan) {
    // Lost connection to device, go back and re-scan
    unsigned long now = millis();
    if (now > (last_scan + (scan_interval_sec * 1000))) {
      startScan();
      last_scan = now;
    }
  }
}

void BLECentralLeaf::mqtt_do_subscribe() 
{
  Leaf::mqtt_do_subscribe();
  registerCommand(HERE,"scan", "Initate a bluetooth scan");
}


bool BLECentralLeaf::mqtt_receive(String type, String name, String topic, String payload, bool direct) {
  LEAF_ENTER(L_INFO);
  bool handled = false;

  WHEN("cmd/scan",{startScan();})
#if 0
    ELSEWHEN("set/other",{set_other(payload)});
#endif
  else {
    handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
  }

  LEAF_LEAVE;
  return handled;
}
    



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:

