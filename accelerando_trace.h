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
#ifndef DEBUG_WAIT
#define DEBUG_WAIT 0
#endif
#ifndef DEBUG_FILES
#define DEBUG_FILES false
#endif
#ifndef DEBUG_SLOW
#define DEBUG_SLOW 500
#endif

int debug_level = DEBUG_LEVEL;
bool debug_files = DEBUG_FILES;
bool debug_lines = DEBUG_LINES;
bool debug_flush = DEBUG_FLUSH;
int debug_slow = DEBUG_SLOW;
int debug_wait = DEBUG_WAIT;


#ifndef DBG
#define DBG Serial
#endif

Stream *debug_stream = &DBG;

#if USE_BT_CONSOLE
#define DBGPRINTF(...) {			\
  debug_stream->printf(__VA_ARGS__);			\
  if (SerialBT && SerialBT->hasClient()) SerialBT->printf(__VA_ARGS__);	\
  }
#define DBGPRINTLN(...) { \
  debug_stream->println(__VA_ARGS__);       \
  if (SerialBT && SerialBT->hasClient()) SerialBT->println(__VA_ARGS__);	\
  }
#else
#define DBGPRINTF(...) debug_stream->printf(__VA_ARGS__)
#define DBGPRINTLN(...) debug_stream->println(__VA_ARGS__)
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

#define OLEDLINE(l,s,...) { 	\
  char buf[65];		 \
  snprintf(buf, sizeof(buf), __VA_ARGS__);	\
  __LEAF_DEBUG_PRINT__(__func__,__FILE__,__LINE__,"ACTION",L_WARN,"%s",buf); \
  OLED_TEXT(0, l*11, buf);			\
  }

//#define BANNER(...)  ALERT(__VA_ARGS__); OLEDLINE(0,__VA_ARGS__)
#define STATUS(...) OLEDLINE(0,"STATUS",__VA_ARGS__)
#define ACTION(...) OLEDLINE(1,"ACTION",__VA_ARGS__)
#define ERROR(...)  OLEDLINE(2,"ERROR",__VA_ARGS__)


#ifdef SYSLOG_flag

#ifndef SYSLOG_host
#define SYSLOG_host "notused"
#ifndef SYSLOG_IP
#define SYSLOG_IP 255,255,255,255
#endif
#endif

const unsigned int SYSLOG_port = 514;
WiFiUDP UDP;

void _udpsend(const char *dst, unsigned int port, const char *buf, unsigned int len)
{
  //Serial.print("udpsend "); Serial.print(dst); Serial.print(":");Serial.print(port);Serial.print(" => "); Serial.println(buf);

  static bool udpready = false;
#ifdef SYSLOG_IP
  static IPAddress syslogIP(SYSLOG_IP);
#else
  static IPAddress syslogIP;
#endif
  if (!udpready) {
    if (UDP.begin(SYSLOG_port)) {
      udpready = true;
    }
    else {
      return;
    }
#ifndef SYSLOG_IP
    WiFi.hostByName(dst, syslogIP);
#endif
  }
  UDP.beginPacket(syslogIP, port);
  UDP.write((uint8_t *)buf, len);
  UDP.endPacket();
}


#define SYSLOG(l,...)  \
  if (SYSLOG_flag) {	\
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
    snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, "%s %6s @%s:L%d ", device_id, _level_str(l), __PRETTY_FUNCTION__, (int)__LINE__); \
    offset = strlen(syslogbuf); \
    snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, __VA_ARGS__); \
    _udpsend(SYSLOG_host, SYSLOG_port, syslogbuf, strlen(syslogbuf)); \
  }

#else
#define SYSLOG(l,...) {}
#endif

void __LEAF_DEBUG_PRINT__(const char *func,const char *file, int line, const char *leaf_name, int l, const char *fmt, ...) 
{
  va_list ap;
  va_start(ap, fmt);
  char buf[160];
  char name_buf[16];
  char loc_buf[64];
  if (!func) func="unk";
  if (!file) file="/unk";
  snprintf(name_buf, sizeof(name_buf), "[%s]", leaf_name);
  unsigned long now =millis();
  DBGPRINTF("#%4d.%03d %6s %-12s ", (int)now/1000, (int)now%1000, _level_str(l), name_buf);
  if (debug_lines) {
    snprintf(loc_buf, sizeof(loc_buf), "%s(%d) ", func, (int)line);
    DBGPRINTF("%-50s ", loc_buf);
  } else if (debug_files) {
    snprintf(loc_buf, sizeof(loc_buf), "%s@%s:%d ", func, strrchr(file,'/')+1, line);
    DBGPRINTF("%-50s ", loc_buf);
  }
  if (debug_wait>0) delay(debug_wait);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  DBGPRINTLN(buf);
  if (debug_flush) debug_stream->flush();
}

#define __LEAF_DEBUG__(l,...) { if(debug_level>=(l)) {__LEAF_DEBUG_PRINT__(__func__,__FILE__,__LINE__,get_name_str(),(l),__VA_ARGS__);}}
#define __LEAF_DEBUG_AT__(loc,l,...) { if(debug_level>=(l)) {__LEAF_DEBUG_PRINT__((loc).func,(loc).file,(loc).line,get_name_str(),(l),__VA_ARGS__);}}
#define __DEBUG__(l,...) { if(debug_level>=(l)) {__LEAF_DEBUG_PRINT__(__func__,__FILE__,__LINE__,"",(l),__VA_ARGS__);}}
#define __DEBUG_AT__(loc,l,...) { if(debug_level>=(l)) {__LEAF_DEBUG_PRINT__((loc).func,(loc).file,(loc).line,"",(l),__VA_ARGS__);}}

#define ENTER(l)  int enterlevel=l; if (debug_level>=l) __DEBUG__(l,">%s", __func__)
#define LEAVE  __DEBUG__(enterlevel,"<%s", __func__)
#define RETURN(x) LEAVE; return(x)


#define ALERT( ...) __DEBUG__(L_ALERT ,__VA_ARGS__)

#if MAX_DEBUG_LEVEL >= L_WARN
#define WARN(  ...) __DEBUG__(L_WARN  ,__VA_ARGS__)
#else
#define WARN(...) {}
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

#define LEAF_ENTER(l)  int enterlevel=l; unsigned long entertime=millis(); if (debug_level>=l) {__LEAF_DEBUG__(l,">%s", __func__)}
#define LEAF_ENTER_BOOL(l,b)  int enterlevel=l; unsigned long entertime=millis(); if (debug_level>=l) {__LEAF_DEBUG__(l,">%s(%s)", __func__, TRUTH(b))}
#define LEAF_ENTER_STATE(l,b)  int enterlevel=l; unsigned long entertime=millis(); if (debug_level>=l) {__LEAF_DEBUG__(l,">%s(%s)", __func__, STATE(b))}
#define LEAF_ENTER_INT(l,i)  int enterlevel=l; unsigned long entertime=millis(); if (debug_level>=l) {__LEAF_DEBUG__(l,">%s(%d)", __func__, (i))}
#define LEAF_ENTER_LONG(l,i)  int enterlevel=l; unsigned long entertime=millis(); if (debug_level>=l) {__LEAF_DEBUG__(l,">%s(%lu)", __func__, (i))}
#define LEAF_ENTER_STR(l,s)  int enterlevel=l; unsigned long entertime=millis(); if (debug_level>=l) {__LEAF_DEBUG__(l,">%s(%s)", __func__, (s).c_str())}

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
#else
#define LEAF_INFO(...) {}
#define LEAF_INFO_AT(...) {}
#endif

#if MAX_DEBUG_LEVEL >= L_DEBUG
#define LEAF_DEBUG(...)  __LEAF_DEBUG__(L_DEBUG  ,__VA_ARGS__)
#define LEAF_DEBUG_AT(loc, ...)  __LEAF_DEBUG_AT__((loc), L_DEBUG  ,__VA_ARGS__)
#else
#define LEAF_DEBUG(...) {}
#define LEAF_DEBUG_AT(...) {}
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

void DumpHex(int level, const char *leader, const void* data, size_t size) {
	if (debug_level < level) return;
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

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
