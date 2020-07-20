#BOARD ?= esp8266:esp8266:generic:eesz=1M64,baud=115200
BOARD ?= esp8266:esp8266:d1_mini_pro
#BOARD ?= esp32:esp32:esp32:PartitionScheme=min_spiffs
#BOARD ?= esp32:esp32:esp32
DEVICE ?= puc000001
PORT ?= /dev/ttyUSB0
#PORT ?= tty.Repleo-CH341-00001114
CHIP ?= $(shell echo $(BOARD) | cut -d: -f2)
LIBDIR ?= $(HOME)/Arduino/libraries
SDKVERSION ?= $(shell ls -1 $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/ | tail -1)
OTAPROG ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/espota.py
ifeq ($(CHIP),esp8266)
ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool/esptool.py
else
ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool.py
endif
OTAPASS ?= changeme
PROGRAM ?= animaltrap-puc

CCFLAGS ?=
#CCFLAGS ?= --verbose --warnings all
MAIN = $(PROGRAM).ino
OBJ = $(PROGRAM).ino.bin
SRCS = $(MAIN) \
	accelerando_trace.h \
	oled.h \
	leaf.h \
	config.h \
	leaves.h \
	leaf_*.h

# LIBS are the libraries you can install through the arduino library manager
# Format is LIBNAME[@VERSION]
LIBS = "Adafruit NeoPixel" \
	"Adafruit SGP30 Sensor" \
	ArduinoJson@6.11.0 \
	Bounce2 \
	DallasTemperature \
	"ESP8266 and ESP32 Oled Driver for SSD1306 display" \
	"Sharp GP2Y Dust Sensor" \
	ModbusMaster \
	OneWire \
	PID \
	"SparkFun SCD30 Arduino Library" \
	Time \
	NtpClientLib

# EXTRALIBS are the libraries that are not in the arduino library manager, but installed via git
# Format is LIBNAME@REPOURL
EXTRALIBS = AsyncTCP@https://github.com/me-no-dev/AsyncTCP.git \
	ESPAsyncTCP@https://github.com/me-no-dev/ESPAsyncTCP.git \
	async-mqtt-client@https://github.com/marvinroger/async-mqtt-client.git \
	DHT12_sensor_library@https://github.com/xreef/DHT12_sensor_library \
	ESPAsyncUDP@https://github.com/me-no-dev/ESPAsyncUDP.git \
	WIFIMANAGER-ESP32@https://github.com/ozbotics/WIFIMANAGER-ESP32 \
	SimpleMap@https://github.com/spacehuhn/SimpleMap \
        Adafruit_FONA@https://github.com/botletics/SIM7000-LTE-Shield.git

build: $(OBJ)

$(OBJ): $(SRCS) Makefile
	arduino-cli compile -b $(BOARD) --build-cache-path . $(CCFLAGS) -o $< $(MAIN)

clean:
	rm -f $(OBJ)

ota: $(OBJ)
	@if [ -z "$$IP" ] ; then \
		IP=`avahi-browse -ptr  "_arduino._tcp" | egrep ^= | cut -d\; -f4,8,9 | grep ^$$DEVICE | cut -d\; -f2` -p `avahi-browse -ptr  "_arduino._tcp" | egrep ^= | cut -d\; -f4,8,9 | grep ^$$DEVICE | cut -d\; -f3` ;\
	fi ;\
	python $(OTAPROG) -i $(IP) "--auth=$(OTAPASS)" -f $(OBJ)

find:
	@if [ `uname -s` = Darwin ] ; then \
		dns-sd -B _arduino._tcp ;\
	else \
		avahi-browse -ptr  "_arduino._tcp" | egrep ^= | cut -d\; -f4,8,9 ;\
	fi

upload: #$(OBJ)
	python $(ESPTOOL) --port $(PORT) write_flash 0x10000 $(OBJ)
#	arduino-cli upload -b $(BOARD) -p $(PORT) -i $(OBJ) -v -t

erase:
	python $(ESPTOOL) --port $(PORT) erase_flash

monitor:
#	cu -s 115200 -l $(PORT)
	miniterm --rts 0 --dtr 0 $(PORT) 115200

go: build upload

gosho: go monitor

include cli.mk

libs:
	@for lib in $(LIBS) ; \
	do libdir=`echo "$$lib" | sed -e 's/ /_/g'` ; \
	  if [ -d "$(LIBDIR)/$$libdir" ] ; \
	  then \
	    true ; \
	  else \
	    echo "Installing $$lib" ; \
	    arduino-cli lib install "$$lib" ; \
          fi ;\
        done

extralibs:
	@[ -d $(LIBDIR) ] || mkdir -p $(LIBDIR)
	@for lib in $(EXTRALIBS) ; \
	do repo=`echo $$lib | cut -d@ -f2` ; \
	  dir=`echo $$lib | cut -d@ -f1`; \
	  if [ -d "$(LIBDIR)/$$dir" ] ; \
	  then \
	    echo "Found $$dir" ; \
	  else \
	    echo "Clone $$repo => $$dir" ; \
	    cd $(LIBDIR) && git clone $$repo $$dir ; \
          fi ; \
	done

installdeps: installcore libs extralibs
