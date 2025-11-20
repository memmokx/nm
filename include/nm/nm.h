#ifndef FT_NM_H
#define FT_NM_H

#include <elf.h>
#include <stddef.h>

typedef enum {
  // "A" The symbol's value is absolute, and will not be changed by further linking.
  SYM_ABSOLUTE_G = 'A',
  SYM_ABSOLUTE_L = 'a',
  // "B" "b" The symbol is in the BSS data section.
  SYM_BSS_G = 'B',
  SYM_BSS_L = 'b',
  // "C" "c" The symbol is common.  Common symbols are uninitialized data.
  SYM_COMMON_G = 'C',
  SYM_COMMON_L = 'c',
  // "D" "d" The symbol is in the initialized data section.
  SYM_INITD_G = 'D',
  SYM_INITD_L = 'd',
  // "i" The symbol is an indirect reference to another symbol.
  SYM_INDIR = 'i',
  // "N" The symbol is a debugging symbol.
  SYM_DEBUG = 'N',
  // "n" The symbol is in a non-data, non-code, non-debug read-only section.
  SYM_RD_ONLY = 'n',
  // "p" The symbol is in a stack unwind section.
  SYM_UNWIND = 'p',
  // "R"
  // "r" The symbol is in a read only data section.
  SYM_RD_ONLY_DATA_G = 'R',
  SYM_RD_ONLY_DATA_L = 'r',
  // "S"
  // "s" The symbol is in an uninitialized or zero-initialized data section for small objects.
  SYM_UNINIT_DATA_G = 'S',
  SYM_UNINIT_DATA_L = 's',
  // "T"
  // "t" The symbol is in the text (code) section.
  SYM_CODE_G = 'T',
  SYM_CODE_L = 't',
  // "U" The symbol is undefined.
  SYM_UNDEFINED = 'U',
  // "u" The symbol is a unique global symbol.
  SYM_UNIQUE_GLOBAL = 'u',
  // "V" "v" The symbol is a weak object.
  SYM_WEAK_OBJ_G = 'V',
  SYM_WEAK_OBJ_L = 'v',
  // "W" "w" The symbol is a weak symbol that has not been specifically tagged as a weak object symbol.
  SYM_WEAK_G = 'W',
  SYM_WEAK_L = 'w',
  // ? The symbol type is unknown, or object file format specific.
  SYM_UNKNOWN = '?',
} nm_sym_type_t;

#include "elfu.h"

typedef struct {
  const char* name;
  const char* version;
  elfu_isym_t o;
  nm_sym_type_t type;
  uint64_t value;
  size_t pos;
} nm_symbol_t;

typedef int (*cmp_fn)(const nm_symbol_t*, const nm_symbol_t*);
void heapsort(nm_symbol_t* arr, size_t n, cmp_fn cmp);

typedef struct {
  nm_symbol_t* ptr;
  size_t len;
  size_t cap;
} nm_symbol_vector_t;

bool nm_symbol_vector_new(nm_symbol_vector_t* vec);
void nm_symbol_vector_destroy(nm_symbol_vector_t* vec);
bool nm_symbol_vector_grow(nm_symbol_vector_t* vec, size_t min);
bool nm_symbol_vector_push(nm_symbol_vector_t* vec, nm_symbol_t sym);

#endif