BOARD ?= espressif:esp32:esp32c3
ifeq ($(BOARD),espressif:esp32:esp32c3)
BOARD_OPTIONS ?= CDCOnBoot=cdc
endif
STACX_DIR=../..
PARTITION_SCHEME ?= min_spiffs
BAUD=115200
ARCHIVE=n
include $(STACX_DIR)/cli.mk

