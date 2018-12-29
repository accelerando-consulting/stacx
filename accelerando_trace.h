/* 
 * Macros for consistent use of trace output
 *
 * You should define DBG to be the stream you want to use (default: Serial)
 */
#include "config.h"

#ifndef DBG
#define DBG Serial
#endif
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL L_ALERT
#endif


#define DBGMILLIS { \
    DBG.printf("%8lu %s(%d) ", millis(), __FUNCTION__, __LINE__);	\
  }

#define DBGINDENT DBG.print("\t")

#define L_ALERT 0
#define L_NOTICE 1
#define L_INFO 2
#define L_DEBUG 3

byte debug = DEBUG_LEVEL;

#ifdef SYSLOG_flag

const unsigned int SYSLOG_port = 514;
WiFiUDP UDP;

void _udpsend(char *dst, unsigned int port, char *buf, unsigned int len)
{
  //Serial.print("udpsend "); Serial.print(dst); Serial.print(":");Serial.print(port);Serial.print(" => "); Serial.println(buf);

  static bool udpready = false;
  static IPAddress syslogIP;
  if (!udpready) {
    if (UDP.begin(SYSLOG_port)) {
      udpready = true;
    }
    else {
      return;
    }
    WiFi.hostByName(dst, syslogIP);
  }
  UDP.beginPacket(syslogIP, port);
  UDP.write(buf, len);
  UDP.endPacket();
}


#define SYSLOG(l,...) \
  if (SYSLOG_flag) { \
    char syslogbuf[512]; \
    int facility = 1; \
    int severity; \
    int offset = 0; \
    time_t now; \
    struct tm *localtm; \
    switch (l) { \
    case L_ALERT: severity=1; break; \
    case L_NOTICE: severity=5; break; \
    case L_INFO: severity=6; break; \
    default: severity=7; \
    } \
    snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, "<%d>", (facility<<3)+severity); \
    offset = strlen(syslogbuf); \
    time(&now); \
    localtm = localtime(&now); \
    strftime(syslogbuf+offset, sizeof(syslogbuf)-offset, "%b %e %T ", localtm); \
    offset = strlen(syslogbuf); \
    strlcpy(syslogbuf+offset, device_id, sizeof(syslogbuf)-offset); \
    offset = strlen(syslogbuf); \
    strlcpy(syslogbuf+offset, " ", sizeof(syslogbuf)-offset); \
    offset = strlen(syslogbuf); \
    snprintf(syslogbuf+offset, sizeof(syslogbuf)-offset, __VA_ARGS__); \
    _udpsend(SYSLOG_host, SYSLOG_port, syslogbuf, strlen(syslogbuf)); \
  }

#else
#define SYSLOG(l,...) {}
#endif  
   
#define ENTER(l)  int enterlevel=l; if (debug>=l) __DEBUG__(l,">%s\n", __func__)
#define LEAVE  __DEBUG__(enterlevel,"<%s\n", __func__)
#define __DEBUG__(l,...) {if(debug>=l){DBGMILLIS; DBG.printf(__VA_ARGS__); DBG.println();SYSLOG(l,__VA_ARGS__)}}
#define ALERT( ...) __DEBUG__(L_ALERT ,__VA_ARGS__)
#define NOTICE(...) __DEBUG__(L_NOTICE,__VA_ARGS__)
#define INFO(...)   __DEBUG__(L_INFO  ,__VA_ARGS__)
#define DEBUG(...)  __DEBUG__(L_DEBUG ,__VA_ARGS__)
#define STATE(s) ((s==LOW)?"LOW":"HIGH")
#define TRUTH(b) ((b==0)?"FALSE":"TRUE")


// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
