#include test/Makefile

VERBOSE=@

X86_COMPILER_PRE=
ARM_COMPILER_PRE=arm-linux-gnueabihf-
MIPS_COMPILER_PRE=mipsel-openwrt-linux-
CC=gcc
CFLAGS=-Wall -Werror -I../inc -I./inc
# CFLAGS += -pthread -I./include
LD=ld
# LDFLAGS=-L./out/ -static -llog_proxy
# LDFLAGS+=
AR=ar
STATIC_FLAGS=rc
SHARED_FLAGS=-fPIC -shared

SHARE_DIR=/home/sleng/win/share
OUT_DIR=./out

#BIN_TARGET=
SRCS=$(wildcard src/*.c)
SRCS_NODIR=$(notdir $(SRCS))

X86_OBJS=$(SRCS:%.c=%_x86.o)
ARM_OBJS=$(SRCS:%.c=%_arm.o)
MIPS_OBJS=$(SRCS:%.c=%_mips.o)
BIN_OBJS=$(X86_OBJS) $(ARM_OBJS) $(MIPS_OBJS)

X86_LIB=$(OUT_DIR)/libutil_x86.a
ARM_LIB=$(OUT_DIR)/libutil_arm.a
MIPS_LIB=$(OUT_DIR)/libutil_mips.a
LIB_TARGET=$(X86_LIB) $(ARM_LIB) $(MIPS_LIB)
# STATIC_LIB_TARGET=$(OUT_DIR)/.a
# SHARED_LIB_TARGET=$(OUT_DIR)/.so


# all:$(BIN_TARGET) $(LIB_TARGET) $(CLIENT_TARGET) $(INPUT_TARGET) $(OUTPUT_TARGET)
all:lib
lib:$(LIB_TARGET)
x86:$(X86_LIB)
arm:$(ARM_LIB)
mips:$(MIPS_LIB)
info:
	@echo $(SRCS)
	@echo $(X86_OBJS)
	@echo $(ARM_OBJS)
	@echo $(MIPS_OBJS)


$(X86_LIB):$(X86_OBJS)
	@mkdir -p out/x86
	@echo Generic $@ ...
	$(VERBOSE) $(X86_COMPILER_PRE)$(AR) $(STATIC_FLAGS) $@ $^

$(ARM_LIB):$(ARM_OBJS)
	@mkdir -p out/arm
	@echo Generic $@ ...
#	$(VERBOSE) $(ARM_COMPILER_PRE)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	# $(VERBOSE) $(ARM_COMPILER_PRE)$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
	$(VERBOSE) $(ARM_COMPILER_PRE)$(AR) $(STATIC_FLAGS) $@ $^

$(MIPS_LIB):$(MIPS_OBJS)
	@mkdir -p out/mips
	@echo Generic $@ ...
	$(VERBOSE) $(MIPS_COMPILER_PRE)$(AR) $(STATIC_FLAGS) $@ $^


$(X86_OBJS):%_x86.o:%.c
	@echo Compiling $@ from $< ...
	$(VERBOSE) $(X86_COMPILER_PRE)$(CC) $(CFLAGS) -o $@ -c $<

$(ARM_OBJS):%_arm.o:%.c
	@echo Compiling $@ from $< ...
	$(VERBOSE) $(ARM_COMPILER_PRE)$(CC) $(CFLAGS) -o $@ -c $<

$(MIPS_OBJS):%_mips.o:%.c
	@echo Compiling $@ from $< ...
	$(VERBOSE) $(MIPS_COMPILER_PRE)$(CC) $(CFLAGS) -o $@ -c $<


.PHONY:clean
clean:
	rm -rf $(BIN_TARGET) $(BIN_OBJS) $(LIB_TARGET) $(LIB_OBJS) $(CLIENT_TARGET) $(CLIENT_OBJS) $(INPUT_TARGET) $(OUTPUT_TARGET)
