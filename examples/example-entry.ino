#include "config.h"
#define USE_PREFS 0
#define USE_OTA 0
#define USE_FTP 0
#include "stacx.h"

#if USE_PREFS
#include "leaf_fs_preferences.h"
#endif
#include "leaf_ip_null.h"
#include "leaf_pubsub_null.h"
#include "leaf_ip_esp.h"
#include "leaf_pubsub_mqtt_esp.h"
#include "leaf_shell.h"

#include "leaf_lock.h"
#include "leaf_contact.h"
#include "leaf_button.h"

#include "leaf_tone.h"
#include "leaf_light.h"
#include "leaf_motion.h"

#include "app_entry.h"



Leaf *leaves[] = {
  //new FSLeaf("fs", FS_DEFAULT, FS_ALLOW_FORMAT),
#if USE_PREFS
  new FSPreferencesLeaf("prefs"),
#endif
  new ShellLeaf("shell"),
  
  new IpEspLeaf("wifi","fs,prefs"),
  (new PubsubEspAsyncMQTTLeaf("wifimqtt","fs,prefs,wifi"))->setTrace(L_NOTICE),
  //new IpNullLeaf("nullip", "fs"),
  //new PubsubNullLeaf("nullmqtt", "nullip"),


  new LockLeaf("entry" ,   LEAF_PIN( 5 /* D1 OUT */), true, true), 
  new ContactLeaf("entry", LEAF_PIN( 4 /* D2  IN */)),
  new ButtonLeaf("egress", LEAF_PIN( 0 /* D3  IN */)),
  
  new ToneLeaf("spkr",     LEAF_PIN(14 /* D5 OUT */)),
  (new LightLeaf("entry",   "", LEAF_PIN(12 /* D6 OUT */)))->setTrace(L_NOTICE),
  new MotionLeaf("porch",  LEAF_PIN(13 /* D7  IN */)),
  new MotionLeaf("entry",  LEAF_PIN(16 /* D8  IN */)),
  
  (new EntryAppLeaf("entry", "light@entry,button@egress,motion@porch,motion@entry"))->setTrace(L_NOTICE),
  
  NULL
};


