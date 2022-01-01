#BOARD ?= esp8266:esp8266:generic:eesz=1M64,baud=115200
BOARD ?= esp8266:esp8266:d1_mini_pro
#BOARD ?= esp8266:esp8266:d1_mini_pro:eesz=16M15M,baud=921600,xtal=80
#BOARD ?= esp8266:esp8266:d1_mini_pro:eesz=16M15M,baud=921600
#BOARD ?= espressif:esp32:esp32:PartitionScheme=min_spiffs
#BOARD ?= espressif:esp32:esp32
DEVICE ?= stacx00000001
PORT ?= /dev/tty.SLAB_USBtoUART
#PORT ?= tty.Repleo-CH341-00001114
#PROXYHOST ?= tweety
#PROXYPORT ?= /dev/ttyUSB0
BAUD ?= 460800
CHIP ?= $(shell echo $(BOARD) | cut -d: -f2)
LIBDIR ?= $(HOME)/Arduino/libraries
SDKVERSION ?= $(shell ls -1 $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/ | tail -1)
ifeq ($(CHIP),esp8266)
ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool/esptool.py
OTAPROG ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/espota.py
else
ESPTOOL ?= $(HOME)/Arduino/hardware/espressif/$(CHIP)/tools/esptool.py
OTAPROG ?= $(HOME)/Arduino/hardware/espressif/$(CHIP)/tools/espota.py
#ESPTOOL ?= $(HOME)/.arduino15/packages/$(CHIP)/hardware/$(CHIP)/$(SDKVERSION)/tools/esptool.py
endif
OTAPASS ?= changeme
PROGRAM ?= stacx

#CCFLAGS ?=
CCFLAGS ?= --verbose --warnings default
#CCFLAGS ?= --verbose --warnings all
BINDIR ?= build/$(shell echo $(BOARD) | cut -d: -f1-3 | tr : .)
MAIN = $(PROGRAM).ino
OBJ = $(BINDIR)/$(PROGRAM).ino.bin
BOOTOBJ = $(BINDIR)/$(PROGRAM).ino.bootloader.bin
PARTOBJ = $(BINDIR)/$(PROGRAM).ino.partitions.bin
SRCS = $(MAIN) \
	accelerando_trace.h \
	oled.h \
	leaf.h \
	config.h \
	leaves.h \
	abstract*.h \
	leaf_*.h \
	app_*.h 

# LIBS are the libraries you can install through the arduino library manager
# Format is LIBNAME[@VERSION]
LIBS = "Adafruit NeoPixel" \
	ArduinoJson@6.14.0 \
	Bounce2 \
	DallasTemperature \
	"ESP8266 and ESP32 Oled Driver for SSD1306 display" \
	ModbusMaster \
	OneWire \
	PID \
	Time \
	NtpClientLib

#	"Adafruit SGP31 Sensor" \
#	"SparkFun SCD30 Arduino Library" \
#	"Sharp GP2Y Dust Sensor" \

# EXTRALIBS are the libraries that are not in the arduino library manager, but installed via git
# Format is LIBNAME@REPOURL
EXTRALIBS = AsyncTCP@https://github.com/me-no-dev/AsyncTCP.git \
	ESPAsyncTCP@https://github.com/me-no-dev/ESPAsyncTCP.git \
	async-mqtt-client@https://github.com/marvinroger/async-mqtt-client.git \
	DHT12_sensor_library@https://github.com/xreef/DHT12_sensor_library \
	ESPAsyncUDP@https://github.com/me-no-dev/ESPAsyncUDP.git \
	SimpleMap@https://github.com/spacehuhn/SimpleMap \
	ModbusSlave@https://github.com/yaacov/ArduinoModbusSlave
#	WIFIMANAGER-ESP32@https://github.com/ozbotics/WIFIMANAGER-ESP32 \

include cli.mk

