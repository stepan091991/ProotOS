#ifndef PROOTOS_STRING_H
#define PROOTOS_STRING_H

#include "types.h"

size_t strlen(const char* str);
int    strcmp(const char* a, const char* b);
int    strncmp(const char* a, const char* b, size_t n);
char*  strcpy(char* dst, const char* src);
char*  strncpy(char* dst, const char* src, size_t n);
char*  strcat(char* dst, const char* src);

void*  memset(void* ptr, int c, size_t n);
void*  memcpy(void* dst, const void* src, size_t n);
int    memcmp(const void* a, const void* b, size_t n);
void*  memchr(const void* ptr, int c, size_t n);

void   strrev(char* str);

char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
long  strtol(const char *s, char **endptr, int base);
int snprintf(char *buf, size_t size, const char *fmt, ...);

#endif
