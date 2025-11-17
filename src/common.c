#include <nm/common.h>
#include <nm/elfu.h>

void nm_memcpy(void* dst, const void* src, size_t n) {
  const u8* s = src;
  u8* d = dst;

  while (n--)
    *d++ = *s++;
}

int nm_strcmp(const char* a, const char* b) {
  while (*a && *b && *a == *b) {
    a++;
    b++;
  }

  return *(const u8*)a - *(const u8*)b;
}
