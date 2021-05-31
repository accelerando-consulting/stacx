#include "leaf_storage_abstract.h"

class SPIFFSPreferencesLeaf : public StorageLeaf
{
public:
  SPIFFSPreferencesLeaf(String name, String defaults="", String filename="") : StorageLeaf(name, defaults) {
    if (filename.length()) {
      this->prefs_file = filename;
    }
  }
    
  virtual void load();
  virtual void save(bool force_format=false);
  virtual void put(String name, String value);

protected:
  String prefs_file = "prefs.json";
  bool autosave = false;
};

void SPIFFSPreferencesLeaf::load() 
{
  if (!SPIFFS.begin()) {
    LEAF_ALERT("NO SPIFFS.  Formatting");
    this->save(true);
    LEAF_ALERT("Rebooting after format");
    delay(3000);
    reboot();
  }

  LEAF_NOTICE("mounted file system");
  if (!SPIFFS.exists(prefs_file.c_str())) {
    LEAF_ALERT("No configuration file found");
    save(false);
  }

  //file exists, reading and loading
  LEAF_NOTICE("reading config file");
  File configFile = SPIFFS.open(prefs_file.c_str(), "r");
  if (!configFile) {
    LEAF_ALERT("Cannot read config file");
    return;
  }

  size_t size = configFile.size();
  LEAF_NOTICE("Parsing config file, size=%d", size);

  DynamicJsonDocument doc(size*2);
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    ALERT("Failed to parse config file: %s", error.c_str());
    configFile.close();
    return;
  }

  JsonObject root = doc.as<JsonObject>();

  for (JsonPair kv : root) {
    const char *key = kv.key().c_str();
    const char *value = kv.value().as<char*>();;
    if (strlen(key)==0) continue;
    LEAF_NOTICE("Load config value [%s] <= [%s]", key, value);
    this->put(String(key), String(value));
  }

  configFile.close();
  return;
}

void SPIFFSPreferencesLeaf::save(bool force_format)
{
  ALERT("%ssaving config to SPIFFS file %s", force_format?"REFORMATTING, then ":"",prefs_file.c_str());

  if (force_format) {
    ALERT("Reformatting SPIFFS");
    if (SPIFFS.format()) {
      NOTICE("Reformatted OK");
    }
    else {
      ALERT("SPIFFS Format failed");
      return;
    }
  }

  if (!SPIFFS.begin()) {
    ALERT("failed to mount FS");
    return;
  }

#if 0
  String bak = prefs_file + ".bak";
  if (!SPIFFS.rename(prefs_file.c_str(),bak.c_str())) {
    ALERT("Unable to create backup config file");
    return;
  }
#endif

  File configFile = SPIFFS.open(prefs_file.c_str(), "w");
  if (!configFile) {
    ALERT("Unable to open config file for writing");
    return;
  }

  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();

  for (int i=0; i < values->size(); i++) {
    String key = values->getKey(i);
    String value = values->getData(i);
    LEAF_NOTICE("Save config value [%s] <= [%s]", key.c_str(), value.c_str());
    root[key.c_str()] = value.c_str();
  }
  
  if (serializeJson(doc, configFile) == 0) {
    ALERT("Failed to serialise configuration");
  }
  configFile.close();
}

void SPIFFSPreferencesLeaf::put(String name, String value) {
  size_t p_size;

  LEAF_ENTER(L_INFO);

  if (values->has(name)) {
    String current = values->get(name);
    if (value == current) {
      LEAF_LEAVE;
      return;
    }
  }

  values->put(name, value);
  if (this->autosave) {
    this->save();
  }
  
  LEAF_LEAVE;
}




// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
