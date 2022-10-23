#include "abstract_storage.h"
#include <FS.h>
#include <LittleFS.h>

class FSPreferencesLeaf : public StorageLeaf
{
public:
  FSPreferencesLeaf(String name, String defaults="", String filename="", bool auto_save=true) : StorageLeaf(name, defaults) {
    if (filename.length()) {
      this->prefs_file = filename;
    }
    this->impersonate_backplane = true;
    this->auto_save = auto_save;
  }
    
  virtual void setup();
  virtual void load();
  virtual void save(bool force_format=false);
  virtual void put(String name, String value, bool no_save=false);
  virtual void mqtt_do_subscribe();
  virtual bool mqtt_receive(String type, String name, String topic, String payload);
  virtual bool wants_topic(String type, String name, String topic);

protected:
  String prefs_file = "/prefs.json";
  bool auto_save = false;
};

static void listDir(const char * dirname) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = LittleFS.open(dirname);

  File file = root.openNextFile();
  while (file) {
    Serial.print("    ");
    Serial.print(file.name());
    Serial.print(" ");
    if (file.isDirectory()) {
      Serial.println("<DIR>");
    }
    else {
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}


void FSPreferencesLeaf::setup() 
{
  // note: slightly weird order of superclass setup in this module, in order to get the
  // filesystem initialised before trying to load saved preferences
  Leaf::setup();
  LEAF_ENTER(L_NOTICE);
  if (!LittleFS.begin()) {
    LEAF_ALERT("NO LittleFS.  Formatting");
    this->save(true);
    LEAF_ALERT("Rebooting after format");
    delay(3000);
    reboot();
  }
  LEAF_NOTICE("LittleFS listing root directory");
  listDir("/");
  LEAF_NOTICE("LittleFS setup done");

  File file = LittleFS.open(prefs_file.c_str());
  if (file) {
    LEAF_NOTICE("Configuration File:");
    while(file.available()){
      Serial.write(file.read());
    }
    Serial.println();
    file.close();
  }

  StorageLeaf::setup();

  // Load a configured device ID if present.  This relies on the prefs leaf being the first leaf.
  String new_device_id = this->get("device_id", device_id);
  if (new_device_id != device_id) {
    strncpy(device_id, new_device_id.c_str(), sizeof(device_id)); 
   LEAF_NOTICE("Loaded device_id [%s] from NVS", device_id);
    // redo the topic setup for this leaf
    Leaf::setup();
  }

  getIntPref("heartbeat_interval_sec", &::heartbeat_interval_seconds, "Period after which to publish a proof-of-life message");
  heartbeat_interval_seconds = ::heartbeat_interval_seconds;

  // Check for preferences of the form inhibit_NAME which temporarily inhibit a leaf
  for (int i=0; leaves[i]; i++) {
    Leaf *l = leaves[i];
    String leaf_pref = String("leaf_inhibit_")+l->get_name();
    if (getBoolPref(leaf_pref, false)) {

      LEAF_WARN("Leaf %s is inhibited by preference %s", l->describe().c_str(), leaf_pref.c_str());
      leaves[i]->inhibit();
    }
  }
  
  LEAF_LEAVE;
}

bool FSPreferencesLeaf::wants_topic(String type, String name, String topic) 
{
  //LEAF_NOTICE("topic=%s", topic.c_str());
  if (StorageLeaf::wants_topic(type, name, topic)) return true;
  
  if (topic=="cmd/format") return true;
  if (topic=="cmd/ls") return true;
  if (topic=="cmd/rm") return true;
  if (topic=="cmd/cat") return true;
  if (topic=="cmd/store") return true;
  return false;
}

void FSPreferencesLeaf::load() 
{
  LEAF_ENTER(L_NOTICE);
  if (!LittleFS.begin()) {
    LEAF_ALERT("NO LittleFS.  Formatting");
    this->save(true);
    LEAF_ALERT("Rebooting after format");
    delay(3000);
    reboot();
  }

  LEAF_NOTICE("mounted file system");
  if (!LittleFS.exists(prefs_file.c_str())) {
    LEAF_ALERT("No configuration file found");
    save(false);
  }

  //file exists, reading and loading
  LEAF_NOTICE("reading config file");
  File configFile = LittleFS.open(prefs_file.c_str(), "r");
  if (!configFile) {
    LEAF_ALERT("Cannot read config file");
    LEAF_VOID_RETURN;
  }

  size_t size = configFile.size();
  LEAF_NOTICE("Parsing config file, size=%d", size);

  DynamicJsonDocument doc(size*2);
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    LEAF_ALERT("Failed to parse config file: %s", error.c_str());
    configFile.close();
    LEAF_VOID_RETURN;
  }

  JsonObject root = doc.as<JsonObject>();

  for (JsonPair kv : root) {
    const char *key = kv.key().c_str();
    const char *value = kv.value().as<const char*>();;
    if (!key || !value) {
      LEAF_WARN("Bogus JSON item");
      continue;
    }
    if (strlen(key)==0) continue;
    LEAF_INFO("Load config value [%s] <= [%s]", key, value);
    this->put(String(key), String(value), true);
  }

  configFile.close();
  LEAF_VOID_RETURN;
}

void FSPreferencesLeaf::save(bool force_format)
{
  LEAF_NOTICE("%ssaving config to FS file %s", force_format?"REFORMATTING, then ":"",prefs_file.c_str());

  if (force_format) {
    LEAF_ALERT("Reformatting FS");
    if (LittleFS.format()) {
      LEAF_NOTICE("Reformatted OK");
    }
    else {
      LEAF_ALERT("FS Format failed");
      return;
    }

    if (!LittleFS.begin()) {
      LEAF_ALERT("failed to mount FS");
      return;
    }
  }

#if 0
  String bak = prefs_file + ".bak";
  if (!LittleFS.rename(prefs_file.c_str(),bak.c_str())) {
    LEAF_ALERT("Unable to create backup config file");
    return;
  }
#endif

  File configFile = LittleFS.open(prefs_file.c_str(), "w");
  if (!configFile) {
    LEAF_ALERT("Unable to open config file for writing");
    return;
  }

  //StaticJsonDocument<1024> doc;
  DynamicJsonDocument doc(2048);
  //JsonObject root = doc.to<JsonObject>();

  for (int i=0; i < values->size(); i++) {
    String key = values->getKey(i);
    String value = values->getData(i);
    LEAF_INFO("Save config value [%s] <= [%s]", key.c_str(), value.c_str());
    //root[key.c_str()] = value.c_str();
    doc[key] = value;
  }

  //serializeJsonPretty(doc, Serial);  Serial.println();

  if (serializeJsonPretty(doc, configFile) == 0) {
    LEAF_ALERT("Failed to serialise configuration");
  }
  configFile.close();
}

void FSPreferencesLeaf::put(String name, String value, bool no_save) {
  size_t p_size;

  LEAF_ENTER(L_DEBUG);

  if (values->has(name)) {
    String current = values->get(name);
    if (value == current) {
      LEAF_LEAVE;
      return;
    }
  }

  LEAF_INFO("prefs:put %s <= [%s]", name.c_str(), value.c_str());
  values->put(name, value);
  if (!no_save && this->auto_save) {
    this->save();
  }
  
  LEAF_LEAVE;
}

void FSPreferencesLeaf::mqtt_do_subscribe() 
{
  StorageLeaf::mqtt_do_subscribe();
  register_mqtt_cmd("format", "format the nonvolatile storage");
  register_mqtt_cmd("ls", "list a directory");
  register_mqtt_cmd("rm", "remove a file");
  register_mqtt_cmd("cat", "print a file");
  register_mqtt_cmd("store", "read lines from console and store to a file (Console CLI only)");
}



bool FSPreferencesLeaf::mqtt_receive(String type, String name, String topic, String payload)
{
  bool handled = false;
  
    LEAF_ENTER(L_DEBUG);
    LEAF_DEBUG("fs_preferences mqtt_receive %s %s => %s", type.c_str(), name.c_str(), topic.c_str());

    WHEN("cmd/format", {
	LEAF_NOTICE("littlefs format");
	LittleFS.format();
	LEAF_NOTICE("littlefs format done");
	if (!LittleFS.begin()) {
	  LEAF_ALERT("LittleFS mount failed");
	}
    })
    ELSEWHEN("cmd/ls", {
	listDir(payload.c_str());
      })
    ELSEWHEN("cmd/rm", {
	LittleFS.remove(payload.c_str());
      })
    ELSEWHEN("cmd/cat", {
      File file = LittleFS.open(payload.c_str(), "r");
      if(!file) {
	LEAF_ALERT("Preferences file not readable");
        return handled;
      }
      LEAF_NOTICE("Listing preferences file %s", prefs_file.c_str());
      while(file.available()){
        Serial.write(file.read());
      }
      Serial.println();
      file.close();
    })
    ELSEWHEN("cmd/store",{
      File file = LittleFS.open(payload.c_str(), "w");
      if(!file) {
	LEAF_ALERT("File not writable");
        return handled;
      }
      // Read from "stdin" (console)
      Serial.println("> (send CRLF.CRLF to finish)");
      while (1) {
	String line = Serial.readStringUntil('\n');
	if (line.startsWith(".\r") || line.startsWith(".\n")) {
	  break;
	}
	Serial.println(line);
	file.println(line);
      }
      file.close();
    })
    else {
      handled = StorageLeaf::mqtt_receive(type, name, topic, payload);
    }
    return handled;
}



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
