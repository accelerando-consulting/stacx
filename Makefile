BOARD ?= esp8266:esp8266:d1_mini_pro
#BOARD ?= espressif:esp33:ttgo-t7-v13-mini32
#BOARD ?= espressif:esp32:esp32c3
#BAUD=230400
ifeq ($(BOARD),espressif:esp32:esp32c3)
BOARD_OPTIONS ?= CDCOnBoot=cdc
endif

SRCS = $(MAIN) *.h  

# LIBS are the library dependencies that you can install through the arduino library manager
# Format is LIBNAME[@VERSION]
LIBS = \
	"Adafruit NeoPixel" \
	ArduinoJson@6.11.0 \
	Time 

# EXTRALIBS are the library dependencies that are NOT in the arduino library manager,
# but can be installed via git.  
# Format is LIBNAME@REPOURL
EXTRALIBS = AsyncTCP@https://github.com/me-no-dev/AsyncTCP.git \
	async-mqtt-client@https://github.com/marvinroger/async-mqtt-client.git \
	WIFIMANAGER-ESP32@https://github.com/ozbotics/WIFIMANAGER-ESP32 \
	SimpleMap@https://github.com/spacehuhn/SimpleMap \
	Shell@https://github.com/accelerando-consulting/Shell.git 

include cli.mk

