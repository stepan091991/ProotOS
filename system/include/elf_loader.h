#ifndef PROOTOS_ELF_LOADER_H
#define PROOTOS_ELF_LOADER_H

#include "types.h"

typedef void (*elf_entry_t)(void);

elf_entry_t elf_load(const char* path);
int          elf_exec(const char* path);

#endif
