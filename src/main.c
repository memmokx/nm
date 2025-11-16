#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <nm/elfu.h>
#include <stdlib.h>

// TODO: add libadvanced
static void nm_memcpy(void* dst, const void* src, size_t n) {
  const u8* s = src;
  u8* d = dst;

  while (n--)
    *d++ = *s++;
}

typedef enum {
  // "A" The symbol's value is absolute, and will not be changed by further linking.
  SYM_ABSOLUTE = 'A',
  // "B" "b" The symbol is in the BSS data section.
  SYM_BSS_G = 'B',
  SYM_BSS_L = 'b',
  // "C" "c" The symbol is common.  Common symbols are uninitialized data.
  SYM_COMMON_G = 'C',
  SYM_COMMON_L = 'c',
  // "D" "d" The symbol is in the initialized data section.
  SYM_INITD_G = 'D',
  SYM_INITD_L = 'd',
  // "I" The symbol is an indirect reference to another symbol.
  SYM_INDIR = 'I',
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

typedef struct {
  const char* name;
  nm_sym_type_t type;
  uint64_t value;
  Elf64_Sym o;
} nm_symbol_t;

typedef struct {
  nm_symbol_t* ptr;
  size_t len;
  size_t cap;
} nm_symbol_vector_t;

static bool nm_symbol_vector_new(nm_symbol_vector_t* vec) {
  constexpr size_t initial = 4;

  vec->ptr = malloc(initial * sizeof(nm_symbol_t));
  if (!vec->ptr)
    return false;

  vec->len = 0;
  vec->cap = initial;
  return true;
}

static void nm_symbol_vector_destroy(nm_symbol_vector_t* vec) {
  if (vec->ptr) {
    free(vec->ptr);
    vec->ptr = nullptr;
  }

  vec->len = 0;
  vec->cap = 0;
}

static bool nm_symbol_vector_grow(nm_symbol_vector_t* vec, const size_t min) {
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

static bool nm_symbol_vector_push(nm_symbol_vector_t* vec, const nm_symbol_t sym) {
  if (vec->len >= vec->cap && !nm_symbol_vector_grow(vec, vec->len + 1))
    return false;

  vec->ptr[vec->len++] = sym;
  return true;
}

#define shndx(s) ((s).st_shndx)

static nm_sym_type_t nm_sym_type(const elfu_t* obj, const Elf64_Sym s) {
  (void)obj;
  const auto type = ELF64_ST_TYPE(s.st_info);
  const auto bind = ELF64_ST_BIND(s.st_info);

  if (shndx(s) == SHN_COMMON)
    return SYM_COMMON_G;
  if (shndx(s) == SHN_ABS)
    return SYM_ABSOLUTE;
  if (shndx(s) == SHN_UNDEF) {
    if (bind == STB_WEAK)
      return (type == STT_OBJECT) ? SYM_WEAK_OBJ_L : SYM_WEAK_L;
    return SYM_UNDEFINED;
  }

  if (type == STT_GNU_IFUNC)
    return SYM_INDIR;
  if (bind == STB_GNU_UNIQUE)
    return SYM_UNIQUE_GLOBAL;
  return SYM_UNKNOWN;
}

static bool nm_keep_sym(const elfu_t* obj, const Elf64_Sym s) {
  (void)obj;

  const auto type = ELF64_ST_TYPE(s.st_info);
  if (type == STT_FILE)
    return false;

  return true;
}

/*!
 * Process the given \a symtab ELF section and push its listed symbols to \a symbols.
 * @param obj
 * @param symtab
 * @param symbols
 * @return The number of symbols found, or \c -1 on error.
 */
static ssize_t nm_process_sym_tab(const elfu_t* obj,
                                  const elfu_section_t* symtab,
                                  nm_symbol_vector_t* symbols) {
  const auto n = symtab->hdr.sh_size / symtab->hdr.sh_entsize;
  const auto table = (const Elf64_Sym*)symtab->data;
  const auto strtab = symtab->hdr.sh_link;

  const char* name;
  for (size_t i = 1; i < n; i++) {
    const auto sym = table[i];

    if ((name = elfu_strptr(obj, strtab, sym.st_name)) == nullptr)
      name = "<corrupt>";

    if (!nm_keep_sym(obj, sym))
      continue;

    if (!nm_symbol_vector_push(symbols, (nm_symbol_t){name, nm_sym_type(obj, sym),
                                                      sym.st_value, sym}))
      return -1;
  }

  return (ssize_t)n;
}

static void nm_display_sym(const nm_symbol_t* s) {
  if (s->o.st_shndx != SHN_UNDEF) {
    printf("%016lx", s->value);
  } else
    printf("                ");
  printf(" %c ", (char)s->type);
  printf("%s\n", s->name);
}

static __attribute_maybe_unused__ void nm_display_symbols(const nm_symbol_vector_t* symbols) {
  for (size_t i = 0; i < symbols->len; i++)
    nm_display_sym(&symbols->ptr[i]);
}

static void nm_list_symbols(const elfu_t* obj, bool* found) {
  nm_symbol_vector_t symbols = {};
  if (!nm_symbol_vector_new(&symbols))
    return;

  elfu_section_t sym;
  if (elfu_get_symtab(obj, &sym) && nm_process_sym_tab(obj, &sym, &symbols) < 0)
    goto err;

  if (symbols.len != 0)
    *found = true;

  nm_display_symbols(&symbols);

err:
  *found = false;
  nm_symbol_vector_destroy(&symbols);
}

int nm_process_file(const char* name) {
  int exit_code = EXIT_SUCCESS;
  elfu_t* obj = nullptr;

  const int fd = open(name, O_RDONLY);
  if (fd < 0)
    goto err;

  if ((obj = elfu_new(fd)) == nullptr)
    goto err;

  bool found = false;
  nm_list_symbols(obj, &found);
  // TODO: no symbols found

  goto done;

err:
  exit_code = EXIT_FAILURE;
done:
  if (fd != -1)
    close(fd);
  if (obj)
    elfu_destroy(&obj);
  return exit_code;
}

#define NM_DEFAULT_PROGRAM "a.out"

int main(const int argc, char** argv) {
  if (argc == 1)
    return nm_process_file(NM_DEFAULT_PROGRAM);
  if (argc == 2)
    return nm_process_file(argv[1]);

  int exit_code = EXIT_SUCCESS;
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      printf("\n%s:\n", argv[i]);
      exit_code += nm_process_file(argv[i]);
    }
  }

  return exit_code;
}