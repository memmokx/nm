#ifndef NM_COMMON_H
#define NM_COMMON_H

#include <stddef.h>

void nm_putstr_fd(int fd, const char* str);
void nm_putstr(const char* str);
int nm_strcmp(const char* a, const char* b);
void nm_memcpy(void* dst, const void* src, size_t n);

#endif
