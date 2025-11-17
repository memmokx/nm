#include <nm/common.h>
#include <nm/nm.h>
#include <stdlib.h>

bool nm_symbol_vector_new(nm_symbol_vector_t* vec) {
  constexpr size_t initial = 4;

  vec->ptr = malloc(initial * sizeof(nm_symbol_t));
  if (!vec->ptr)
    return false;

  vec->len = 0;
  vec->cap = initial;
  return true;
}

void nm_symbol_vector_destroy(nm_symbol_vector_t* vec) {
  if (vec->ptr) {
    free(vec->ptr);
    vec->ptr = nullptr;
  }

  vec->len = 0;
  vec->cap = 0;
}

bool nm_symbol_vector_grow(nm_symbol_vector_t* vec, const size_t min) {
  size_t new = vec->cap;
  while (new <= min)
    new += new / 2 + 8;

  void* old = vec->ptr;
  vec->ptr = malloc(new * sizeof(nm_symbol_t));
  if (!vec->ptr) {
    free(old);
    return false;
  }

  vec->cap = new;
  nm_memcpy(vec->ptr, old, vec->len * sizeof(nm_symbol_t));
  free(old);

  return true;
}

bool nm_symbol_vector_push(nm_symbol_vector_t* vec, const nm_symbol_t sym) {
  if (vec->len >= vec->cap && !nm_symbol_vector_grow(vec, vec->len + 1))
    return false;

  vec->ptr[vec->len++] = sym;
  return true;
}
