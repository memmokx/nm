#include <nm/common.h>
#include <nm/elfu.h>
#include <unistd.h>

void nm_putstr_fd(const int fd, const char* str) {
  size_t i = 0;
  while (str[i])
    i++;

  write(fd, str, i);
}

void nm_putstr(const char* str) {
  nm_putstr_fd(STDOUT_FILENO, str);
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

int nm_strncmp(const char* a, const char* b, const size_t n) {
  int64_t nn = (int64_t)n;
  while (nn-- && *a && *b && *a == *b) {
    a++;
    b++;
  }

  if (nn == -1)
    return 0;

  return *(const u8*)a - *(const u8*)b;
}
