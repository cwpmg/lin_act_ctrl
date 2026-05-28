# Cosmic stm8 simple Makefile

ECHO := echo
MKDIR := mkdir -p
RM := rm -rf

# ------------------------------------------------------
# Target 

TARGET_NAME := stm8_test

DEVICE := STM8S103
FLASH_CHIP := STM8S103F3

# ------------------------------------------------------
# PATH

STVP_PATH := C:\Program Files (x86)\STMicroelectronics\st_toolset\stvp
COSMIC_PATH := C:\Program Files (x86)\COSMIC\FSE_Compilers\CXSTM8

SRC_DIR := ./src
BUILD_DIR := ./build
BIN_DIR := ./bin

SPL_SRC_DIR := ./spl/src
SPL_INC_DIR := ./spl/inc

# ------------------------------------------------------
# cxstm8

CC := cxstm8
AS := castm8
LD := clnk
CHEX := chex

# ------------------------------------------------------
# Sources, Objects, .h Dependencies

PRJ_SRCS := $(wildcard ${SRC_DIR}/*.c)

SPL_SRCS := $(SPL_SRC_DIR)/stm8s_gpio.c $(SPL_SRC_DIR)/stm8s_clk.c $(SPL_SRC_DIR)/stm8s_flash.c $(SPL_SRC_DIR)/stm8s_tim4.c

SPL_OBJS := $(patsubst %.c,${BUILD_DIR}/%.o,$(notdir ${SPL_SRCS}))

PRJ_OBJS := $(patsubst %.c,${BUILD_DIR}/%.o,$(notdir ${PRJ_SRCS}))

H_FILES_DEPS := $(wildcard ${SRC_DIR}/*.h)

# ------------------------------------------------------
# stm8 Options

INCLUDES := -i"$(COSMIC_PATH)\Hstm8" -i"$(SRC_DIR)" -i"$(SPL_INC_DIR)"
DEFINES := -d$(DEVICE) 

C_FLAGS := -l +modsl0 +split +warn -f cxstm8.cfg $(INCLUDES) $(DEFINES) -co "$(BUILD_DIR)"
LD_FLAGS := -sa -l"$(COSMIC_PATH)\Lib" -l"$(BUILD_DIR)" -m $(BIN_DIR)/$(TARGET_NAME).map
ASFLAGS := -l

# ------------------------------------------------------
# bin files

S19_FILE := $(BIN_DIR)/$(TARGET_NAME).s19
HEX_FILE := $(BIN_DIR)/$(TARGET_NAME).hex

# ------------------------------------------------------

all: $(BUILD_DIR) $(BIN_DIR) $(S19_FILE)

$(BUILD_DIR) :
	$(MKDIR) $(BUILD_DIR)

$(BIN_DIR) :
	$(MKDIR) $(BIN_DIR)

$(S19_FILE): $(SPL_OBJS) $(PRJ_OBJS)
	$(ECHO) "$(PRJ_OBJS)" > $(BUILD_DIR)/objs_to_lkf_file.obl
	$(ECHO) "$(SPL_OBJS)" >> $(BUILD_DIR)/objs_to_lkf_file.obl
	$(LD) $(LD_FLAGS) -o $(BUILD_DIR)/$(TARGET_NAME).sm8 stm8s103.lkf
	$(CHEX) -fm -o $(S19_FILE) $(BUILD_DIR)/$(TARGET_NAME).sm8	
	$(CHEX) -fi -o $(HEX_FILE) $(BUILD_DIR)/$(TARGET_NAME).sm8	

$(SPL_OBJS): $(BUILD_DIR)/%.o : $(SPL_SRC_DIR)/%.c
	$(CC) $(C_FLAGS) $<

$(PRJ_OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(H_FILES_DEPS)
	$(CC) $(C_FLAGS) $<

flash: all
	"$(STVP_PATH)\STVP_CmdLine" -Device=$(FLASH_CHIP) -no_log -no_loop -FileProg=$(S19_FILE)
#	"$(STVP_PATH)\STVP_CmdLine" -Device=STM8S003F3 -no_log -no_loop -FileProg=$(HEX_FILE) -FileData=$(HEX_FILE)

rebuild: clean all

clean:
	$(RM) $(BUILD_DIR)/*
	$(RM) $(BIN_DIR)/*

# PHONY targets don't create output files with the same name as target
.PHONY: all flash clean
