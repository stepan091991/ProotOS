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

SRC_C = \
	main.c \
	mphalport.c \
	libc_shim.c \
	$(SRC_SHARED) \
	$(SRC_LIB) \
	$(SRC_PY_CORE) \
	$(SRC_PY_GC) \
	$(SRC_PY_FORMAT) \
	$(SRC_PY_OBJ) \
	$(SRC_PY_PARSE) \
	$(SRC_PY_QSTR) \
	$(SRC_PY_STACK) \
	$(SRC_PY_MISC) \
	$(SRC_PY_BUILTIN) \
	$(SRC_PY_COMPILE) \
	$(SRC_PY_NLR) \
	$(SRC_PY_REPL) \
	$(SRC_PY_MOD) \
	$(SRC_PY_EXEC) \
	$(SRC_PY_DYNLOAD) \
	$(SRC_PY_PERSISTENT) \
	$(SRC_PY_SCHED) \
	$(SRC_PY_READLINE) \
	$(SRC_PY_PRINT) \
	$(SRC_PY_EMIT) \
	$(SRC_PY_LEXER) \
	$(SRC_PY_MPZ) \
	$(SRC_PY_RGB) \
	$(SRC_PY_PROFILE) \
	$(SRC_PY_ASM) \
	$(SRC_PY_MALLOC) \
	$(SRC_PY_MODSTACK) \
	$(SRC_PY_ARGFILE) \
	$(SRC_PY_ARGPARSE)

# ─── Shared: только pyexec (readline у нас своя) ──
SRC_SHARED = \
	$(TOP)/shared/runtime/pyexec.c

OBJ = $(PY_CORE_O) \
      $(addprefix $(BUILD)/, $(patsubst %.c,%.o,$(notdir $(SRC_C)))) \
      $(addprefix $(BUILD)/, $(patsubst %.c,%.o,$(notdir $(SRC_SHARED)))) \
      $(addprefix $(BUILD)/, libc_shim.o mphalport.o main.o)

$(BUILD)/pyexec.o: $(TOP)/shared/runtime/pyexec.c
	$(call compile_c)

all: $(BUILD)/firmware.elf

$(BUILD)/firmware.elf: $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $^
	$(Q)$(SIZE) $@

include $(TOP)/py/mkrules.mk
