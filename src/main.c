#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>

#include <nm/common.h>
#include <nm/elfu.h>
#include <nm/nm.h>
#include <stdlib.h>

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

static nm_sym_type_t nm_section_type(const elfu_t* obj, const size_t index) {
  // Obviously not a valid section index
  if (index != SHN_UNDEF && index >= SHN_LORESERVE)
    return SYM_UNKNOWN;

  elfu_section_t section;
  if (!elfu_get_section(obj, index, &section))
    return SYM_UNKNOWN;

  const auto type = section.hdr.sh_type;
  const auto flags = section.hdr.sh_flags;

  if (type == SHT_PROGBITS) {
    // .text section
    if (flags & SHF_EXECINSTR)
      return SYM_CODE_L;
  }
  // .bss section
  if (type == SHT_NOBITS && (flags & SHF_WRITE && flags & SHF_ALLOC))
    return SYM_BSS_L;
  // data sections
  if (flags & SHF_WRITE && flags & SHF_ALLOC)
    return SYM_INITD_L;
  if ((flags & SHF_WRITE) == 0)
    return SYM_RD_ONLY_DATA_L;
  return SYM_UNKNOWN;
}

#define shndx(s) ((s).st_shndx)

static nm_sym_type_t nm_sym_type(const elfu_t* obj, const Elf64_Sym s) {
  const auto type = ELF64_ST_TYPE(s.st_info);
  const auto bind = ELF64_ST_BIND(s.st_info);

  if (shndx(s) == SHN_COMMON)
    return SYM_COMMON_G;
  if (shndx(s) == SHN_UNDEF) {
    if (bind == STB_WEAK)
      return (type == STT_OBJECT) ? SYM_WEAK_OBJ_L : SYM_WEAK_L;
    return SYM_UNDEFINED;
  }

  if (type == STT_GNU_IFUNC)
    return SYM_INDIR;
  if (bind == STB_GNU_UNIQUE)
    return SYM_UNIQUE_GLOBAL;
  if (bind == STB_WEAK)
    return (type == STT_OBJECT) ? SYM_WEAK_OBJ_G : SYM_WEAK_G;

  const auto stype = (shndx(s) == SHN_ABS) ? SYM_ABSOLUTE_L : nm_section_type(obj, shndx(s));
  if (stype != SYM_UNKNOWN && bind == STB_GLOBAL)
    return stype - 32;

  return stype;
}

static bool nm_keep_symbol(const elfu_t* obj, const Elf64_Sym s) {
  (void)obj;

  const auto type = ELF64_ST_TYPE(s.st_info);
  // I'm pretty sure those are only used for debugging ? And nm filters debugging symbols
  // by default.
  // TODO: if -a flag this should be changed
  if (type == STT_FILE || type == STT_SECTION)
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
static ssize_t nm_process_symtab(const elfu_t* obj,
                                 const elfu_section_t* symtab,
                                 nm_symbol_vector_t* symbols) {
  const auto n = symtab->hdr.sh_size / symtab->hdr.sh_entsize;
  const auto table = (const Elf64_Sym*)symtab->data;
  const auto strtab = symtab->hdr.sh_link;

  for (size_t i = 1; i < n; i++) {
    const char* name = nullptr;
    const auto sym = table[i];

    if (ELF64_ST_TYPE(sym.st_info) == STT_SECTION &&
        (name = elfu_get_section_name(obj, sym.st_shndx)) == nullptr)
      name = "<corrupt>";
    if (!name && (name = elfu_strptr(obj, strtab, sym.st_name)) == nullptr)
      name = "<corrupt>";

    if (!nm_keep_symbol(obj, sym))
      continue;

    if (!nm_symbol_vector_push(symbols, (nm_symbol_t){name, nm_sym_type(obj, sym),
                                                      sym.st_value, sym}))
      return -1;
  }

  return (ssize_t)n;
}

static void nm_display_symbol(const nm_symbol_t* s) {
  if (s->o.st_shndx != SHN_UNDEF) {
    printf("%016lx", s->value);
  } else
    printf("                ");
  printf(" %c ", (char)s->type);
  printf("%s\n", s->name);
}

static int nm_cmp_symbol(const nm_symbol_t* a, const nm_symbol_t* b) {
  return nm_strcmp(a->name, b->name);
}

static __attribute_maybe_unused__ void nm_display_symbols(const nm_symbol_vector_t* symbols) {
  for (size_t i = 0; i < symbols->len; i++)
    nm_display_symbol(&symbols->ptr[i]);
}

static void nm_list_symbols(const elfu_t* obj, bool* found) {
  nm_symbol_vector_t symbols = {};
  if (!nm_symbol_vector_new(&symbols))
    return;

  elfu_section_t sym;
  if (elfu_get_symtab(obj, &sym) && nm_process_symtab(obj, &sym, &symbols) < 0)
    goto err;

  if (symbols.len != 0)
    *found = true;

  heapsort(symbols.ptr, symbols.len, nm_cmp_symbol);

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
  setlocale(LC_MESSAGES, "");
  setlocale(LC_CTYPE, "");
  setlocale(LC_COLLATE, "");

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