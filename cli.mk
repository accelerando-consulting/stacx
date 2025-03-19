#
# Default configuration (override these in your Makefile if needed)
#
PROGRAM ?= $(shell basename $(PWD))
MAIN ?= $(PROGRAM).ino

DEFAULT_BOARD ?= espressif:esp32:esp32
BOARD ?= $(shell (grep '^#pragma STACX_BOARD ' $(MAIN) || echo "#pragma STACX_BOARD $(DEFAULT_BOARD)" ) | awk '{print $$3}' )
DEVICE_ID ?= $(PROGRAM)
FQBN ?= $(shell echo "$(BOARD)"| cut -d: -f1-3)
BAUD ?= 921600
CHIP ?= $(shell echo $(BOARD) | cut -d: -f2)
VARIANT ?= $(shell echo $(BOARD) | cut -d: -f3)

STACX_DIR ?= .

CORE_DEBUG ?= 0
ifeq ($(CORE_DEBUG),0)
BUILD_OPTIONS := 
else
BUILD_OPTIONS := --build-property "build.code_debug=$(CORE_DEBUG)"
endif

ifneq ($(BAUD),none)
ifeq ($(CHIP),esp32)
ifeq ($(BOARD_OPTIONS),)
BOARD_OPTIONS := UploadSpeed=$(BAUD)
else
BOARD_OPTIONS := $(BOARD_OPTIONS),UploadSpeed=$(BAUD)
endif
endif

ifneq ($(PARTITION_SCHEME),)
ifeq ($(BOARD_OPTIONS),)
BOARD_OPTIONS := PartitionScheme=$(PARTITION_SCHEME)
else
BOARD_OPTIONS := $(BOARD_OPTIONS),PartitionScheme=$(PARTITION_SCHEME)
endif
endif
endif

#ifneq ($(CORE_DEBUG),)
#ifeq ($(BOARD_OPTIONS),)
#BOARD_OPTIONS := DebugLevel=$(CORE_DEBUG)
#else
#BOARD_OPTIONS := $(BOARD_OPTIONS),DebugLevel=$(CORE_DEBUG)
#endif
#endif

ifneq ($(BOARD_OPTIONS),)
BOARD := $(BOARD):$(BOARD_OPTIONS)
endif

MONITOR_BAUD ?= 115200
LIBDIRS ?= $(HOME)/Documents/Arduino/libraries,$(PWD)/libraries
LIBDIR ?= $(shell echo "$(LIBDIRS)" | cut -d, -f1)
SDKVERSION ?= $(shell ls -1 $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/ | tail -1)
ESPEFUSE ?= espefuse.py
OTA_PORT ?= 3232
ifeq ($(PROXYHOST),)
ifeq ($(CHIP),esp8266)
ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool/esptool.py
OTAPROG ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/espota.py
else
ESPTOOL ?= $(HOME)/Arduino/hardware/espressif/$(CHIP)/tools/esptool/esptool.py
OTAPROG ?= $(HOME)/Arduino/hardware/espressif/$(CHIP)/tools/espota.py
#ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool.py
OPENOCD ?= openocd
endif
else
# Remote helper system
ESPTOOL ?= esptool
OTAPROG ?= espota
endif
ARDUINO_CLI ?= arduino-cli
MONITORHOST ?= $(PROXYHOST)
MONITOR ?= miniterm
MINITERM ?= miniterm

OTAPASS ?= changeme
BUILD_NUMBER ?= $(shell grep '^.define BUILD_NUMBER' config.h /dev/null 2>/dev/null | awk '{print $$3}' )

CCFLAGS ?= --verbose --warnings default
#CCFLAGS ?= --verbose --warnings all

BINDIR ?= build/$(shell echo $(BOARD) | cut -d: -f1-3 | tr : .)
#BINDIR ?= .
MAIN ?= $(PROGRAM).ino
OBJ ?= $(BINDIR)/$(PROGRAM).ino.bin
BOOTOBJ ?= $(BINDIR)/$(PROGRAM).ino.bootloader.bin
PARTOBJ ?= $(BINDIR)/$(PROGRAM).ino.partitions.bin
ARCHOBJ ?= $(PROGRAM)-build$(BUILD_NUMBER).zip
SRCS ?= $(MAIN)

PORT ?= $(shell ls -1 /dev/ttyUSB* /dev/tty.u* /dev/tty.SLAB* | head -1)
PROXYPORT ?= /dev/ttyUSB0

#BUILDPATH=--build-cache-path $(BINDIR) --build-path $(BINDIR)
BUILDPATH=--export-binaries

ifneq ($(DEVICE_ID),)
CPPFLAGS := -DDEVICE_ID=\"$(DEVICE_ID)\" $(CPPFLAGS)
endif

ifneq ($(STACX_DIR),)
CPPFLAGS := -I$(STACX_DIR) $(CPPFLAGS)
endif



#
# Make targets
#

build: $(OBJ)

$(OBJ): $(SRCS) Makefile
	@rm -f $(BINDIR)/compile_commands.json # workaround arduino-cli bug 1646
	$(ARDUINO_CLI) compile -b $(BOARD) $(BUILDPATH) --libraries $(LIBDIRS) $(CCFLAGS) $(BUILD_OPTIONS) --build-property "compiler.cpp.extra_flags=$(CPPFLAGS)" $(MAIN)
ifneq ($(ARCHIVE),n)
	zip -qr $(ARCHOBJ) $(BINDIR)
endif

pp: 
	$(ARDUINO_CLI) compile -b $(BOARD) $(BUILDPATH) --libraries $(LIBDIRS) $(CCFLAGS) --build-property "compiler.cpp.extra_flags=$(CPPFLAGS)" --preprocess $(MAIN)

config:
	@[ -e config.h ] || grep '^#include "config.h"' $(MAIN) >/dev/null && touch config.h || true

touch:
	touch $(MAIN)

debug: $(OBJ)
	$(ARDUINO_CLI) debug -b $(FQBN) --input-dir $(BINDIR) --port $(PORT) --board-options "$(BOARD_OPTIONS)"

increment-build:
	@[ -e $(STACX_DIR)/scripts/increment_build ] && $(STACX_DIR)/scripts/increment_build config.h 
	@grep define.BUILD_NUMBER config.h

inc: increment-build

clean:
	rm -f $(OBJ)

ota: $(OBJ)
	@if [ -z "$${IP}" ] ; then \
	IP=`avahi-browse -ptr  "_arduino._tcp" | egrep ^= | cut -d\; -f4,8,9 | grep ^$${DEVICE_ID} | cut -d\; -f2` ; fi ; \
	if [ -z "$${OTA_PORT}" ] ; then \
	OTA_PORT=`avahi-browse -ptr  "_arduino._tcp" | egrep ^= | cut -d\; -f4,8,9 | grep ^$${DEVICE_ID} | cut -d\; -f3` ; fi; \
	python $(OTAPROG) -i $${IP} -p $${OTA_PORT} "--auth=$(OTAPASS)" -f $(OBJ)

otai: ota increment-build

find:
	@if [ `uname -s` = Darwin ] ; then \
		dns-sd -B _arduino._tcp ;\
	else \
		avahi-browse -ptr  "_arduino._tcp" | egrep ^= | cut -d\; -f4,8,9 ;\
	fi

bootloader: $(BOOTOBJ)
ifeq ($(PROXYHOST),)
	python $(ESPTOOL) --port $(PORT) --baud $(BAUD) $(ESPTOOL_OPTIONS) write_flash 0x1000 $(BOOTOBJ)
else
	scp $(BOOTOBJ) $(PROXYHOST):tmp/$(PROGRAM).ino.bootloader.bin
	ssh -t $(PROXYHOST) $(ESPTOOL) -p $(PROXYPORT) --baud $(BAUD) $(ESPTOOL_OPTIONS) write_flash --flash_size detect 0x1000 tmp/$(PROGRAM).ino.bootloader.bin
endif

partition partitions: $(PARTOBJ)
ifeq ($(PROXYHOST),)
	python $(ESPTOOL) --port $(PORT) --baud $(BAUD) $(ESPTOOL_OPTIONS) write_flash 0x8000 $(PARTOBJ)
else
	scp $(PARTOBJ) $(PROXYHOST):tmp/$(PROGRAM).ino.partitions.bin
	ssh -t $(PROXYHOST) $(ESPTOOL) -p $(PROXYPORT) --baud $(BAUD) $(ESPTOOL_OPTIONS) write_flash --flash_size detect 0x8000 tmp/$(PROGRAM)/$(BINDIR)/$(PROGRAM).ino.partitions.bin
endif

upload: #$(OBJ)
ifeq ($(PROXYHOST),)
	@true
else ifeq ($(PROGRAMMER),openocd)
	@ssh $(PROXYHOST) mkdir -p tmp/$(PROGRAM)/build
	scp openocd.cfg $(PROXYHOST):tmp/$(PROGRAM)
	rsync -avP --delete --exclude "**/*.elf" --exclude "**/*.map" build/ $(PROXYHOST):tmp/$(PROGRAM)/build/
else
	@ssh $(PROXYHOST) mkdir -p tmp/$(PROGRAM)/build
ifeq ($(MONITOR),idf_monitor)
	rsync -avP --delete build/ $(PROXYHOST):tmp/$(PROGRAM)/build/
else
	rsync -avP --delete --exclude "**/*.elf" --exclude "**/*.map" build/ $(PROXYHOST):tmp/$(PROGRAM)/build/
endif
endif

prompt:
	say "Yo do the thing"

pass:
	say "You may pass"

fail:
	say "You.  Shall.  Not.  Pass"

program: #$(OBJ)
ifeq ($(PROXYHOST),)
ifeq ($(PROGRAMMER),esptool)
	$(ESPTOOL) --port $(PORT) --baud $(BAUD) $(ESPTOOL_OPTIONS) write_flash --flash_size detect 0x10000 $(OBJ)
else ifeq ($(PROGRAMMER),openocd)
	$(OPENOCD) -f openocd.cfg -c "program_esp $(OBJ) 0x10000 verify exit"
else
	$(ARDUINO_CLI) upload -b $(FQBN) --input-dir $(BINDIR) --port $(PORT) --board-options "$(BOARD_OPTIONS)"
endif
else
ifeq ($(PROGRAMMER),esptool)
	ssh -t $(PROXYHOST) $(ESPTOOL) -p $(PROXYPORT) --baud $(BAUD) $(ESPTOOL_OPTIONS) write_flash 0x10000 tmp/$(PROGRAM)/$(BINDIR)/$(PROGRAM).ino.bin
else ifeq ($(PROGRAMMER),openocd)
	ssh -t $(PROXYHOST) openocd -f tmp/$(PROGRAM)/openocd.cfg -c \"program_esp tmp/$(PROGRAM)/$(BINDIR)/$(PROGRAM).ino.bin 0x10000 verify exit\"
else
	ssh -t $(PROXYHOST) "cd tmp/$(PROGRAM) && $(ARDUINO_CLI) upload -b $(FQBN) --input-dir $(BINDIR) --port $(PROXYPORT) --board-options \"$(BOARD_OPTIONS)\""
endif
endif

erase:
ifeq ($(PROXYHOST),)
	$(ESPTOOL) --port $(PORT) erase_flash
else
	ssh -t $(PROXYHOST) $(ESPTOOL) --port $(PROXYPORT) erase_flash
endif

chipid:
ifeq ($(PROXYHOST),)
	$(ESPTOOL) --port $(PORT) chip_id
else
	ssh -t $(PROXYHOST) $(ESPTOOL) --port $(PROXYPORT) chip_id
endif

fuse:
ifeq ($(PROXYHOST),)
	$(ESPEFUSE) --port $(PORT) set_flash_voltage 3.3V
else
	ssh -t $(PROXYHOST) $(ESPEFUSE) --port $(PROXYPORT) set_flash_voltage 3.3V
endif


monitor sho:
ifeq ($(MONITORHOST),)
ifeq ($(MONITOR),cu)
	cu -s $(MONITOR_BAUD) -l $(PORT) $(MONITOR_ARGS) || true
endif
ifeq ($(MONITOR),tio)
	tio -b $(MONITOR_BAUD) $(MONITOR_ARGS) $(PORT) || true
endif
ifeq ($(MONITOR),idf_monitor)
	pushd ~/src/net/esp/esp-idf-latest-stable ; . ./export.sh ; popd ; cd $(BINDIR) ; python $$IDF_PATH/tools/idf_monitor.py -p $(PORT) -b $(MONITOR_BAUD) $(MONITOR_ARGS) $(PROGRAM).ino.elf
endif
ifeq ($(MONITOR),miniterm)
	while : ; do if [ -e $(PORT) ] ; then $(MINITERM) --raw --rts 0 --dtr 0 $(MONITOR_ARGS) $(PORT) $(MONITOR_BAUD) ; echo "miniterm exit $$?" ; else printf "\rwait for modem `date +%T`" ; fi ; sleep 1 ; done || true
	stty sane
endif
ifeq ($(MONITOR),minitermonce)
	$(MINITERM) --raw --rts 0 --dtr 0 $(MONITOR_ARGS) $(PORT) $(MONITOR_BAUD) || true
	stty sane
endif
else
ifeq ($(MONITOR),cu)
	ssh -t $(MONITORHOST) cu -s $(MONITOR_BAUD) $(PROXYPORT) || true
endif
ifeq ($(MONITOR),tio)
	ssh -t $(MONITORHOST) tio  -b $(MONITOR_BAUD) $(PROXYPORT)  || true
endif
ifeq ($(MONITOR),idf_monitor)
	ssh -t $(MONITORHOST) 'cd src/net/esp/esp-idf && . ./export.sh ; MONITOR_DIR=`echo "tmp/$(PROGRAM)/build/$(FQBN)" | tr : .` && cd $$HOME/$$MONITOR_DIR && pwd && python $$IDF_PATH/tools/idf_monitor.py -p $(PROXYPORT) -b $(MONITOR_BAUD) $(MONITOR_ARGS) $(PROGRAM).ino.elf'
endif
ifeq ($(MONITOR),miniterm)
	ssh -t $(MONITORHOST) 'while true ; do if [ -e $(PROXYPORT) ] ; then $(MINITERM) --raw --rts 0 --dtr 0 $(MONITOR_ARGS) $(PROXYPORT) $(MONITOR_BAUD) ; else printf \"\\rwait for modem \`date +%T\`\" ; fi ; sleep 1 ; done' || true
endif
endif

go: build upload program

gosho: go monitor

osho: ota monitor

psho: upload program monitor

igosho: increment-build gosho

tgosho: touch gosho

tigosho: touch igosho

goisho: go increment-build monitor


dist:
	@echo "$(BUILD_NUMBER)" >latest.txt
	scp $(OBJ) $(DISTHOST):$(DISTDIR)/$(PROGRAM)-build$(BUILD_NUMBER).bin
	scp $(BOOTOBJ) $(DISTHOST):$(DISTDIR)/$(PROGRAM)-build$(BUILD_NUMBER)-bootloader.bin
	scp $(PARTOBJ) $(DISTHOST):$(DISTDIR)/$(PROGRAM)-build$(BUILD_NUMBER)-partition.bin
ifneq ($(ARCHIVE),n)
	scp $(ARCHOBJ) $(DISTHOST):$(DISTDIR)/
endif
	scp $(OBJ) $(DISTHOST):$(DISTDIR)/$(PROGRAM).bin
	scp latest.txt $(DISTHOST):$(DISTDIR)/


bt backtrace stacktrace:
ifeq ($(CHIP),esp8266)
	java -jar $(HOME)/bin/EspStackTraceDecoder.jar $(HOME)/.arduino15/packages/esp8266/tools/xtensa-lx106-elf-gcc/3.0.4-gcc10.3-1757bed/bin/xtensa-lx106-elf-addr2line $(BINDIR)/$(PROGRAM).ino.elf /dev/stdin
else
	java -jar $(HOME)/bin/EspStackTraceDecoder.jar $(HOME)/src/arduino/hardware/espressif/esp32/tools/xtensa-esp32-elf/bin/xtensa-esp32-elf-addr2line $(BINDIR)/$(PROGRAM).ino.elf /dev/stdin
endif

installcli: 
	go install -v github.com/arduino/arduino-cli@latest && arduino-cli core update-index

installcore: cliconfig installcli
	cat arduino-cli.yaml && arduino-cli core update-index && ls -l ~/.arduino15
	$(ARDUINO_CLI) core list
	$(ARDUINO_CLI) core list | grep ^esp8266:esp8266 >/dev/null || $(ARDUINO_CLI) core install esp8266:esp8266
	$(ARDUINO_CLI) core list | grep ^esp32:esp32 >/dev/null || $(ARDUINO_CLI) core install esp32:esp32

cliconfig:
	 [ -d $(GOPATH) ] || mkdir -p $(GOPATH)
	 [ -d $(GOPATH)/bin ] || mkdir -p $(GOPATH)/bin
	 [ -d $(GOPATH)/src ] || mkdir -p $(GOPATH)/src
	if [ \! -f arduino-cli.yaml ] ; then \
	echo "board_manager:" >>arduino-cli.yaml ; \
	echo "  additional_urls:" >>arduino-cli.yaml ; \
	echo "    - http://arduino.esp8266.com/stable/package_esp8266com_index.json" >>arduino-cli.yaml ; \
	echo "    - https://dl.espressif.com/dl/package_esp32_index.json" >>arduino-cli.yaml ; \
	fi

libs:
	@for lib in $(LIBS) $(shell egrep -e 'include..(stacx/)?leaf_' $(MAIN) | cut -d\" -f2 | while read inc ; do [ -e $$inc ] && echo $$inc ; [ -e stacx/$$inc ] && echo stacx/$$inc ; done | xargs grep '^#pragma STACX_LIB ' | awk '{print $$3}') ;\
	do libdir=`echo "$$lib" | sed -e 's/ /_/g' -e 's/@.*//'`; \
	  if [ -d "$(LIBDIR)/$$libdir" ] ; \
	  then \
	    true ; \
	  else \
	    echo "Installing $$lib to $(LIBDIR)" ; \
	    $(ARDUINO_CLI) lib install "$$lib" ; \
          fi ;\
        done

extralibs:
	@[ -e $(LIBDIR) ] || mkdir -p $(LIBDIR)
	@for lib in $(EXTRALIBS) $(shell grep include..leaf_ $(MAIN) | cut -d\" -f2 | while read inc ; do [ -e $$inc ] && echo $$inc ; [ -e stacx/$$inc ] && echo stacx/$$inc ; done | xargs grep '^#pragma STACX_EXTRALIB ' /dev/null | awk '{print $3}') ;\
	do repo=`echo $$lib | cut -d@ -f2-` ; \
	  dir=`echo $$lib | cut -d@ -f1`; \
	  if [ -d "$(LIBDIR)/$$dir" ] ; \
	  then \
	    true ; \
	  else \
	    echo "Clone $$repo => $(LIBDIR)/$$dir" ; \
	    cd $(LIBDIR) && git clone $$repo $$dir ; \
          fi ; \
	done

installdeps: installcore libs extralibs
