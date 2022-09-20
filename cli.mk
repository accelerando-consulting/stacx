#
# Default configuration (override these in your Makefile if needed)
#
BOARD ?= espressif:esp32:esp32
FQBN := $(BOARD)
BAUD ?= 921600

ifneq ($(PARTITION_SCHEME),)
ifeq ($(BOARD_OPTIONS),)
BOARD_OPTIONS := PartitionScheme=$(PARTITION_SCHEME)
else
BOARD_OPTIONS := $(BOARD_OPTIONS),PartitionScheme=$(PARTITION_SCHEME)
endif
endif

ifneq (($FQBN),espressif:esp32:esp32cam)
ifeq ($(BOARD_OPTIONS),)
BOARD_OPTIONS := UploadSpeed=$(BAUD)
else
BOARD_OPTIONS := $(BOARD_OPTIONS),UploadSpeed=$(BAUD)
endif
endif

ifneq ($(BOARD_OPTIONS),)
BOARD := $(BOARD):$(BOARD_OPTIONS)
endif

MONITOR_BAUD ?= 115200
CHIP ?= $(shell echo $(BOARD) | cut -d: -f2)
LIBDIR ?= $(HOME)/Documents/Arduino/libraries
SDKVERSION ?= $(shell ls -1 $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/ | tail -1)
ifeq ($(PROXYHOST),)
ifeq ($(CHIP),esp8266)
ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool/esptool.py
OTAPROG ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/espota.py
else
#ESPTOOL ?= $(HOME)/Arduino/hardware/espressif/$(CHIP)/tools/esptool.py
ESPTOOL ?= $(HOME)/Arduino/hardware/espressif/$(CHIP)/tools/esptool/esptool
OTAPROG ?= $(HOME)/Arduino/hardware/espressif/$(CHIP)/tools/espota.py
#ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool.py
endif
else
# Remote helper system
ESPTOOL ?= esptool
OTAPROG ?= espota
endif
MONITOR ?= miniterm

OTAPASS ?= changeme
PROGRAM ?= $(shell basename $(PWD))
#BUILD_NUMBER ?= $(shell grep '^#define BUILD_NUMBER' config.h | awk '{print $$3}')

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

#
# Make targets
#

build: $(OBJ)

$(OBJ): $(SRCS) Makefile
	@rm -f $(BINDIR)/compile_commands.json # workaround arduino-cli bug 1646
	arduino-cli compile -b $(BOARD) $(BUILDPATH) --libraries $(LIBDIR) $(CCFLAGS) --build-property "compiler.cpp.extra_flags=$(CPPFLAGS)" $(MAIN)
ifneq ($(ARCHIVE),n)
	zip -qr $(ARCHOBJ) $(BINDIR)
endif

increment-build:
	@if [ -e scripts/increment_build ] ; then scripts/increment_build config.h ; else stacx/scripts/increment_build config.h ; fi
	@grep define.BUILD_NUMBER config.h

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
else
	@#scp $(OBJ) $(BOOTOBJ) $(PARTOBJ) $(PROXYHOST):tmp/$(PROGRAM).ino.bin
	@ssh $(PROXYHOST) mkdir -p tmp/$(PROGRAM)/build
	rsync -avP --delete --exclude "**/*.elf" --exclude "**/*.map" build/ $(PROXYHOST):tmp/$(PROGRAM)/build/
endif

program: #$(OBJ)
ifeq ($(PROXYHOST),)
ifeq ($(PROGRAMMER),esptool)
	$(ESPTOOL) --port $(PORT) --baud $(BAUD) $(ESPTOOL_OPTIONS) write_flash --flash_size detect 0x10000 $(OBJ)
else
	arduino-cli upload -b $(FQBN) --input-dir $(BINDIR) --port $(PORT) --board-options "$(BOARD_OPTIONS)"
endif
else
ifeq ($(PROGRAMMER),esptool)
	ssh -t $(PROXYHOST) $(ESPTOOL) -p $(PROXYPORT) --baud $(BAUD) $(ESPTOOL_OPTIONS) write_flash 0x10000 tmp/$(PROGRAM)/$(BINDIR)/$(PROGRAM).ino.bin
else
	ssh -t $(PROXYHOST) "cd tmp/$(PROGRAM) && arduino-cli upload -b $(FQBN) --input-dir $(BINDIR) --port $(PROXYPORT) --board-options \"$(BOARD_OPTIONS)\""
endif
endif

erase:
ifeq ($(PROXYHOST),)
	$(ESPTOOL) --port $(PORT) erase_flash
else
	ssh -t $(PROXYHOST) $(ESPTOOL) --port $(PROXYPORT) erase_flash
endif

fuse:
ifeq ($(PROXYHOST),)
	$(ESPEFUSE) --port $(PORT) set_flash_voltage 3.3V
else
	ssh -t $(PROXYHOST) $(ESPEFUSE) --port $(PROXYPORT) set_flash_voltage 3.3V
endif


monitor sho:
ifeq ($(PROXYHOST),)
ifeq ($(MONITOR),cu)
	cu -s $(MONITOR_BAUD) -l $(PORT) $(MONITOR_ARGS)
endif
ifeq ($(MONITOR),tio)
	tio -b $(MONITOR_BAUD) $(MONITOR_ARGS) $(PORT)
endif
ifeq ($(MONITOR),miniterm)
	miniterm --rts 0 --dtr 0 $(MONITOR_ARGS) $(PORT) $(MONITOR_BAUD)
endif
else
ifeq ($(MONITOR),cu)
	ssh -t $(PROXYHOST) cu -s $(MONITOR_BAUD) $(PROXYPORT) 
endif
ifeq ($(MONITOR),tio)
	ssh -t $(PROXYHOST) tio  -b $(MONITOR_BAUD) $(PROXYPORT) 
endif
ifeq ($(MONITOR),miniterm)
	ssh -t $(PROXYHOST) miniterm --raw --rts 0 --dtr 0 $(PROXYPORT) $(MONITOR_BAUD)
endif
endif

go: build upload program

gosho: go monitor

dist:
	scp $(OBJ) $(DISTHOST):$(DISTDIR)/$(PROGRAM)-build$(BUILD_NUMBER).bin
	scp $(BOOTOBJ) $(DISTHOST):$(DISTDIR)/$(PROGRAM)-build$(BUILD_NUMBER)-bootloader.bin
	scp $(PARTOBJ) $(DISTHOST):$(DISTDIR)/$(PROGRAM)-build$(BUILD_NUMBER)-partition.bin
	scp $(ARCHOBJ) $(DISTHOST):$(DISTDIR)/


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
	arduino-cli core list
	arduino-cli core list | grep ^esp8266:esp8266 >/dev/null || arduino-cli core install esp8266:esp8266
	arduino-cli core list | grep ^esp32:esp32 >/dev/null || arduino-cli core install esp32:esp32

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
	    echo "Installing $$lib" ; \
	    arduino-cli lib install "$$lib" ; \
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
	    echo "Clone $$repo => $$dir" ; \
	    cd $(LIBDIR) && git clone $$repo $$dir ; \
          fi ; \
	done

installdeps: installcore libs extralibs
