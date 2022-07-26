#BOARD ?= esp8266:esp8266:generic:eesz=1M64,baud=115200
#BOARD ?= esp8266:esp8266:d1_mini_pro
#BOARD ?= esp8266:esp8266:d1_mini_pro:eesz=16M15M,baud=921600,xtal=80
#BOARD ?= esp8266:esp8266:d1_mini_pro:eesz=16M15M,baud=921600
#BOARD ?= espressif:esp32:esp32:PartitionScheme=min_spiffs
#BOARD ?= espressif:esp32:esp32:PartitionScheme=no_ota
#BOARD ?= espressif:esp32:esp32
#BOARD ?= esp32:esp32:esp32cam
BOARD ?= espressif:esp32:esp32cam
#BOARD ?= espressif:esp32:pico32
ifeq ($(BOARD),espressif:esp32:esp32cam)
PARTITION_SCHEME ?= min_spiffs
endif
ifneq ($(PARTITION_SCHEME),)
BOARD := $(BOARD):PartitionScheme=$(PARTITION_SCHEME)
endif
DEVICE ?= stacx00000001
PORT ?= /dev/tty.SLAB_USBtoUART
#PORT ?= tty.Repleo-CH341-00001114
#PROXYHOST ?= piranha
PROXYPORT ?= /dev/ttyUSB0

BUILD_NUMBER ?= $(shell grep BUILD_NUMBER config.h | awk '{print $$3}')

BAUD ?= 460800
CHIP ?= $(shell echo $(BOARD) | cut -d: -f2)
LIBDIR ?= $(HOME)/Documents/Arduino/libraries
SDKVERSION ?= $(shell ls -1 $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/ | tail -1)
ifeq ($(PROXYHOST),)
ifeq ($(CHIP),esp8266)
ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool/esptool.py
OTAPROG ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/espota.py
else
ESPTOOL ?= $(HOME)/Arduino/hardware/espressif/$(CHIP)/tools/esptool.py
OTAPROG ?= $(HOME)/Arduino/hardware/espressif/$(CHIP)/tools/espota.py
#ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool.py
endif
else
# Remote helper system
ESPTOOL ?= esptool
OTAPROG ?= espota
endif
ESPEFUSE ?= espefuse
x
OTAPASS ?= changeme
PROGRAM ?= stacx

DISTHOST ?= firmware
DISTDIR ?= firmware/stacx

#CCFLAGS ?=
CCFLAGS ?= --verbose --warnings default
#CCFLAGS ?= --verbose --warnings all
BINDIR ?= build/$(shell echo $(BOARD) | cut -d: -f1-3 | tr : .)
MAIN = $(PROGRAM).ino
OBJ = $(BINDIR)/$(PROGRAM).ino.bin
BOOTOBJ = $(BINDIR)/$(PROGRAM).ino.bootloader.bin
PARTOBJ = $(BINDIR)/$(PROGRAM).ino.partitions.bin
ARCHOBJ = $(PROGRAM)-build$(BUILD_NUMBER).zip

SRCS = $(MAIN) \
	stacx.h \
	accelerando_trace.h \
	oled.h \
	leaf.h \
	config.h \
	trait_*.h \
	abstract_*.h \
	leaf_*.h


# LIBS are the libraries you can install through the arduino library manager
# Format is LIBNAME[@VERSION]
LIBS = \
	ArduinoJson@6.11.0 \
	Time \
	NtpClientLib \
	JPEGDEC


# EXTRALIBS are the libraries that are not in the arduino library manager,
# but can be installed via git.  
# Format is LIBNAME@REPOURL
EXTRALIBS = AsyncTCP@https://github.com/me-no-dev/AsyncTCP.git \
	ESPAsyncTCP@https://github.com/me-no-dev/ESPAsyncTCP.git \
	async-mqtt-client@https://github.com/marvinroger/async-mqtt-client.git \
	ESPAsyncUDP@https://github.com/me-no-dev/ESPAsyncUDP.git \
	WIFIMANAGER-ESP32@https://github.com/ozbotics/WIFIMANAGER-ESP32 \
	SimpleMap@https://github.com/spacehuhn/SimpleMap \
	ESP32_FTPClient@https://github.com/accelerando-consulting/ESP32_FTPClient.git \
	Shell@https://github.com/accelerando-consulting/Shell.git \
	Adafruit_LIS3DH@https://github.com/accelerando-consulting/Adafruit_LIS3DH.git \
	SIM7000-LTE-Shield@https://github.com/DylanGWork/SIM7000.git \
	rBase64@https://github.com/accelerando-consulting/rBase64

include cli.mk

customlibs: extralibs
	#cd $$HOME/Documents/Arduino/libraries/SIM7000-LTE-Shield && mv README.md README-Top.md && ln -s Code/* .

