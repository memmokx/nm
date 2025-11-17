#include <nm/common.h>
#include <nm/elfu.h>
#include <unistd.h>

void nm_putstr(const char* str) {
  size_t i = 0;
  while (str[i])
    i++;

  write(STDOUT_FILENO, str, i);
}

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
