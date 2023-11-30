#BOARD ?= espressif:esp32:corinda
#PARTITION_SCHEME ?= min_spiffs
#BOARD_OPTIONS ?= CDCOnBoot=cdc

BOARD ?= rp2040:rp2040:alitrax
BOARD_OPTIONS ?= flash=2097152_262144,usbstack=tinyusb,dbgport=Serial,dbglvl=SPI
MONITOR_DTR=1

STACX_DIR=../..
DEVICE_ID=max3140

ARCHIVE=n
include $(STACX_DIR)/cli.mk

