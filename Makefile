PARTITION_SCHEME ?= min_spiffs
BOARD ?= espressif:esp32:ttgo-t7-v13-mini32

PROGRAM ?= $(shell basename $(PWD))
MAIN = $(PROGRAM).ino
SRCS = $(MAIN) \
	accelerando_trace.h \
	oled.h \
	leaf.h \
	leaves.h \
	abstract*.h \
	leaf_*.h \
	app_*.h 

# LIBS are the libraries you can install through the arduino library manager
# Format is LIBNAME[@VERSION]
# The cli.mk will also look for libs that correspond to "#pragma STACX_LIB <libname>" directives in leaf files
LIBS =  ArduinoJson@6.19.2 \
	"ESP8266 and ESP32 Oled Driver for SSD1306 displays" \
	Time \
	NtpClientLib

# EXTRALIBS are the libraries that are not in the arduino library manager, but installed via git
# Format is LIBNAME@REPOURL
# The cli.mk will also look for libs that correspond to "#pragma STACX_EXTRALIB <libspec>" directives in leaf files
EXTRALIBS = AsyncTCP@https://github.com/me-no-dev/AsyncTCP.git \
	ESPAsyncTCP@https://github.com/me-no-dev/ESPAsyncTCP.git \
	async-mqtt-client@https://github.com/marvinroger/async-mqtt-client.git \
	SimpleMap@https://github.com/spacehuhn/SimpleMap \

include cli.mk

