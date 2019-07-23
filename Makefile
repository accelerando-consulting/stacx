#BOARD ?= esp8266:esp8266:generic:eesz=1M64,baud=115200
#BOARD ?= esp8266:esp8266:d1_mini:eesz=4M2M
BOARD ?= esp32:esp32:esp32
DEVICE ?= stacx
PORT ?= /dev/ttyUSB0
#PORT ?= tty.Repleo-CH341-00001114
CHIP ?= esp32
LIBDIR ?= $(HOME)/Arduino/libraries
SDKVERSION ?= $(shell ls -1 $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/ | tail -1)
OTAPROG ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/espota.py
ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool.py
OTAPASS ?= changeme
PROGRAM ?= stacx
CCFLAGS ?=
#CCFLAGS ?= --verbose --warnings all
MAIN = $(PROGRAM).ino
OBJ = $(PROGRAM).ino.bin
SRCS = $(MAIN) accelerando_trace.h wifi.h mqtt.h leaf.h config.h leaves.h leaf_*.h 

LIBS = "Adafruit NeoPixel" "Adafruit SGP30 Sensor" ArduinoJson Bounce2 "ESP8266 and ESP32 Oled Driver for SSD1306 display" ModbusMaster PID "SparkFun SCD30 Arduino Library" Time NtpClientLib
EXTRALIBS = https://github.com/me-no-dev/ESPAsyncTCP.git%ESPAsyncTCP https://github.com/marvinroger/async-mqtt-client.git%async-mqtt-client https://github.com/xreef/DHT12_sensor_library%DHT12_sensor_library https://github.com/me-no-dev/ESPAsyncUDP.git%ESPAsyncUDP https://github.com/ozbotics/WIFIMANAGER-ESP32%WIFIMANAGER-ESP32 https://github.com/spacehuhn/SimpleMap%SimpleMap

build: $(OBJ)

$(OBJ): $(SRCS) Makefile
	arduino-cli compile -b $(BOARD) --build-cache-path . $(CCFLAGS) -o $< $(MAIN)

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

monitor:
	#cu -s 115200 -l $(PORT)
	miniterm --rts 0 --dtr 0 $(PORT) 115200

go: build upload

gosho: go monitor

installcore: cliconfig
	@[ -f $(GOPATH)/bin/arduino-cli ] || go get -v -u github.com/arduino/arduino-cli && arduino-cli core update-index
	@arduino-cli core list | grep ^esp8266:esp8266 >/dev/null || arduino-cli core install esp8266:esp8266

cliconfig:
	@ [ -d $(GOPATH) ] || mkdir -p $(GOPATH)
	@ [ -d $(GOPATH)/bin ] || mkdir -p $(GOPATH)/bin
	@ [ -d $(GOPATH)/src ] || mkdir -p $(GOPATH)/src
	@if [ \! -f $(GOPATH)/bin/.cli-config.yml ] ; then \
	echo "board_manager:" >>$(GOPATH)/bin/.cli-config.yml ; \
	echo "  additional_urls:" >>$(GOPATH)/bin/.cli-config.yml ; \
	echo "    - http://arduino.esp8266.com/stable/package_esp8266com_index.json" >>$(GOPATH)/bin/.cli-config.yml ; \
	echo "    - https://dl.espressif.com/dl/package_esp32_index.json" >>$(GOPATH)/bin/.cli-config.yml ; \
	fi

libs:
	@for lib in $(LIBS) ; do libdir=`echo "$$lib" | sed -e 's/ /_/g'` ; if [ -d "$(LIBDIR)/$$libdir" ] ; then true ; else echo "Installing $$lib" ; arduino-cli lib install "$$lib" ; fi ; done

extralibs:
	@[ -d $(LIBDIR) ] || mkdir -p $(LIBDIR)
	@for lib in $(EXTRALIBS) ; do repo=`echo $$lib | cut -d% -f1` ; dir=`echo $$lib | cut -d% -f2`; if [ -d "$(LIBDIR)/$$dir" ] ; then echo "Found $$dir" ; else echo "Clone $$repo => $$dir" ; cd $(LIBDIR) && git clone $$repo $$dir ; fi ; done

installdeps: installcore libs extralibs
