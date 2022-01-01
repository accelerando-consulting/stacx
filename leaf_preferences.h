
#include "abstract_storage.h"

#if defined(ESP8266)
#error Preferences not supported on ESP8266.  Use FSPreferences.
#else
#include <Preferences.h>
#endif

class PreferencesLeaf : public StorageLeaf
{
public:
  PreferencesLeaf(String name, String defaults="") : StorageLeaf(name,defaults) {
  }

  virtual String get(String name, String defaultValue = "");
  virtual void put(String name, String value);

  //virtual bool mqtt_receive(String type, String name, String topic, String payload);

protected:
  Preferences preferences;
};

String PreferencesLeaf::get(String name, String defaultValue) {
  String result;

  if (values->has(name)) {
    //
    // Value is in our in-memory cache
    //
    result = values->get(name);
  }
  else {
    //
    // Value not in cache, check the flash NVS
    //
    preferences.begin(leaf_name.c_str(), true);
    result = preferences.getString(name.c_str(), defaultValue);
    preferences.end();
    values->put(name, result);
  }
  LEAF_INFO("    read preference %s=%s", name.c_str(), result.c_str());

  return result;
}

void PreferencesLeaf::put(String name, String value) {
  size_t p_size;

  LEAF_ENTER(L_INFO);

  preferences.begin(leaf_name.c_str(), false);
  p_size = preferences.putString(name.c_str(), value);
  if (p_size != value.length()) {
    LEAF_ALERT("Preference write failed for %s=%s", name.c_str(), value.c_str());
  }
  else {
    LEAF_INFO("Wrote preference %s=%s size=%d", name.c_str(), value.c_str(), (int)p_size);
  }
  preferences.end();
  values->put(name, value);
  LEAF_LEAVE;
}

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
