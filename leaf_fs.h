//
//@**************************** class SDCardLeaf *****************************
//
// Access to SD cards
//
#include "FS.h"
#include <LittleFS.h>
#include <SPI.h>

#define FS_ALLOW_FORMAT true
#define FS_DONOT_FORMAT false
#define FS_DEFAULT NULL

class FSLeaf : public Leaf
{
  fs::FS *fs;

  //
  // Declare your leaf-specific instance data here
  //
  bool format_on_fail = false;

public:
  //
  // Leaf constructor method(s)
  // Call the superclass constructor to handle common arguments (type, name,
  // pins)
  //
  // Pass fs=NULL to initalise and use littlefs, otherwise pass your own FS pointer
  //
  FSLeaf(String name, fs::FS *fs=NULL, bool format=false)
    : Leaf("fs", name, (pinmask_t)0)
    , Debuggable(name)
  {
    this->fs = fs;
    format_on_fail = format;
  }

  //
  // Arduino Setup function
  // Use this to configure any IO resources
  //
  void setup(void) {
    Leaf::setup();
    LEAF_ENTER(L_INFO);

    if (!fs) {
#ifdef ESP8266
      if(!LittleFS.begin()) {
#else
      if(!LittleFS.begin(format_on_fail)) {
#endif
	LEAF_ALERT("Filesystem Mount Failed");
	return;
      }
      fs = &LittleFS;
#ifdef ESP8266
      FSInfo info;
      if (LittleFS.info(info)) {
	LEAF_NOTICE("Total space: %lukB", (unsigned long)(info.totalBytes / 1024));
	LEAF_NOTICE("Used space: %lukB", (unsigned long)(info.usedBytes / 1024));
      }
#else
      LEAF_NOTICE("Total space: %lukB", (unsigned long)(LittleFS.totalBytes() / 1024));
      LEAF_NOTICE("Used space: %lukB", (unsigned long)(LittleFS.usedBytes() / 1024));
#endif
    }

    if (getDebugLevel() >= L_NOTICE) {
      listDir("/", 0, NULL);
    }

    registerCommand(HERE,"append/+", "append payload to a file");
    registerCommand(HERE,"appendl/+", "append payload to a file, adding a newline");
    registerCommand(HERE,"ls", "list a directory");
    registerCommand(HERE,"cat", "print the content of a file");
    registerCommand(HERE,"rm", "remove a file");
    registerCommand(HERE,"mv", "rename a file (oldname SPACE newname)");
    registerCommand(HERE,"format", "format flash filesystem");
    registerCommand(HERE,"store", "store data to a file");

    LEAF_LEAVE;
  }

    void listDir(const char * dirname, uint8_t levels, Stream *output=&Serial){
    LEAF_NOTICE("Listing directory: %s", dirname);

    File root = fs->open(dirname);
    if(!root){
      LEAF_NOTICE("Failed to open directory");
      return;
    }
    if(!root.isDirectory()){
      LEAF_NOTICE("Not a directory");
      return;
    }

    File file = root.openNextFile();
    while(file){
      if  (file.isDirectory()) {
	LEAF_NOTICE("    %s <DIR>", file.name());
      }
      else {
	LEAF_NOTICE("    %s %d", file.name(), (int)file.size());
      }
      if (output) {
	DBGPRINT("    ");
	DBGPRINT(file.name());
	DBGPRINT(" ");
	if (file.isDirectory()) {
	  DBGPRINTLN("<DIR>");
	}
	else {
	  DBGPRINTLN(file.size());
	}
      }
      file = root.openNextFile();
    }
  }

  void createDir(const char * path){
    LEAF_NOTICE("Creating Dir: %s", path);
    if(fs->mkdir(path)){
      LEAF_NOTICE("Dir created");
    } else {
      LEAF_NOTICE("mkdir failed");
    }
  }

  void removeDir(const char * path){
    LEAF_NOTICE("Removing Dir: %s", path);
    if(fs->rmdir(path)){
      LEAF_NOTICE("Dir removed");
    } else {
      LEAF_NOTICE("rmdir failed");
    }
  }

  void readFile(const char * path){
    LEAF_NOTICE("Reading file: %s", path);

    File file = fs->open(path);
    if(!file){
      LEAF_NOTICE("Failed to open file for reading");
      return;
    }

    LEAF_NOTICE("Read from file: ");
    char buf[257];
    int a;
    while(a = file.available()){
      if (a>=sizeof(buf)) a=sizeof(buf)-1;
      int got = file.readBytesUntil('\n',buf, a);
      if (got==0) break;
      buf[got]='\0';
      DBGPRINTLN(buf);
    }
    file.close();
  }

  void writeFile(const char * path, const char * message){
    LEAF_NOTICE("Writing file: %s", path);

    File file = fs->open(path, FILE_WRITE);
    if(!file){
      LEAF_NOTICE("Failed to open file for writing");
      return;
    }
    if(file.print(message)){
      LEAF_NOTICE("File written");
    } else {
      LEAF_NOTICE("Write failed");
    }
    file.close();
  }

  void appendFile(const char * path, const char * message, bool newline=false){
    LEAF_INFO("Appending to file: %s", path);

    File file = fs->open(path, FILE_APPEND);
    if(!file){
      LEAF_NOTICE("Failed to open file for appending");
      return;
    }
    bool result;
    if (newline) {
      result = file.println(message);
    }
    else {
      result = file.print(message);
    }
    if(result){
      //LEAF_DEBUG("Message appended");
    } else {
      LEAF_ALERT("Append failed");
    }
    file.close();
  }

  bool renameFile(String &path1, String &path2){
    bool result=false;
    LEAF_NOTICE("Renaming file %s to %s", path1.c_str(), path2.c_str());
    if (fs->rename(path1, path2)) {
      LEAF_NOTICE("File renamed");
      result = true;

    } else {
      LEAF_ALERT("Rename of '%s' failed", path1.c_str());
    }
    return result;
  }

  void deleteFile(const char * path){
    LEAF_NOTICE("Deleting file: %s", path);
    if(fs->remove(path)){
      LEAF_NOTICE("File deleted");
    } else {
      LEAF_NOTICE("Delete failed");
    }
  }

  void testFileIO(const char * path){
    File file = fs->open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
      len = file.size();
      size_t flen = len;
      start = millis();
      while(len){
	size_t toRead = len;
	if(toRead > 512){
	  toRead = 512;
	}
	file.read(buf, toRead);
	len -= toRead;
      }
      end = millis() - start;
      LEAF_NOTICE("%u bytes read for %u ms", flen, end);
      file.close();
    } else {
      LEAF_NOTICE("Failed to open file for reading");
    }


    file = fs->open(path, FILE_WRITE);
    if(!file){
      LEAF_NOTICE("Failed to open file for writing");
      return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
      file.write(buf, 512);
    }
    end = millis() - start;
    LEAF_NOTICE("%u bytes written for %u ms", 2048 * 512, end);
    file.close();
  }

  void mqtt_do_subscribe()
  {
    Leaf::mqtt_do_subscribe();
  }

  //
  // MQTT message callback
  //
  virtual bool mqtt_receive(String type, String name, String topic, String payload, bool direct=false) {
    LEAF_ENTER(L_DEBUG);
    bool handled = false;

    do {
      WHEN("cmd/format", {
	LEAF_NOTICE("littlefs format");
	LittleFS.format();
	LEAF_NOTICE("littlefs format done");
	if (!LittleFS.begin()) {
	  LEAF_ALERT("LittleFS mount failed");
	}
	})
      else if (topic.startsWith("cmd/append/") || topic.startsWith("cmd/appendl/")) {
	handled=true;
	bool newline = false;
	if (topic.startsWith("cmd/appendl/")) {
	  topic.remove(0, strlen("cmd/appendl"));
	  newline = true;
	}
	else {
	  topic.remove(0, strlen("cmd/append"));
	}

	if (topic.length() < 2) {
	  LEAF_ALERT("filename too short");
	  break;
	}
	appendFile(topic.c_str(), payload.c_str(), newline);
      }
      ELSEWHEN("cmd/store",{
	File file = fs->open(payload.c_str(), "w");
	if(!file) {
	  LEAF_ALERT("File not writable");
	  return handled;
	}
	// Read from "stdin" (console)
	DBGPRINTLN("> (send CRLF.CRLF to finish)");
	while (debug_stream) {
	  String line = debug_stream->readStringUntil('\n');
	  if (line.startsWith(".\r") || line.startsWith(".\n")) {
	    break;
	  }
	  DBGPRINTLN(line);
	  file.println(line);
	}
	file.close();
      })
      ELSEWHEN("cmd/ls",{
	if (payload == "") payload="/";
	listDir(payload.c_str(),1);
	})
      ELSEWHEN("cmd/cat",{
	readFile(payload.c_str());
      })
      ELSEWHEN("cmd/rm",{
	deleteFile(payload.c_str());
      })
      ELSEWHENEITHER("cmd/mv","cmd/rename",{
	int pos = payload.indexOf(" ");
	if (pos < 0) break;
	String from = payload.substring(0,pos);
	String to = payload.substring(pos+1);
	renameFile(from, to);
      })
      ELSEWHEN("cmd/fsinfo",{
#ifdef ESP8266
	FSInfo info;
	if (LittleFS.info(info)) {
	  mqtt_publish("status/fs_total_bytes", String(info.totalBytes));
	  mqtt_publish("status/fs_used_bytes", String(info.usedBytes));
	}
#else
	mqtt_publish("status/fs_total_bytes", String(LittleFS.totalBytes()));
	mqtt_publish("status/fs_used_bytes", String(LittleFS.usedBytes()));
#endif
      })
      else {
	handled = Leaf::mqtt_receive(type, name, topic, payload, direct);
      }
    } while(false);

    return handled;
  }

};


// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
