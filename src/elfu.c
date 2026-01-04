#define ELFU_PRIVATE
#include <fcntl.h>
#include <nm/elfu.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#undef ELFU_PRIVATE

static thread_local elfu_err_t g_err = ELFU_SUCCESS;

#define seterr(e) \
  do {            \
    g_err = (e);  \
  } while (0)

#define translate(e, v)                        \
  ((e)->endian == (e)->hendian ? (v)           \
                               : _Generic((v), \
           u16: __builtin_bswap16,             \
           u32: __builtin_bswap32,             \
           u64: __builtin_bswap64)((v)))

static elfu_ehdr_t _elfu_read_ehdr(const elfu_t* e, const uintptr_t offset) {
  elfu_ehdr_t ehdr = {};

  if (e->class == CLASS32) {
    _elfu32_ehdr_t e32 = *(_elfu32_ehdr_t*)(e->raw + offset);

    ehdr.e_type = translate(e, e32.e_type);
    ehdr.e_machine = translate(e, e32.e_machine);
    ehdr.e_version = translate(e, e32.e_version);
    ehdr.e_entry = translate(e, e32.e_entry);
    ehdr.e_phoff = translate(e, e32.e_phoff);
    ehdr.e_shoff = translate(e, e32.e_shoff);
    ehdr.e_flags = translate(e, e32.e_flags);
    ehdr.e_ehsize = translate(e, e32.e_ehsize);
    ehdr.e_phentsize = translate(e, e32.e_phentsize);
    ehdr.e_phnum = translate(e, e32.e_phnum);
    ehdr.e_shentsize = translate(e, e32.e_shentsize);
    ehdr.e_shnum = translate(e, e32.e_shnum);
    ehdr.e_shstrndx = translate(e, e32.e_shstrndx);
  } else {
    ehdr = *(elfu_ehdr_t*)(e->raw + e->offset);

    ehdr.e_type = translate(e, ehdr.e_type);
    ehdr.e_machine = translate(e, ehdr.e_machine);
    ehdr.e_version = translate(e, ehdr.e_version);
    ehdr.e_entry = translate(e, ehdr.e_entry);
    ehdr.e_phoff = translate(e, ehdr.e_phoff);
    ehdr.e_shoff = translate(e, ehdr.e_shoff);
    ehdr.e_flags = translate(e, ehdr.e_flags);
    ehdr.e_ehsize = translate(e, ehdr.e_ehsize);
    ehdr.e_phentsize = translate(e, ehdr.e_phentsize);
    ehdr.e_phnum = translate(e, ehdr.e_phnum);
    ehdr.e_shentsize = translate(e, ehdr.e_shentsize);
    ehdr.e_shnum = translate(e, ehdr.e_shnum);
    ehdr.e_shstrndx = translate(e, ehdr.e_shstrndx);
  }

  return ehdr;
}

static elfu_shdr_t _elfu_read_shdr(const elfu_t* e, const uintptr_t offset) {
  elfu_shdr_t hdr = {};

  if (e->class == CLASS32) {
    Elf32_Shdr h32 = *(Elf32_Shdr*)(e->raw + offset);

    hdr.sh_name = translate(e, h32.sh_name);
    hdr.sh_type = translate(e, h32.sh_type);
    hdr.sh_flags = translate(e, h32.sh_flags);
    hdr.sh_addr = translate(e, h32.sh_addr);
    hdr.sh_offset = translate(e, h32.sh_offset);
    hdr.sh_size = translate(e, h32.sh_size);
    hdr.sh_link = translate(e, h32.sh_link);
    hdr.sh_info = translate(e, h32.sh_info);
    hdr.sh_addralign = translate(e, h32.sh_addralign);
    hdr.sh_entsize = translate(e, h32.sh_entsize);
  } else {
    hdr = *(elfu_shdr_t*)(e->raw + offset);

    hdr.sh_name = translate(e, hdr.sh_name);
    hdr.sh_type = translate(e, hdr.sh_type);
    hdr.sh_flags = translate(e, hdr.sh_flags);
    hdr.sh_addr = translate(e, hdr.sh_addr);
    hdr.sh_offset = translate(e, hdr.sh_offset);
    hdr.sh_size = translate(e, hdr.sh_size);
    hdr.sh_link = translate(e, hdr.sh_link);
    hdr.sh_info = translate(e, hdr.sh_info);
    hdr.sh_addralign = translate(e, hdr.sh_addralign);
    hdr.sh_entsize = translate(e, hdr.sh_entsize);
  }

  return hdr;
}

static elfu_isym_t _elfu_read_sym(const elfu_t* e, const uintptr_t offset) {
  elfu_isym_t raw = {};

  if (e->class == CLASS32) {
    const auto s32 = *(Elf32_Sym*)(e->raw + offset);

    raw.st_name = translate(e, s32.st_name);
    raw.st_info = s32.st_info;
    raw.st_other = s32.st_other;
    raw.st_shndx = translate(e, s32.st_shndx);
    raw.st_value = translate(e, s32.st_value);
    raw.st_size = translate(e, s32.st_size);
  } else {
    raw = *(Elf64_Sym*)(e->raw + offset);

    raw.st_name = translate(e, raw.st_name);
    raw.st_shndx = translate(e, raw.st_shndx);
    raw.st_value = translate(e, raw.st_value);
    raw.st_size = translate(e, raw.st_size);
  }

  return raw;
}

static Elf64_Verneed _elfu_read_verneed(const elfu_t* e, const uintptr_t offset) {
  Elf64_Verneed raw = *(Elf64_Verneed*)(e->raw + offset);

  raw.vn_version = translate(e, raw.vn_version);
  raw.vn_cnt = translate(e, raw.vn_cnt);
  raw.vn_file = translate(e, raw.vn_file);
  raw.vn_aux = translate(e, raw.vn_aux);
  raw.vn_next = translate(e, raw.vn_next);

  return raw;
}

static Elf64_Vernaux _elfu_read_vernaux(const elfu_t* e, const uintptr_t offset) {
  Elf64_Vernaux raw = *(Elf64_Vernaux*)(e->raw + offset);

  raw.vna_hash = translate(e, raw.vna_hash);
  raw.vna_flags = translate(e, raw.vna_flags);
  raw.vna_other = translate(e, raw.vna_other);
  raw.vna_name = translate(e, raw.vna_name);
  raw.vna_next = translate(e, raw.vna_next);

  return raw;
}

static Elf64_Verdef _elfu_read_verdef(const elfu_t* e, const uintptr_t offset) {
  Elf64_Verdef raw = *(Elf64_Verdef*)(e->raw + offset);

  raw.vd_version = translate(e, raw.vd_version);
  raw.vd_flags = translate(e, raw.vd_flags);
  raw.vd_ndx = translate(e, raw.vd_ndx);
  raw.vd_cnt = translate(e, raw.vd_cnt);
  raw.vd_aux = translate(e, raw.vd_aux);
  raw.vd_next = translate(e, raw.vd_next);

  return raw;
}

static Elf64_Verdaux _elfu_read_verdaux(const elfu_t* e, const uintptr_t offset) {
  Elf64_Verdaux raw = *(Elf64_Verdaux*)(e->raw + offset);

  raw.vda_name = translate(e, raw.vda_name);
  raw.vda_next = translate(e, raw.vda_next);

  return raw;
}

static elfu_endian_t fetch_host_endian() {
  const union {
    u8 raw[4];
    u32 v;
  } data = {.raw = {1, 2, 3, 4}};

  if (data.v == 0x04030201)
    return ENDIAN_LITTLE;
  return ENDIAN_BIG;
}

static bool elf_read_ident(elfu_t* e) {
  constexpr u8 elf_magic[] = {0x7f, 'E', 'L', 'F'};

  if (e->fsize < sizeof(elf_ident_t)) {
    seterr(ELFU_UNKNOWN_FORMAT);
    return false;
  }

  const auto id = (elf_ident_t*)e->raw;
  if (*(u32*)id->magic != *(u32*)elf_magic) {
    seterr(ELFU_UNKNOWN_FORMAT);
    return false;
  }

  e->class = id->class;
  e->endian = id->data;

  if (id->version != EV_CURRENT || (e->class != CLASS32 && e->class != CLASS64) ||
      (e->endian != ENDIAN_LITTLE && e->endian != ENDIAN_BIG)) {
    seterr(ELFU_UNKNOWN_FORMAT);
    return false;
  }

  e->offset += sizeof(elf_ident_t);

  return true;
}

static bool elf_read_header(elfu_t* e) {
  const auto expected_size =
      e->class == CLASS32 ? sizeof(_elfu32_ehdr_t) : sizeof(elfu_ehdr_t);

  if (e->fsize < expected_size + e->offset) {
    seterr(ELFU_MALFORMED);
    return false;
  }

  const auto ehdr = _elfu_read_ehdr(e, e->offset);

  e->flags.ehdr = true;
  e->ehdr = ehdr;
  e->offset += expected_size;

  return true;
}

elfu_t* elfu_new(const int fd) {
  elfu_t* elf = malloc(sizeof(elfu_t));
  if (!elf) {
    seterr(ELFU_OUT_OF_MEMORY);
    goto err;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    seterr(ELFU_SYS_ERR);
    goto err;
  }

  if (S_ISDIR(st.st_mode)) {
    seterr(ELFU_IS_DIR);
    goto err;
  }

  if (!S_ISREG(st.st_mode)) {
    seterr(ELFU_NOTA_FILE);
    goto err;
  }

  *elf = (elfu_t){};
  elf->fsize = st.st_size;

  elf->raw = mmap(nullptr, elf->fsize, PROT_READ, MAP_PRIVATE, fd, 0);
  if (elf->raw == MAP_FAILED) {
    seterr(ELFU_MAP_FAILED);
    goto err;
  }

  elf->hendian = fetch_host_endian();
  if (!elf_read_ident(elf))
    goto err;
  if (!elf_read_header(elf))
    goto err;

  return elf;

err:
  elfu_destroy(&elf);
  return nullptr;
}

bool elfu_get_ehdr(const elfu_t* e, elfu_ehdr_t* ehdr) {
  if (!e || !ehdr) {
    seterr(ELFU_INVALID_ARG);
    return false;
  }

  if (!e->flags.ehdr) {
    seterr(ELFU_MALFORMED);
    return false;
  }

  *ehdr = e->ehdr;
  return true;
}

static bool _elfu_first_section_by_type(const elfu_t* e,
                                        const uint64_t type,
                                        elfu_section_t* section) {
  if (!e || !e->flags.ehdr || !section) {
    seterr(ELFU_INVALID_ARG);
    return false;
  }

  for (size_t i = 0; i < e->ehdr.e_shnum; i++) {
    elfu_section_t tmp;
    if (!elfu_get_section(e, i, &tmp))
      return false;

    if (tmp.hdr.sh_type == type) {
      *section = tmp;
      return true;
    }
  }

  return false;
}

static const char* _elfu_version_from_verdef(const elfu_section_t* verdef,
                                             const elfu_isym_t* sym,
                                             const size_t version) {
  const auto e = verdef->elf;
  const auto base = (uintptr_t)verdef->hdr.sh_offset;
  const auto end = base + verdef->hdr.sh_size;
  const auto strtab = verdef->hdr.sh_link;

  if (verdef->hdr.sh_size == 0 || e->fsize < base || e->fsize < end)
    return nullptr;

  uintptr_t cursor = 0;
  uintptr_t vnoff = 0;
  for (size_t i = 0; i < verdef->hdr.sh_info; i++) {
    cursor = base + vnoff;
    if (cursor + sizeof(Elf64_Verdef) < cursor || end < cursor + sizeof(Elf64_Verdef)) {
      seterr(ELFU_MALFORMED);
      return nullptr;
    }

    const auto vd = _elfu_read_verdef(e, cursor);
    if (vd.vd_ndx == version) {
      cursor += vd.vd_aux;
      if (cursor + sizeof(Elf64_Verdaux) < cursor || end < cursor + sizeof(Elf64_Verdaux)) {
        seterr(ELFU_MALFORMED);
        return nullptr;
      }

      const auto vdaux = _elfu_read_verdaux(e, cursor);
      // If the name is the same as the symbol name, avoid returning it as its redundant.
      // nm seems to be doing this so we follow the same behavior.
      return (vdaux.vda_name != sym->st_name) ? elfu_strptr(e, strtab, vdaux.vda_name)
                                              : nullptr;
    }

    vnoff += vd.vd_next;
  }

  return nullptr;
}

static const char* _elfu_version_from_verneed(const elfu_section_t* verneed,
                                              const size_t version) {
  const auto e = verneed->elf;
  const auto base = (uintptr_t)verneed->hdr.sh_offset;
  const auto end = base + verneed->hdr.sh_size;
  const auto strtab = verneed->hdr.sh_link;

  if (verneed->hdr.sh_size == 0 || e->fsize < base || e->fsize < end)
    return nullptr;

  uintptr_t cursor = 0;
  uintptr_t vnoff = 0;
  for (size_t i = 0; i < verneed->hdr.sh_info; i++) {
    cursor = base + vnoff;
    if (cursor + sizeof(Elf64_Verneed) < cursor || end < cursor + sizeof(Elf64_Verneed)) {
      seterr(ELFU_MALFORMED);
      return nullptr;
    }

    const auto vn = _elfu_read_verneed(e, cursor);
    cursor += vn.vn_aux;

    for (size_t aux = 0; aux < vn.vn_cnt; aux++) {
      if (cursor + sizeof(Elf64_Vernaux) < cursor || end < cursor + sizeof(Elf64_Vernaux)) {
        seterr(ELFU_MALFORMED);
        return nullptr;
      }

      const auto vnaux = _elfu_read_vernaux(e, cursor);
      if (vnaux.vna_other == version)
        return elfu_strptr(e, strtab, vnaux.vna_name);
      cursor += vnaux.vna_next;
    }

    vnoff += vn.vn_next;
  }

  return nullptr;
}

#define VERSYM_HIDDEN 0x8000
#define VERSYM_VERSION 0x7fff

static const char* _elfu_get_sym_version(const elfu_t* e,
                                         const elfu_version_t* v,
                                         const elfu_isym_t* sym,
                                         const size_t index,
                                         bool* hidden) {
  // https://lists.debian.org/lsb-spec/1999/12/msg00017.html
  // https://www.gabriel.urdhr.fr/2015/09/28/elf-file-format/#symbol-versions

  // We can recover the version string like this:
  //  1. in the versym section, each entries map to a symbol in the dynsym table,
  //     each entry is an u16 value that represents the version, (0: means local & 1: means global)
  //  2. The verneed section has `sh_info` entries. The entries each list the number of vernaux element they hold.
  //      Looks like:
  //      0x00: Elf*_Verneed: {.., vn_cnt: 5, vn_next: 0x60}
  //      0x10: Elf*_Vernaux: {.., vna_name: offset to string table}
  //      ...
  //      0x60: Elf*_Verneed: {.., vn_cnt: 3, ...}
  //      0x70: Elf*_Vernaux: {.., vna_name: offset to string table}

  const auto versym = &v->versym;
  const auto verneed = (v->flags & ELFU_VER_NEED) ? &v->verneed : nullptr;
  const auto verdef = (v->flags & ELFU_VER_DEF) ? &v->verdef : nullptr;

  const auto versym_base = (uintptr_t)versym->data;
  const auto versym_offset = index * sizeof(u16);
  if (versym_offset + sizeof(u16) < versym_offset ||
      versym->hdr.sh_size < versym_offset + sizeof(u16)) {
    seterr(ELFU_MALFORMED);
    return nullptr;
  }

  const auto version = translate(e, *(u16*)(versym_base + versym_offset));
  if ((version & VERSYM_VERSION) == VER_NDX_LOCAL ||
      (version & VERSYM_VERSION) == VER_NDX_GLOBAL)
    return nullptr;

  // related: https://github.com/golang/go/issues/63952#issuecomment-1936641809
  if ((version & VERSYM_HIDDEN) != 0)
    *hidden = true;

  if (sym->st_shndx != SHN_UNDEF && version != (VERSYM_HIDDEN | 0x1) && verdef != nullptr) {
    const char* name;
    if ((name = _elfu_version_from_verdef(verdef, sym, version & VERSYM_VERSION)))
      return name;
  }

  // The verneed section holds the version information for undefined symbols, thus the
  // symbol is definitely hidden.
  *hidden = true;
  return (verneed != nullptr) ? _elfu_version_from_verneed(verneed, version) : nullptr;
}

bool elfu_get_symtab(const elfu_t* e, elfu_section_t* symtab) {
  return _elfu_first_section_by_type(e, SHT_SYMTAB, symtab);
}

bool elfu_get_dynsymtab(const elfu_t* e, elfu_section_t* dynsymtab) {
  return _elfu_first_section_by_type(e, SHT_DYNSYM, dynsymtab);
}

bool elfu_sym_iter_next(elfu_sym_iter_t* i, elfu_sym_t* sym) {
  if (!i || !sym) {
    seterr(ELFU_INVALID_ARG);
    return false;
  }

  if (i->cursor >= i->total)
    return false;

  const auto e = i->elf;
  const auto symtab = i->symtab;
  const auto entry_size = (e->class == CLASS64) ? sizeof(Elf64_Sym) : sizeof(Elf32_Sym);
  const auto off = symtab->hdr.sh_offset + i->cursor * entry_size;

  if (off + entry_size < off || e->fsize < off + entry_size) {
    seterr(ELFU_MALFORMED);
    return false;
  }

  const char* name = nullptr;
  const char* version = nullptr;
  const auto raw = _elfu_read_sym(e, off);

  if (ELF64_ST_TYPE(raw.st_info) == STT_SECTION && raw.st_shndx < SHN_LORESERVE &&
      (name = elfu_get_section_name(e, raw.st_shndx)) == nullptr)
    name = "<corrupt>";
  if (!name && (name = elfu_strptr(e, symtab->hdr.sh_link, raw.st_name)) == nullptr)
    name = "<corrupt>";

  bool hidden = false;
  if (i->has_version)
    version = _elfu_get_sym_version(e, &i->version, &raw, i->cursor, &hidden);

  uint64_t sh_addr = 0;
  elfu_section_t section;
  if (elfu_get_section(e, raw.st_shndx, &section))
    sh_addr = section.hdr.sh_addr;

  *sym = (elfu_sym_t){
      .name = name,
      .version = version,
      .version_hidden = hidden,
      .sh_addr = sh_addr,
      .sym = raw,
  };

  i->cursor++;

  return true;
}

bool elfu_get_sym_iter(const elfu_t* e, const elfu_section_t* symtab, elfu_sym_iter_t* i) {
  if (!e || !symtab) {
    seterr(ELFU_INVALID_ARG);
    return false;
  }

  // We assume `symtab` is valid and was obtained using `elfu_get_section`.

  const auto hdr = symtab->hdr;
  if (hdr.sh_type != SHT_SYMTAB && hdr.sh_type != SHT_DYNSYM) {
    seterr(ELFU_INVALID_ARG);
    return false;
  }

  const auto entry_size = (e->class == CLASS64) ? sizeof(Elf64_Sym) : sizeof(Elf32_Sym);
  if (hdr.sh_entsize != entry_size) {
    seterr(ELFU_MALFORMED);
    return false;
  }

  const auto count = hdr.sh_size / hdr.sh_entsize;

  elfu_sym_iter_t iter = {
      .elf = e,
      .symtab = symtab,
      .cursor = 1,
      .total = count,
  };

  // If it is a dynsym section we're trying to iterate, we try to find the associated
  // version sections
  if (hdr.sh_type == SHT_DYNSYM) {
    elfu_section_t versym;
    elfu_section_t verneed;
    elfu_section_t verdef;

    if (_elfu_first_section_by_type(e, SHT_GNU_versym, &versym)) {
      iter.has_version = true;
      iter.version.flags = ELFU_VER_NONE;
      iter.version.versym = versym;

      if (_elfu_first_section_by_type(e, SHT_GNU_verneed, &verneed)) {
        iter.version.verneed = verneed;
        iter.version.flags |= ELFU_VER_NEED;
      }

      if (_elfu_first_section_by_type(e, SHT_GNU_verdef, &verdef)) {
        iter.version.verdef = verdef;
        iter.version.flags |= ELFU_VER_DEF;
      }
    }
  }

  *i = iter;

  return true;
}

bool elfu_get_section(const elfu_t* e, const size_t index, elfu_section_t* section) {
  if (!e || !section || !e->flags.ehdr) {
    seterr(ELFU_INVALID_ARG);
    return false;
  }

  if (index >= e->ehdr.e_shnum) {
    seterr(ELFU_INVALID_ARG);
    return false;
  }

  const auto hdrsize = e->ehdr.e_shentsize;
  const auto off = e->ehdr.e_shoff + index * hdrsize;

  // overflow OR out of bounds
  if (off + hdrsize < off || e->fsize < off + hdrsize) {
    seterr(ELFU_MALFORMED);
    return false;
  }

  const auto hdr = _elfu_read_shdr(e, off);
  if (hdr.sh_type == SHT_NOBITS) {
    *section = (elfu_section_t){
        .hdr = hdr,
        .data = nullptr,
        .elf = e,
    };
    return true;
  }

  const auto section_start = hdr.sh_offset;
  const auto section_end = hdr.sh_offset + hdr.sh_size;
  if (section_end < section_start || e->fsize < section_end) {
    seterr(ELFU_MALFORMED);
    return false;
  }

  *section = (elfu_section_t){
      .hdr = hdr,
      .data = e->raw + hdr.sh_offset,
      .elf = e,
  };

  return true;
}

const char* elfu_get_section_name(const elfu_t* e, const size_t index) {
  if (!e || !e->flags.ehdr) {
    seterr(ELFU_INVALID_ARG);
    return nullptr;
  }

  elfu_section_t tmp;
  if (!elfu_get_section(e, index, &tmp))
    return nullptr;

  return elfu_strptr(e, e->ehdr.e_shstrndx, tmp.hdr.sh_name);
}

const char* elfu_strptr(const elfu_t* e, const size_t index, const size_t str) {
  elfu_section_t strtab;
  if (!elfu_get_section(e, index, &strtab))
    return nullptr;

  const auto start = (uintptr_t)strtab.data;
  const auto end = (uintptr_t)(strtab.data + str);

  if (end < start)
    return nullptr;

  if (e->fsize < end - (uintptr_t)e->raw)
    return nullptr;

  // We do this check to ensure the strtab is actually null terminated and that in the
  // worst case we don't read out of bounds.
  const auto strtab_size = strtab.hdr.sh_size;
  if (strtab_size == 0)
    return nullptr;
  if (*(strtab.data + strtab_size - 1) != 0 && *(strtab.data + strtab_size) != 0)
    return nullptr;

  return (const char*)(strtab.data + str);
}

void elfu_destroy(elfu_t** e) {
  if (!e || !*e)
    return;

  if ((*e)->raw && (*e)->raw != MAP_FAILED)
    munmap((*e)->raw, (*e)->fsize);
  free(*e);
  *e = nullptr;
}

void elfu_reset_err() {
  g_err = ELFU_SUCCESS;
}

bool elfu_has_err() {
  return g_err != ELFU_SUCCESS;
}

elfu_err_t elfu_get_err() {
  return g_err;
}
