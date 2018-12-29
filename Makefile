BOARD ?= esp8266:esp8266:d1_mini
DEVICE ?= pod01
PORT ?= /dev/tty.SLAB_USBtoUART
LIBDIR ?= $(HOME)/Arduino/libraries
OTAPROG ?= $(HOME)/.arduino15/packages/esp8266/hardware/esp8266/2.4.2/tools/espota.py
OTAPASS ?= pamela
PROGRAM ?= pamela-pod

MAIN = $(PROGRAM).ino
OBJ = $(PROGRAM).ino.bin
SRCS = $(MAIN) wifi.h mqtt.h pod.h accelerando_trace.h credentials.h config.h pod.h 

LIBS = "Adafruit NeoPixel" ArduinoJson Bounce2 PubSubClient WiFiManager 
EXTRALIBS = 

build: $(OBJ)

$(OBJ): $(SRCS)
	arduino-cli compile -b $(BOARD) -o $< $(MAIN)

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

upload: $(OBJ)
	arduino-cli upload -b $(BOARD) -p $(PORT) -i $(OBJ) -v -t 

installcore: cliconfig
	@ [ -f $(GOPATH)/bin/arduino-cli ] || go get -v -u github.com/arduino/arduino-cli
	@ arduino-cli core list | grep ^esp8266:esp8266 >/dev/null || arduino-cli core install esp8266:esp8266

cliconfig:
	@if [ \! -f $(GOPATH)/bin/.cli-config.yml ] ; then \
	echo "board_manager:" >>$(GOPATH)/bin/.cli-config.yml ; \
	echo "  additional_urls:" >>$(GOPATH)/bin/.cli-config.yml ; \
	echo "    - http://arduino.esp8266.com/stable/package_esp8266com_index.json" >>$(GOPATH)/bin/.cli-config.yml ; \
	echo "    - https://dl.espressif.com/dl/package_esp32_index.json" >>$(GOPATH)/bin/.cli-config.yml ; \
	arduino-cli core update-index ; \
	fi

libs:
	@for lib in $(LIBS) ; do libdir=`echo "$$lib" | sed -e 's/ /_/g'` ; if [ -d "$(LIBDIR)/$$libdir" ] ; then true ; else echo "Installing $$lib" ; arduino-cli lib install "$$lib" ; fi ; done

extralibs:
	@[ -d $(LIBDIR) ] || mkdir -p $(LIBDIR)
	@for lib in $(EXTRALIBS) ; do repo=`echo $$lib | cut -d% -f1` ; dir=`echo $$lib | cut -d% -f2`; if [ -d "$$dir" ] ; then echo "Found $$dir" ; else echo "Clone $$repo => $$dir" ; cd $(LIBDIR) && git clone $$repo $$dir ; fi ; done

installdeps: installcore libs extralibs


