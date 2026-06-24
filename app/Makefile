PROJECT 	= led-test
BUILD_DIR 	= build
BIN_DIR 	= bin
SRC_DIR 	= src
INC_DIR 	= inc

SHARED_DIR = ../shared
CFILES = $(SRC_DIR)/firmware.c
# CFILES += api.c
# AFILES += api-asm.S

# TODO - you will need to edit these two lines!
DEVICE=stm32f401re
OOCD_FILE = target/stm32f4x.cfg

# You shouldn't have to edit anything below here.
VPATH += $(SHARED_DIR)
VPATH += $(INC_DIR)
INCLUDES += $(patsubst %,-I%, . $(SHARED_DIR))
INCLUDES += $(patsubst %,-I%, . $(INC_DIR))
OPENCM3_DIR= lib/libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk