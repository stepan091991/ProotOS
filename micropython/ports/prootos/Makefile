TOP = ../..
include $(TOP)/py/mkenv.mk
include $(TOP)/py/py.mk

CC = gcc
LD = ld
SIZE = size

CFLAGS = -m32 -ffreestanding -nostdlib \
         -fno-pic -fno-pie -fno-stack-protector \
         -fno-common -O2 -Wall -Wno-error \
         -Wpointer-arith -std=gnu99 -DNDEBUG \
         -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
         -I. -I$(TOP) -I$(BUILD)

LDFLAGS = -m elf_i386 -T linker.ld

QSTR_DEFS = qstrdefsport.h

vpath %.c $(TOP)/extmod $(TOP)/shared/runtime

SRC_EXTMOD = \
	$(TOP)/extmod/modbinascii.c \
	$(TOP)/extmod/modheapq.c \
	$(TOP)/extmod/modjson.c \
	$(TOP)/extmod/moductypes.c \
	$(TOP)/extmod/modframebuf.c \
	$(TOP)/extmod/modplatform.c \
	$(TOP)/extmod/modrandom.c \
	$(TOP)/extmod/modtime.c

SRC_SHARED = \
	$(TOP)/shared/runtime/pyexec.c

SRC_C = \
	main.c \
	mphalport.c \
	libc_shim.c \
	$(SRC_SHARED) \
	$(SRC_EXTMOD) \
	$(SRC_PY_CORE) \
	$(SRC_PY_GC) \
	$(SRC_PY_FORMAT) \
	$(SRC_PY_OBJ) \
	$(SRC_PY_PARSE) \
	$(SRC_PY_QSTR) \
	$(SRC_PY_STACK) \
	$(SRC_PY_BUILTIN) \
	$(SRC_PY_COMPILE) \
	$(SRC_PY_NLR) \
	$(SRC_PY_REPL) \
	$(SRC_PY_MOD) \
	$(SRC_PY_EXEC) \
	$(SRC_PY_MALLOC) \
	$(SRC_LIB)

SRC_QSTR += $(SRC_C)

OBJ = $(filter-out $(BUILD)/py/persistentcode.o, $(PY_CORE_O)) \
      $(addprefix $(BUILD)/, $(patsubst %.c,%.o,$(notdir $(SRC_C))))

all: $(BUILD)/firmware.elf

$(BUILD)/firmware.elf: $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $^
	$(Q)$(SIZE) $@

include $(TOP)/py/mkrules.mk
