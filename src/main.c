#include <fcntl.h>
#include <sys/mman.h>
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

typedef struct {
  const char* name;
  Elf64_Sym sym;  // temporary
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

static __attribute_maybe_unused__ void nm_symbol_vector_clear(nm_symbol_vector_t* vec) {
  vec->len = 0;
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
  for (size_t i = 0; i < n; i++) {
    const auto sym = table[i];

    if ((name = elfu_strptr(obj, strtab, sym.st_name)) == nullptr)
      return -1;

    if (!nm_symbol_vector_push(symbols, (nm_symbol_t){name, sym}))
      return -1;
  }

  return (ssize_t)n;
}

#include <stdio.h>

static void nm_list_symbols(const elfu_t* obj, const elfu_ehdr_t* hdr, bool* found) {
  nm_symbol_vector_t symbols = {};
  if (!nm_symbol_vector_new(&symbols))
    return;

  for (size_t i = 0; i < hdr->e_shnum; i++) {
    elfu_section_t section;
    if (!elfu_get_section(obj, i, &section))
      goto err;

    if (section.hdr.sh_type != SHT_SYMTAB)
      continue;

    if (nm_process_sym_tab(obj, &section, &symbols) < 0)
      goto err;
  }

  if (symbols.len != 0)
    *found = true;

  for (size_t i = 0; i < symbols.len; i++)
    printf("symbol: '%s'\n", symbols.ptr[i].name);

err:
  nm_symbol_vector_destroy(&symbols);
}

void nm_process_file(const char* name) {
  elfu_t* obj = nullptr;

  const int fd = open(name, O_RDONLY);
  if (fd < 0)
    goto err;

  if ((obj = elfu_new(fd)) == nullptr)
    goto err;

  elfu_ehdr_t ehdr;
  if (!elfu_get_ehdr(obj, &ehdr))
    goto err;

  bool found = false;
  nm_list_symbols(obj, &ehdr, &found);
  // TODO: no symbols found

err:
  if (fd != -1)
    close(fd);
  if (obj)
    elfu_destroy(&obj);
}

int main(const int argc, char** argv) {
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      nm_process_file(argv[i]);
    }
  }
}