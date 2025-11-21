#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <nm/common.h>
#include <nm/elfu.h>
#include <nm/nm.h>
#include <stdlib.h>
#include <string.h>

#include <nm/opt.h>

static const char* g_filename = nullptr;

static bool flag_no_sort = false;
static bool flag_reverse_sort = false;
static bool flag_only_undefined = false;
static bool flag_only_external = false;
static bool flag_dynamic = false;
static bool flag_no_filter = false;

static auto symtab_fetch = elfu_get_symtab;

// too lazy to pull libft ...

#define nm_err(err)                          \
  do {                                       \
    nm_putstr_fd(STDERR_FILENO, "nm: ");     \
    nm_putstr_fd(STDERR_FILENO, g_filename); \
    nm_putstr_fd(STDERR_FILENO, ": ");       \
    nm_putstr_fd(STDERR_FILENO, (err));      \
    nm_putstr_fd(STDERR_FILENO, "\n");       \
  } while (false)

#define nm_warn_p(err, prefix)               \
  do {                                       \
    nm_putstr_fd(STDERR_FILENO, "nm: ");     \
    nm_putstr_fd(STDERR_FILENO, (prefix));   \
    nm_putstr_fd(STDERR_FILENO, "'");        \
    nm_putstr_fd(STDERR_FILENO, g_filename); \
    nm_putstr_fd(STDERR_FILENO, "' ");       \
    nm_putstr_fd(STDERR_FILENO, (err));      \
    nm_putstr_fd(STDERR_FILENO, "\n");       \
  } while (false)

#define nm_warn(err) nm_warn_p(err, "")

static nm_sym_type_t nm_section_type(const elfu_t* obj, const size_t index) {
  // Obviously not a valid section index
  if (index != SHN_UNDEF && index >= SHN_LORESERVE)
    return SYM_UNKNOWN;

  // Really, really special case, when nm (bfd) encounters a symbol for which his section
  // doesn't exist, it classifies it as absolute.
  if (index > obj->ehdr.e_shnum)
    return SYM_ABSOLUTE_L;

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
  const auto bind = ELF64_ST_BIND(s.st_info);
  if (flag_only_undefined)
    return (s.st_shndx == SHN_UNDEF);
  if (flag_only_external)
    return (bind == STB_GLOBAL || bind == STB_WEAK || bind == STB_GNU_UNIQUE);
  if (flag_no_filter)
    return true;

  // I'm pretty sure those are only used for debugging ? And nm filters debugging symbols
  // by default.
  if (type == STT_FILE || type == STT_SECTION)
    return false;

  return true;
}

/*!
 * Process the given \a symtab ELF section and push its listed symbols to \a symbols.
 * @return \c -1 on error.
 */
static ssize_t nm_process_symtab(const elfu_t* obj,
                                 const elfu_section_t* symtab,
                                 nm_symbol_vector_t* symbols,
                                 bool* has_symbols) {
  elfu_sym_iter_t iter;
  if (!elfu_get_sym_iter(obj, symtab, &iter))
    return -1;

  elfu_sym_t s;
  while (elfu_sym_iter_next(&iter, &s)) {
    if (!nm_keep_symbol(obj, s.sym))
      continue;

    const nm_symbol_t symbol = {
        .name = s.name,
        .version = s.version,
        .version_hidden = s.version_hidden,
        .type = nm_sym_type(obj, s.sym),
        .value = s.sym.st_value,
        .o = s.sym,
        .pos = iter.cursor,
    };

    if (!nm_symbol_vector_push(symbols, symbol))
      return -1;
  }

  *has_symbols = (iter.total > 1);

  return (ssize_t)iter.total;
}

static void nm_symbol_put_value(const nm_symbol_t* s, const bool bits_64) {
  const size_t width = (bits_64) ? 16 : 8;

  char buffer[32] = {};
  auto value = s->value;

  if (s->o.st_shndx != SHN_UNDEF) {
    char* h = buffer + width;

    while (value > 0 && h != buffer) {
      const auto table = "0123456789abcdef";
      *--h = table[value % 16];
      value /= 16;
    }
    while (h != buffer)
      *--h = '0';
  } else {
    for (size_t i = 0; i < width; i++)
      buffer[i] = ' ';
  }

  nm_putstr(buffer);
}

static void nm_display_symbol(const nm_symbol_t* s, const bool bits_64) {
  nm_symbol_put_value(s, bits_64);
  nm_putstr((char[]){' ', (char)s->type, ' ', 0});
  nm_putstr(s->name);
  if (s->version) {
    nm_putstr("@");
    if (!s->version_hidden)
      nm_putstr("@");
    nm_putstr(s->version);
  }
  nm_putstr("\n");
}

static int nm_cmp_symbol(const nm_symbol_t* a, const nm_symbol_t* b) {
  auto cmp = nm_strcmp(a->name, b->name);
  if (cmp == 0)
    cmp = (int)a->pos - (int)b->pos;
  return flag_reverse_sort ? -cmp : cmp;
}

static bool nm_list_symbols(const elfu_t* obj) {
  bool ret = false;
  nm_symbol_vector_t symbols = {};
  if (!nm_symbol_vector_new(&symbols))
    return false;

  elfu_section_t sym;
  if (symtab_fetch(obj, &sym) && nm_process_symtab(obj, &sym, &symbols, &ret) < 0)
    goto err;

  if (!flag_no_sort)
    heapsort(symbols.ptr, symbols.len, nm_cmp_symbol);

  const bool bits_64 = obj->class == CLASS64;
  for (size_t i = 0; i < symbols.len; i++)
    nm_display_symbol(&symbols.ptr[i], bits_64);

err:
  nm_symbol_vector_destroy(&symbols);
  return ret;
}

static void nm_print_err() {
  switch (elfu_get_err()) {
    case ELFU_UNKNOWN_FORMAT:
      nm_err("file format not recognized");
      break;
    case ELFU_SYS_ERR:
      nm_warn(strerror(errno));
      break;
    case ELFU_NOTA_FILE:
      nm_warn_p("is not an ordinary file", "Warning: ");
      break;
    case ELFU_IS_DIR:
      nm_warn_p("is a directory", "Warning: ");
      break;
    default:
      break;
  }
}

static int nm_process_file(const char* name, bool print_filename) {
  int exit_code = EXIT_SUCCESS;
  elfu_t* obj = nullptr;

  g_filename = name;

  const int fd = open(name, O_RDONLY);
  if (fd < 0) {
    if (errno == ENOENT)
      nm_warn("No such file");
    else
      nm_err(strerror(errno));
    goto err;
  }

  if ((obj = elfu_new(fd)) == nullptr) {
    nm_print_err();
    goto err;
  }

  if (print_filename) {
    nm_putstr("\n");
    nm_putstr(name);
    nm_putstr(":\n");
  }

  if (!nm_list_symbols(obj))
    nm_err("no symbols");

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

int main(int argc, char** argv) {
  opt_t opt = nm_opt("prugDa");

  int flag;
  while ((flag = opt_next(&opt, argc, argv)) != OPT_END) {
    switch (flag) {
      case 'a':
        flag_no_filter = true;
        break;
      case 'D':
        flag_dynamic = true;
        symtab_fetch = elfu_get_dynsymtab;
        break;
      case 'g':
        flag_only_external = true;
        break;
      case 'u':
        flag_only_undefined = true;
        break;
      case 'r':
        flag_reverse_sort = true;
        break;
      case 'p':
        flag_no_sort = true;
        break;
      default:
        // TODO: print usage
        break;
    }
  }

  argc -= opt.argc;
  argv += opt.argc;

  if (argc == 0)
    return nm_process_file(NM_DEFAULT_PROGRAM, false);
  if (argc == 1)
    return nm_process_file(argv[0], false);

  int exit_code = EXIT_SUCCESS;
  if (argc > 1) {
    for (int i = 0; i < argc; i++) {
      exit_code += nm_process_file(argv[i], true);
    }
  }

  return exit_code;
}