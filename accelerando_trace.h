/*
 * Macros for consistent use of trace output
 *
 * You should define DBG to be the stream you want to use (default: Serial)
 */

#define L_ALERT 0
#define L_WARN 1
#define L_NOTICE 2
#define L_INFO 3
#define L_DEBUG 4
#define L_TRACE 5

#define L_USE_DEFAULT -2

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL L_NOTICE
#endif
#ifndef MAX_DEBUG_LEVEL
#define MAX_DEBUG_LEVEL L_DEBUG
#endif
#ifndef DEBUG_FLUSH
#define DEBUG_FLUSH false
#endif
#ifndef DEBUG_LINES
#define DEBUG_LINES false
#endif
#ifndef DEBUG_COLOR
#define DEBUG_COLOR false
#endif
#ifndef DEBUG_WAIT
#define DEBUG_WAIT 0
#endif
#ifndef DEBUG_FILES
#define DEBUG_FILES false
#endif
#ifndef DEBUG_SLOW
#define DEBUG_SLOW 500
#endif

#ifndef DEBUG_THREAD
#define DEBUG_THREAD 0
#endif

#ifndef DEBUG_SYSLOG
#define DEBUG_SYSLOG 0
#endif

#ifndef DEBUG_SYSLOG_ENABLE
#define DEBUG_SYSLOG_ENABLE 0
#endif

bool use_debug = false;
int debug_level = DEBUG_LEVEL;
bool debug_files = DEBUG_FILES;
bool debug_lines = DEBUG_LINES;
bool debug_color = DEBUG_COLOR;
bool debug_flush = DEBUG_FLUSH;
int debug_slow = DEBUG_SLOW;
int debug_wait = DEBUG_WAIT;
#if DEBUG_SYSLOG
bool debug_syslog_enable = DEBUG_SYSLOG_ENABLE;
bool debug_syslog_ready = false;
#endif

#ifndef DBG
#define DBG Serial
#endif

Stream *debug_stream = &DBG;

#if DEBUG_THREAD
SemaphoreHandle_t debug_sem=NULL;
StaticSemaphore_t debug_sem_buf;

#endif

#if USE_BT_CONSOLE
#define DBGPRINT(...) {			\
  if (debug_stream) {debug_stream->print(__VA_ARGS__);}		\
  if (SerialBT && SerialBT->hasClient()) {SerialBT->print(__VA_ARGS__);}	\
  }
#define DBGWRITE(...) {			\
  if (debug_stream) {debug_stream->write(__VA_ARGS__);}		\
  if (SerialBT && SerialBT->hasClient()) {SerialBT->write(__VA_ARGS__);}	\
  }
#define DBGFLUSH(...) {			\
  if (debug_stream) {debug_stream->flush();}		\
  if (SerialBT && SerialBT->hasClient()) {SerialBT->flush();}	\
  }
#define DBGPRINTF(...) {			\
  if (debug_stream) {debug_stream->printf(__VA_ARGS__);}		\
  if (SerialBT && SerialBT->hasClient()) {SerialBT->printf(__VA_ARGS__);}	\
  }
#define DBGPRINTLN(...) { \
  if (debug_stream) {debug_stream->println(__VA_ARGS__);}		\
  if (SerialBT && SerialBT->hasClient()) {SerialBT->println(__VA_ARGS__);} \
  }
#else
#define DBGPRINT(...) if(debug_stream){debug_stream->print(__VA_ARGS__);}
#define DBGWRITE(...) if(debug_stream){debug_stream->write(__VA_ARGS__);}
#define DBGFLUSH(...) if(debug_stream){debug_stream->flush();}
#define DBGPRINTF(...) if(debug_stream){debug_stream->printf(__VA_ARGS__);}
#define DBGPRINTLN(...) if(debug_stream){debug_stream->println(__VA_ARGS__);}
#endif

#define DBGMILLIS(l) {							\
  if (debug_lines) {							\
    char loc_buf[64];snprintf(loc_buf, sizeof(loc_buf), "%s(%d)",	__PRETTY_FUNCTION__, __LINE__); \
  DBGPRINTF("#%8lu %6s              %-50s ", millis(), _level_str(l), loc_buf);	\
  } else if (debug_files) {						\
    char loc_buf[64];snprintf(loc_buf, sizeof(loc_buf), "%s@%s(%d)",	__func__, strrchr(__FILE__,'/')+1, __LINE__); \
  DBGPRINTF("#%8lu %6s              %-50s ", millis(), _level_str(l), loc_buf);	\
  } else {								\
    DBGPRINTF("#%8lu %6s              ", (unsigned long)millis(), _level_str(l));	\
  }									\
}									\


#define DBGINDENT DBGPRINTF("\t")

const char *_level_str(int l) {
  switch (l) {
  case L_ALERT: return "ALERT";
  case L_WARN: return "WARN";
  case L_NOTICE: return "NOTICE";
  case L_INFO: return "INFO";
  case L_DEBUG: return "DEBUG";
  default: return "TRACE";
  }
}

#if USE_OLED
#define OLED_TEXT(x,y,t) oled_text(x,y,t)
#else
#define OLED_TEXT(x,y,t) {}
#endif

#define OLEDLINE(l,s,...) {	\
  char buf[65];		 \
  snprintf(buf, sizeof(buf), __VA_ARGS__);	\
  __LEAF_DEBUG_PRINT__(__func__,__FILE__,__LINE__,"ACTION",L_WARN,"%s",buf); \
  OLED_TEXT(2, 1+l*10, buf);			\
  }

//#define BANNER(...)  ALERT(__VA_ARGS__); OLEDLINE(0,__VA_ARGS__)
#define STATUS(...) OLEDLINE(0,"STATUS",__VA_ARGS__)
#define ERROR(...)  OLEDLINE(2,"ERROR",__VA_ARGS__)
#define ACTION(...) OLEDLINE(2,"ACTION",__VA_ARGS__)

#if DEBUG_SYSLOG
#ifndef SYSLOG_HOST
#define SYSLOG_HOST "notused"
#endif
#ifndef SYSLOG_PORT
#define SYSLOG_PORT 514
#endif
String syslog_host=SYSLOG_HOST;
const unsigned int syslog_port = SYSLOG_PORT;
WiFiUDP UDP;
extern char device_id[DEVICE_ID_MAX];

void _udpsend(String dst, unsigned int port, const char *buf, unsigned int len)
{
  //Serial.print("udpsend "); Serial.print(dst); Serial.print(":");Serial.print(port);Serial.print(" => "); Serial.println(buf);
  static bool udpready = false;
#ifdef SYSLOG_IP
  static IPAddress syslogIP(SYSLOG_IP);
#else
  static IPAddress syslogIP;
#endif
  if (!udpready) {
    if (UDP.begin(syslog_port)) {
      udpready = true;
    }
    else {
      return;
    }
#ifndef SYSLOG_IP
    WiFi.hostByName(dst.c_str(), syslogIP);
#endif
  }
  UDP.beginPacket(syslogIP, port);
  UDP.write((uint8_t *)buf, len);
  UDP.endPacket();
}


#define SYSLOG(name, loc, l,...)				\
  if (debug_syslog_enable && debug_syslog_ready) {	\
    char syslogbuf[512]; \
    int facility = 1; \
    int severity; \
    int offset = 0; \
    time_t now; \
    struct tm localtm; \
    switch (l) { \
    case L_ALERT: severity=1; break; \
    case L_WARN: severity=4; break; \
    case L_NOTICE: severity=5; break; \
    case L_INFO: severity=6; break; \
    default: severity=7; \
    } \
    snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, "<%d>", (facility<<3)+severity); \
    offset = strlen(syslogbuf); \
    time(&now); \
    localtime_r(&now, &localtm); \
    strftime(syslogbuf+offset, sizeof(syslogbuf)-offset, "%b %e %T ", &localtm); \
    offset = strlen(syslogbuf); \
    /*snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, "%s ", device_id);*/ \
    snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, "%s %6s %s %s ", device_id, _level_str(l), name, loc);	\
    offset = strlen(syslogbuf); \
    snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, __VA_ARGS__); \
    _udpsend(syslog_host, syslog_port, syslogbuf, strlen(syslogbuf)); \
  }

#else
#define SYSLOG(name, loc, l,...) {}
#endif



void __DEBUG_INIT__()
{
  if (Serial) {
    //Serial.println("DEBUG_INIT");
  }
  else {
    debug_stream = NULL;
  }
#if DEBUG_THREAD
  //if (Serial) {
  // DBGPRINTLN("Multithreaded debug init");
  //}
  debug_sem = xSemaphoreCreateBinaryStatic(&debug_sem_buf); // can't fail
  xSemaphoreGive(debug_sem);
#endif
  use_debug=true;
}

void __LEAF_DEBUG_PRINT__(const char *func,const char *file, int line, const char *leaf_name, int l, const char *fmt, ...)
{
  if (!use_debug
#if DEBUG_THREAD
      || (debug_sem==NULL)
#endif
    ) {
#if !EARLY_SERIAL
    if (Serial) {
      Serial.println("DEBUG UNREADY (CALL __DEBUG_INIT__)");
    }
    return;
#endif
  }

  va_list ap;
  va_start(ap, fmt);
  char buf[160];
  char name_buf[16];
  char loc_buf[64];

  if (!func) func="unk";
  if (!file) file="/unk";
  snprintf(name_buf, sizeof(name_buf), "[%s]", leaf_name);
  unsigned long now =millis();
  bool locked=false;

#if DEBUG_THREAD
  if (xSemaphoreTake(debug_sem, (TickType_t)500/portTICK_PERIOD_MS) != pdTRUE) {
#ifdef LIVE_DANGEROUSLY
    locked=false; //muddle on
#else
    if (Serial) {
      Serial.println("DEBUG BLOCKED");
      return;
#endif
    }
  }
  else {
    locked=true;
  }
#endif

  DBGPRINTF("#%4d.%03d %6s %-12s ", (int)now/1000, (int)now%1000, _level_str(l), name_buf);
  if (debug_flush) {DBGFLUSH();}
  if (debug_lines) {
    snprintf(loc_buf, sizeof(loc_buf), "%s(%d) ", func, (int)line);
    DBGPRINTF("%-50s ", loc_buf);
  } else if (debug_files) {
    snprintf(loc_buf, sizeof(loc_buf), "%s@%s:%d ", func, strrchr(file,'/')+1, line);
    DBGPRINTF("%-50s ", loc_buf);
  }
  if (debug_wait>0) delay(debug_wait);
  if (debug_color) {
    switch (l) {
    case L_ALERT:
      DBGPRINT("\033[37;41m");
      break;
    case L_WARN:
      DBGPRINT("\033[37;43m");
      break;
    case L_NOTICE:
      DBGPRINT("\033[37;42m");
      break;
    case L_INFO:
      DBGPRINT("\033[37;44m");
      break;
    }
  }
  vsnprintf(buf, sizeof(buf), fmt, ap);
  if (debug_color) {
    DBGPRINT(buf);
    DBGPRINTLN("\033[39m\033[49m");
  } else {
    DBGPRINTLN(buf);
  }

  if (debug_flush) {DBGFLUSH();}

  SYSLOG(name_buf, loc_buf, l, "%s", buf);

#if DEBUG_THREAD
  if (locked) {
    xSemaphoreGive(debug_sem);
  }
#endif
}

#define __LEAF_DEBUG__(l,...) { if(getDebugLevel()>=(l)) {__LEAF_DEBUG_PRINT__(__func__,__FILE__,__LINE__,getNameStr(),(l),__VA_ARGS__);}}
#define __LEAF_DUMP__(l,...) { if(getDebugLevel()>=(l)) {_DumpHex(l, __VA_ARGS__);}}
#define __LEAF_DEBUG_AT__(loc,l,...) { if(getDebugLevel()>=(l)) {__LEAF_DEBUG_PRINT__((loc).func,(loc).file,(loc).line,getNameStr(),(l),__VA_ARGS__);}}
#define __DEBUG__(l,...) { if(debug_level>=(l)) {__LEAF_DEBUG_PRINT__(__func__,__FILE__,__LINE__,"stacx",(l),__VA_ARGS__);}}
#define __DEBUG_AT__(loc,l,...) { if(debug_level>=(l)) {__LEAF_DEBUG_PRINT__((loc).func,(loc).file,(loc).line,"stacx",(l),__VA_ARGS__);}}
#define __DEBUG_AT_FROM__(loc,n,l,...) { if(debug_level>=(l)) {__LEAF_DEBUG_PRINT__((loc).func,(loc).file,(loc).line,(n),(l),__VA_ARGS__);}}

#define ENTER(l)  int enterlevel=l; if (debug_level>=l) __DEBUG__(l,">%s", __func__)
#define LEAVE  __DEBUG__(enterlevel,"<%s", __func__)
#define RETURN(x) LEAVE; return(x)


#define ALERT( ...) __DEBUG__(L_ALERT ,__VA_ARGS__)
#define ALERT_AT(loc, ...) __DEBUG_AT__(loc, L_ALERT ,__VA_ARGS__)

#if MAX_DEBUG_LEVEL >= L_WARN
#define WARN(  ...) __DEBUG__(L_WARN  ,__VA_ARGS__)
#define WARN_AT(loc,...) __DEBUG_AT__((loc),L_WARN,__VA_ARGS__)
#else
#define WARN(...) {}
#define WARN_AT(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_NOTICE
#define NOTICE(...) __DEBUG__(L_NOTICE,__VA_ARGS__)
#define NOTICE_AT(loc,...) __DEBUG_AT__((loc),L_NOTICE,__VA_ARGS__)
#else
#define NOTICE(...) {}
#define NOTICE_AT(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_INFO
#define INFO(  ...) __DEBUG__(L_INFO  ,__VA_ARGS__)
#define INFO_AT(loc,...) __DEBUG_AT__((loc),L_INFO,__VA_ARGS__)
#else
#define INFO(...) {}
#define INFO_AT(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_DEBUG
#define DEBUG( ...) __DEBUG__(L_DEBUG ,__VA_ARGS__)
#else
#define DEBUG(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_TRACE
#define TRACE( ...) __DEBUG__(L_TRACE ,__VA_ARGS__)
#else
#define TRACE(...) {}
#endif

#define LEAF_ENTER(l)  int enterlevel=l; unsigned long entertime=millis(); if (getDebugLevel()>=l) {__LEAF_DEBUG__(l,">%s", __func__)}
#define LEAF_ENTER_BOOL(l,b)  int enterlevel=l; unsigned long entertime=millis(); if (getDebugLevel()>=l) {__LEAF_DEBUG__(l,">%s(%s)", __func__, TRUTH(b))}
#define LEAF_ENTER_STATE(l,b)  int enterlevel=l; unsigned long entertime=millis(); if (getDebugLevel()>=l) {__LEAF_DEBUG__(l,">%s(%s)", __func__, STATE(b))}
#define LEAF_ENTER_BYTE(l,b)  int enterlevel=l; unsigned long entertime=millis(); if (getDebugLevel()>=l) {__LEAF_DEBUG__(l,">%s(0x%02x)", __func__, ((uint8_t)(b)))}
#define LEAF_ENTER_INT(l,i)  int enterlevel=l; unsigned long entertime=millis(); if (getDebugLevel()>=l) {__LEAF_DEBUG__(l,">%s(%d)", __func__, ((int)(i)))}
#define LEAF_ENTER_INTPAIR(l,i1,i2)  int enterlevel=l; unsigned long entertime=millis(); if (getDebugLevel()>=l) {__LEAF_DEBUG__(l,">%s(%d,%d)", __func__, (i1),(i2))}
#define LEAF_ENTER_LONG(l,i)  int enterlevel=l; unsigned long entertime=millis(); if (getDebugLevel()>=l) {__LEAF_DEBUG__(l,">%s(%lu)", __func__, (i))}
#define LEAF_ENTER_STR(l,s)  int enterlevel=l; unsigned long entertime=millis(); if (getDebugLevel()>=l) {__LEAF_DEBUG__(l,">%s(%s)", __func__, (s).c_str())}
#define LEAF_ENTER_STRPAIR(l,s1,s2)  int enterlevel=l; unsigned long entertime=millis(); if (getDebugLevel()>=l) {__LEAF_DEBUG__(l,">%s(%s, %s)", __func__, (s1).c_str(), (s2).c_str())}

#define LEAF_SLOW_CHECK_MSEC(msec) {int leave_elapsed=millis()-entertime; if (leave_elapsed > msec) {LEAF_WARN("SLOW EXECUTION %dms", leave_elapsed); }}
#define LEAF_SLOW_CHECK LEAF_SLOW_CHECK_MSEC(debug_slow)

#define LEAF_LEAVE LEAF_SLOW_CHECK; __LEAF_DEBUG__(enterlevel,"<%s", __func__);
#define LEAF_LEAVE_SLOW(msec) LEAF_SLOW_CHECK_MSEC(msec); __LEAF_DEBUG__(enterlevel,"<%s", __func__);

#define LEAF_RETURN(x)  LEAF_LEAVE;return(x);
#define LEAF_RETURN_SLOW(msec, x)  LEAF_LEAVE_SLOW(msec);return(x);
#define LEAF_VOID_RETURN  LEAF_LEAVE;return;
#define LEAF_BOOL_RETURN(x)  LEAF_SLOW_CHECK;  __LEAF_DEBUG__(enterlevel,"<%s %s", __func__, TRUTH(x)); return (x)
#define LEAF_BOOL_RETURN_SLOW(msec, x)  LEAF_SLOW_CHECK_MSEC(msec);  __LEAF_DEBUG__(enterlevel,"<%s %s", __func__, TRUTH(x)); return (x)
#define LEAF_INT_RETURN(x)  LEAF_SLOW_CHECK;  __LEAF_DEBUG__(enterlevel,"<%s %d", __func__, (int)(x)); return (x)
#define LEAF_STR_RETURN(x)  LEAF_SLOW_CHECK;  __LEAF_DEBUG__(enterlevel,"<%s %s", __func__, (x).c_str()); return (x)
#define LEAF_STR_RETURN_SLOW(msec,x)  LEAF_SLOW_CHECK_MSEC(msec);  __LEAF_DEBUG__(enterlevel,"<%s %s", __func__, (x).c_str()); return (x)

#define LEAF_ALERT( ...) __LEAF_DEBUG__(L_ALERT ,__VA_ARGS__)
#define LEAF_ALERT_AT(loc, ...)  __LEAF_DEBUG_AT__((loc), L_ALERT  ,__VA_ARGS__)

#if MAX_DEBUG_LEVEL >= L_WARN
#define LEAF_WARN(...) __LEAF_DEBUG__(L_WARN,__VA_ARGS__)
#define LEAF_WARN_AT(loc, ...)  __LEAF_DEBUG_AT__((loc), L_WARN  ,__VA_ARGS__)
#else
#define LEAF_WARN(...) {}
#define LEAF_WARN_AT(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_NOTICE
#define LEAF_NOTICE(...) __LEAF_DEBUG__(L_NOTICE,__VA_ARGS__)
#define LEAF_NOTICE_AT(loc, ...)  __LEAF_DEBUG_AT__((loc.file?(loc):(HERE)), L_NOTICE  ,__VA_ARGS__)
#else
#define LEAF_NOTICE(...) {}
#define LEAF_NOTICE_AT(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_INFO
#define LEAF_INFO(...)  __LEAF_DEBUG__(L_INFO  ,__VA_ARGS__)
#define LEAF_INFO_AT(loc, ...)  __LEAF_DEBUG_AT__((loc), L_INFO  ,__VA_ARGS__)
#define LEAF_INFODUMP(...)  __LEAF_DUMP__(L_INFO, __VA_ARGS__)
#else
#define LEAF_INFO(...) {}
#define LEAF_INFO_AT(...) {}
#define LEAF_INFODUMP(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_DEBUG
#define LEAF_DEBUG(...)  __LEAF_DEBUG__(L_DEBUG  ,__VA_ARGS__)
#define LEAF_DEBUG_AT(loc, ...)  __LEAF_DEBUG_AT__((loc), L_DEBUG  ,__VA_ARGS__)
#else
#define LEAF_DEBUG(...) {}
#define LEAF_DEBUG_AT(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_TRACE
#define LEAF_TRACE(...)  __LEAF_DEBUG__(L_TRACE  ,__VA_ARGS__)
#define LEAF_TRACE_AT(loc, ...)  __LEAF_DEBUG_AT__((loc), L_TRACE  ,__VA_ARGS__)
#else
#define LEAF_TRACE(...) {}
#define LEAF_TRACE_AT(...) {}
#endif

#define STATE(s) ((s)?"HIGH":"LOW")
#define TRUTH(b) ((b)?"TRUE":"FALSE")
#define TRUTH_lc(b) ((b)?"true":"false")
#define ABILITY(a) ((a)?"on":"off")
#define HEIGHT(h) ((h)?"up":"down")

typedef struct
{
  const char *file;
  const char *func;
  int line;
} codepoint_t;

codepoint_t undisclosed_location = {NULL,NULL,-1};
#define HERE ((codepoint_t){__FILE__,__func__,__LINE__})
#define CODEPOINT(where) ((where).file?where:HERE)

void _DumpHex(int level, const char *leader, const void* data, size_t size) {
	int save_lines = debug_lines;
	debug_lines = 0;

	char ascii[17];
	size_t i, j;
	int leader_len = strlen(leader)+2;
	ascii[16] = '\0';
	DBGMILLIS(level);
	DBGPRINTF("%s: ", leader);
	if (size > 16) {
	  // multi line dump
	  DBGPRINTF("\n      ");
	  leader_len = 6;
	}
	for (i = 0; i < size; ++i) {
		DBGPRINTF("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			DBGPRINTF(" ");
			if ((i+1) % 16 == 0) {
				DBGPRINTF("|  %s \n", ascii);
				if (i+1 != size) {
					for (int ldr=0;ldr<leader_len;ldr++) {
						DBGPRINTF(" ");
					}
				}
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					DBGPRINTF(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					DBGPRINTF("   ");
				}
				DBGPRINTF("|  %s \n", ascii);
			}
		}
	}
	debug_lines = save_lines;
}

void DumpHex(int level, const char *leader, const void* data, size_t size) {
	if (debug_level < level) return;
	_DumpHex(level, leader, data, size);
}


// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
