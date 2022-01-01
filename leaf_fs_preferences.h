#include "abstract_storage.h"
#include <FS.h>
#include <LittleFS.h>

class FSPreferencesLeaf : public StorageLeaf
{
public:
  FSPreferencesLeaf(String name, String defaults="", String filename="") : StorageLeaf(name, defaults) {
    if (filename.length()) {
      this->prefs_file = filename;
    }
    this->impersonate_backplane = true;
  }
    
  virtual void setup();
  virtual void load();
  virtual void save(bool force_format=false);
  virtual void put(String name, String value);
  virtual bool mqtt_receive(String type, String name, String topic, String payload);

protected:
  String prefs_file = "/prefs.json";
  bool autosave = false;
};

static void listDir(const char * dirname) {
  Serial.printf("Listing directory: %s\n", dirname);

  Dir root = LittleFS.openDir(dirname);

  while (root.next()) {
    File file = root.openFile("r");
    Serial.print("    ");
    Serial.print(root.fileName());
    Serial.print(" ");
    Serial.print(file.size());
    time_t cr = file.getCreationTime();
    time_t lw = file.getLastWrite();
    file.close();
    struct tm * tmstruct = localtime(&cr);
    Serial.printf(" %d-%02d-%02d %02d:%02d:%02d",
		  (tmstruct->tm_year) + 1900,
		  (tmstruct->tm_mon) + 1,
		  tmstruct->tm_mday,
		  tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
    tmstruct = localtime(&lw);
    Serial.printf(" %d-%02d-%02d %02d:%02d:%02d\n",
		  (tmstruct->tm_year) + 1900,
		  (tmstruct->tm_mon) + 1,
		  tmstruct->tm_mday,
		  tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
  }
}


void FSPreferencesLeaf::setup() 
{
  if (!LittleFS.begin()) {
    LEAF_ALERT("NO LittleFS.  Formatting");
    this->save(true);
    LEAF_ALERT("Rebooting after format");
    delay(3000);
    reboot();
  }
  StorageLeaf::setup();
  listDir("/");
  LEAF_INFO("LittleFS setup done");
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
    if (strlen(key)==0) continue;
    LEAF_NOTICE("Load config value [%s] <= [%s]", key, value);
    this->put(String(key), String(value));
  }

  configFile.close();
  LEAF_VOID_RETURN;
}

void FSPreferencesLeaf::save(bool force_format)
{
  LEAF_ALERT("%ssaving config to FS file %s", force_format?"REFORMATTING, then ":"",prefs_file.c_str());

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

  StaticJsonDocument<256> doc;
  //DynamicJsonDocument doc(1024);
  //JsonObject root = doc.to<JsonObject>();

  for (int i=0; i < values->size(); i++) {
    String key = values->getKey(i);
    String value = values->getData(i);
    LEAF_NOTICE("Save config value [%s] <= [%s]", key.c_str(), value.c_str());
    //root[key.c_str()] = value.c_str();
    doc[key] = value;
  }
  
  serializeJson(doc, Serial);

  if (serializeJson(doc, configFile) == 0) {
    LEAF_ALERT("Failed to serialise configuration");
  }
  configFile.close();
}

void FSPreferencesLeaf::put(String name, String value) {
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
  if (this->autosave) {
    this->save();
  }
  
  LEAF_LEAVE;
}


bool FSPreferencesLeaf::mqtt_receive(String type, String name, String topic, String payload) {
    LEAF_ENTER(L_DEBUG);
    bool handled = StorageLeaf::mqtt_receive(type, name, topic, payload);

    LEAF_DEBUG("fs_preferences mqtt_receive %s %s => %s", type.c_str(), name.c_str(), topic.c_str());

    WHEN("cmd/format", {
	LEAF_NOTICE("littlefs format");
	LittleFS.format();
	LEAF_NOTICE("littlefs format done");
	if (!LittleFS.begin()) {
	  LEAF_ALERT("LittleFS mount failed");
	}
    })
    WHEN("cmd/ls", {
	listDir(payload.c_str());
      })
    WHEN("cmd/rm", {
	LittleFS.remove(payload.c_str());
      })
    WHEN("cmd/cat", {
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
    });
    return handled;
}



// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
