#include "string.h"

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') len++;
    return len;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(u8)*a - (int)(u8)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(u8)*a - (int)(u8)*b;
}

char* strcpy(char* dst, const char* src) {
    char* ret = dst;
    while ((*dst++ = *src++) != '\0') { }
    return ret;
}

char* strncpy(char* dst, const char* src, size_t n) {
    char* ret = dst;
    while (n && *src) { *dst++ = *src++; n--; }
    while (n--) *dst++ = '\0';
    return ret;
}

char* strcat(char* dst, const char* src) {
    char* ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++) != '\0') { }
    return ret;
}

void* memset(void* ptr, int c, size_t n) {
    u8* p = (u8*) ptr;
    u8  v = (u8) c;
    while (n--) *p++ = v;
    return ptr;
}

void* memcpy(void* dst, const void* src, size_t n) {
    u8*       d = (u8*) dst;
    const u8* s = (const u8*) src;
    while (n--) *d++ = *s++;
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const u8* pa = (const u8*) a;
    const u8* pb = (const u8*) b;
    while (n--) {
        if (*pa != *pb) return (int)*pa - (int)*pb;
        pa++; pb++;
    }
    return 0;
}

void* memchr(const void* ptr, int c, size_t n) {
    const u8* p = (const u8*) ptr;
    u8        v = (u8) c;
    while (n--) {
        if (*p == v) return (void*) p;
        p++;
    }
    return NULL;
}

void strrev(char* str) {
    size_t len = strlen(str);
    if (len < 2) return;
    size_t i = 0;
    size_t j = len - 1;
    while (i < j) {
        char tmp  = str[i];
        str[i]    = str[j];
        str[j]    = tmp;
        i++; j--;
    }
}
