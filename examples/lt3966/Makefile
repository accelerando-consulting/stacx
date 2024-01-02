BOARD ?= espressif:esp32:esp32c3
ifeq ($(BOARD),espressif:esp32:esp32c3)
BOARD_OPTIONS ?= CDCOnBoot=cdc
endif
STACX_DIR=../..
PARTITION_SCHEME ?= min_spiffs
BAUD=921600
ARCHIVE=n
SRCS=lt3966.ino defaults.h config.h $(STACX_DIR)/leaf_lt3966.h
include $(STACX_DIR)/cli.mk

