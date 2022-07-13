#pragma once

#include "abstract_storage.h"

#if defined(ESP8266)
#error Preferences not supported on ESP8266.  Use FSPreferences.
#else
#include <Preferences.h>
#include "nvs.h"
#endif

class StacxPreferences : public Preferences 
{
public:
  StacxPreferences() : Preferences() 
  {
  };
  
  
  void listPreferences(const char *nspace, const char *part_name=NULL) 
  {
    nvs_iterator_t it = nvs_entry_find(part_name, nspace, NVS_TYPE_ANY);
    while (it != NULL) {
      nvs_entry_info_t info;
      nvs_entry_info(it, &info);
      it = nvs_entry_next(it);
      NOTICE("listPreferences: key '%s', type '%d' \n", info.key, info.type);
    };
  };
  
};
  

class PreferencesLeaf : public StorageLeaf
{
public:
  PreferencesLeaf(String name, String defaults="") : StorageLeaf(name,defaults) {

    preferences.listPreferences(leaf_name.c_str());
  }

  virtual String get(String name, String defaultValue = "");
  virtual void put(String name, String value, bool no_save=false);

protected:
  StacxPreferences preferences;
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
    if (!preferences.isKey(name.c_str())) {
      // not defined, return default
      result = defaultValue;
    }
    else {
      result = preferences.getString(name.c_str(), defaultValue);
      LEAF_NOTICE("    read preference %s=%s", name.c_str(), result.c_str());
    }
    preferences.end();
    values->put(name, result);
  }
  LEAF_INFO("Read preference %s=%s", name.c_str(), result.c_str());

  return result;
}

void PreferencesLeaf::put(String name, String value, bool no_save) {
  size_t p_size;

  LEAF_ENTER(L_INFO);

  values->put(name, value);
  if (!no_save) {
    preferences.begin(leaf_name.c_str(), false);
    p_size = preferences.putString(name.c_str(), value);
    if (p_size != value.length()) {
      LEAF_ALERT("Preference write failed for %s=%s (%d)", name.c_str(), value.c_str(), p_size);
    }
    else {
      LEAF_INFO("Wrote preference %s=%s size=%d", name.c_str(), value.c_str(), (int)p_size);
    }
    preferences.end();
  }
  LEAF_LEAVE;
}


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
