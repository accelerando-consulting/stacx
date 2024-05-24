BOARD ?= espressif:esp32:ttgo-t7-v13-mini32
#BOARD ?= espressif:esp32:esp32c3
#BOARD ?= espressif:esp32:esp32
#BOARD ?= esp8266:esp8266:d1_mini_pro
ifeq ($(BOARD),espressif:esp32:esp32c3)
BOARD_OPTIONS ?= CDCOnBoot=cdc
endif

STACX_DIR=../..
PARTITION_SCHEME ?= min_spiffs
BAUD=460800
ARCHIVE=n
include $(STACX_DIR)/cli.mk

