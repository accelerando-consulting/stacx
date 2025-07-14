#if USE_PREFS
#include "abstract_storage.h"
#include <FS.h>
#include <LittleFS.h>

class FSPreferencesLeaf : public StorageLeaf
{
public:
  FSPreferencesLeaf(String name, String defaults="", String filename="", bool auto_save=true)
    : StorageLeaf(name, defaults)
    , Debuggable(name)
  {
    if (filename.length()) {
      this->prefs_file = filename;
    }
    this->impersonate_backplane = true;
    this->auto_save = auto_save;
  }

  virtual void setup();
  virtual void status_pub() {}; // override the superclass method to be a noop
  virtual void load(String name="");
  virtual void save(String name="", bool force_format=false);
  virtual void put(String name, String value, bool no_save=false);
  virtual void remove(String name, bool no_save=false);

protected:
  String prefs_file = "/prefs.json";
  bool auto_save = false;
};

static void listDir(const char * dirname) {
  DBGPRINTF("Listing directory: %s\n", dirname);

#ifdef ESP8266
  Dir root = LittleFS.openDir(dirname);
  while (root.next()) {
    File file = root.openFile("r");
    DBGPRINT("    ");
    DBGPRINT(root.fileName());
    DBGPRINT(" ");
    if (file.isDirectory()) {
      DBGPRINTLN("<DIR>");
    }
    else {
      DBGPRINTLN(file.size());
    }
    file.close();
  }
#else
  File root = LittleFS.open(dirname);
  File file = root.openNextFile();
  while (file) {
    DBGPRINT("    ");
    DBGPRINT(file.name());
    DBGPRINT(" ");
    if (file.isDirectory()) {
      DBGPRINTLN("<DIR>");
    }
    else {
      DBGPRINTLN(file.size());
    }
    file = root.openNextFile();
  }
#endif
}


void FSPreferencesLeaf::setup()
{
  // note: slightly weird order of superclass setup in this module, in order to get the
  // filesystem initialised before trying to load saved preferences
  pixel_code(HERE, 1, PC_BLUE);
  Leaf::setup();
  LEAF_ENTER(L_INFO);

  if (!LittleFS.begin()) {
    LEAF_ALERT("NO LittleFS.  Formatting");
    LittleFS.format();
    LEAF_ALERT("Rebooting after format");
    delay(3000);
    Leaf::reboot("format", true);
  }
  pixel_code(HERE, 2, PC_BLUE);
  LEAF_NOTICE("LittleFS setup done");

  pixel_code(HERE, 3, PC_BLUE);
  if (debug_stream && (getDebugLevel()>=L_NOTICE)) {
#ifdef ESP8266
    File file = LittleFS.open(prefs_file,"r");
#else
    File file = LittleFS.open(prefs_file,"r");
#endif
    if (file ) {
      LEAF_NOTICE("Configuration File:");
      while(file.available()){
	DBGWRITE(file.read());
      }
      DBGPRINTLN();
      file.close();
    }
  }

  pixel_code(HERE, 4, PC_BLUE);
  StorageLeaf::setup();

  pixel_code(HERE, 5, PC_BLUE);

  registerIntValue("debug_level", &debug_level);
  registerBoolValue("debug_flush", &debug_flush, "Flush stream after every log message");
  registerBoolValue("debug_lines", &debug_lines, "Include line numbers log messages");
  registerBoolValue("debug_color", &debug_color, "Include ANSI color changes in log messages");
#if DEBUG_SYSLOG
  registerBoolValue("debug_syslog_enable", &debug_syslog_enable);
  registerStrValue("debug_syslog_host", &syslog_host);
#endif

  // Load a configured device ID if present.  This relies on the prefs leaf being the first leaf.
  pixel_code(HERE, 6, PC_BLUE);
  String new_device_id = this->get("device_id", device_id);
  if (new_device_id != device_id) {
    strncpy(device_id, new_device_id.c_str(), sizeof(device_id));
   LEAF_NOTICE("Loaded device_id [%s] from NVS", device_id);
    // redo the topic setup for this leaf
    Leaf::setup();
  }

  registerIntValue("hello_trace_level", &hello_trace_level);
  registerIntValue("heartbeat_interval_sec", &::heartbeat_interval_seconds, "Period after which to publish a proof-of-life message");
#if HEAP_CHECK
  registerIntValue("heap_check_interval", &heap_check_interval, "Period in milliseconds to check and log memory use");
#endif


  // Check for preferences of the form NAME_leaf_enable (default on) which when set off can temporarily disable a leaf
  // this used to be leaf_inhibit_NAME=on but double negatives are bad mmkay?
  pixel_code(HERE, 7, PC_BLUE);
  for (int i=0; leaves[i]; i++) {
    Leaf *l = leaves[i];
    String leaf_pref = l->getName()+"_leaf_enable";
    if (has(leaf_pref)) {
      if (parseBool(get(leaf_pref))) {
	if (!l->canRun()) {
	  LEAF_WARN("Leaf %s is enabled by config (%s=true)", l->describe().c_str(), leaf_pref.c_str());
	  l->permitRun();
	}
      }
      else {
	if (l->canRun()) {
	  LEAF_WARN("Leaf %s is disabled by config (%s=false)", l->describe().c_str(), leaf_pref.c_str());
	  l->inhibit();
	}
      }
    }
  }

  pixel_code(HERE, 7, PC_BLUE);
  LEAF_LEAVE;
}

void FSPreferencesLeaf::load(String name)
{
  LEAF_ENTER(L_NOTICE);

  if ((name == "") || (name=="1")) {
    name = prefs_file;
  }

  if (!LittleFS.exists(name.c_str())) {
    LEAF_ALERT("Configuration file '%s' not found", name.c_str());
    LEAF_VOID_RETURN;
  }

  //file exists, reading and loading
  LEAF_NOTICE("reading config file [%s]", name.c_str());
  File configFile = LittleFS.open(name, "r");
  if (!configFile) {
    LEAF_ALERT("Cannot read config file");
    LEAF_VOID_RETURN;
  }

  size_t size = configFile.size();
  if (size == 0) {
    LEAF_ALERT("Configuration file %s is empty", name.c_str());
    // TODO: fall back to factory confg?
    LEAF_VOID_RETURN;
  }
  LEAF_NOTICE("Parsing config file, size=%d", size);

  JsonDocument doc;
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

  // since we may have just (re)configured OURSELF, load the values for this leaf
  loadValues();

  LEAF_VOID_RETURN;
}

void FSPreferencesLeaf::save(String name, bool force_format)
{
  LEAF_ENTER(L_INFO);

  if ((name == "") || (name=="1")) {
    name = prefs_file;
  }

  LEAF_NOTICE("%ssaving config to FS file %s", force_format?"REFORMATTING, then ":"",name.c_str());

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

  String temp = name + ".tmp";
  String bak = name + ".bak";
  if (LittleFS.exists(name)) {
    LEAF_NOTICE("Rename %s =>  %s", name.c_str(), bak.c_str());
    if (!LittleFS.rename(name,bak)) {
      LEAF_ALERT("Unable to create backup file '%s'",bak.c_str());
    }
  }

  JsonDocument doc;

  int min_size = 0;

  for (int i=0; i < values->size(); i++) {
    String key = values->getKey(i);
    String value = values->getData(i);
    min_size += 3+key.length()+4+value.length()+3; // output resembles  ^__"foo": "bar",?\n

    LEAF_INFO("Save config value [%s] <= [%s]", key.c_str(), value.c_str());
    //root[key.c_str()] = value.c_str();
    doc[key] = value;
  }

  size_t output_size = measureJsonPretty(doc);
  LEAF_NOTICE("Saving %d bytes to %s", (int)output_size, temp.c_str());
  if (getDebugLevel() >= L_INFO) {
    serializeJsonPretty(doc, Serial);  DBGPRINTLN();
  }

  File configFile = LittleFS.open(temp, "w");
  if (!configFile) {
    LEAF_ALERT("Unable to open '%s' for writing", temp.c_str());
    return;
  }

  if (serializeJsonPretty(doc, configFile) == 0) {
    LEAF_ALERT("Failed to serialise configuration");
  }
  configFile.close();

  // reopen the file to inspect its size and readability
  configFile = LittleFS.open(temp, "r");
  bool success = false;
  if (!configFile) {
    LEAF_ALERT("Unable to reopen temporary file '%s'", temp.c_str());
  }
  else {
    size_t saved = configFile.size();
    configFile.close();
    if (saved != output_size) {
      LEAF_WARN("Saved file size %d is not expected size %d", (int)saved, (int)output_size);
    }
    else {
      LEAF_NOTICE("Rename %s =>  %s", temp.c_str(), name.c_str());
      if (!LittleFS.rename(temp,name)) {
	LEAF_ALERT("Unable to rename temporary output file");
      }
      else {
	success = true;
      }
    }
  }
  if (!success) {
    LEAF_ALERT("PREFERENCE SAVE FAILED, REVERTING TO BACKUP");
    LittleFS.rename(bak, name);
  }

  LEAF_LEAVE_SLOW(1000);
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

void FSPreferencesLeaf::remove(String name, bool no_save)
{
  StorageLeaf::remove(name, no_save);
  if (!no_save && this->auto_save) {
    this->save();
  }
}
#endif // USE_PREFS


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
