#define ELFU_PRIVATE
#include <fcntl.h>
#include <nm/elfu.h>
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

static constexpr u8 elf_magic[] = {0x7f, 'E', 'L', 'F'};

#define translate(e, v)                        \
  ((e)->endian == (e)->hendian ? (v)           \
                               : _Generic((v), \
           u16: __builtin_bswap16,             \
           u32: __builtin_bswap32,             \
           u64: __builtin_bswap64)((v)))

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
  if (e->fsize < sizeof(elf_ident_t)) {
    seterr(ELFU_MALFORMED);
    return false;
  }

  const auto id = (elf_ident_t*)e->raw;
  if (*(u32*)id->magic != *(u32*)elf_magic) {
    seterr(ELFU_MALFORMED);
    return false;
  }

  e->class = id->class;
  e->endian = id->data;  // TODO: handle ELFDATANONE
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

  elfu_ehdr_t ehdr = {};
  if (e->class == CLASS32) {
    _elfu32_ehdr_t e32 = *(_elfu32_ehdr_t*)(e->raw + e->offset);

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

  e->flags.ehdr = true;
  e->ehdr = ehdr;
  e->offset += expected_size;

  return true;
}

#include <stdio.h>

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

bool elfu_get_symtab(const elfu_t* e, elfu_section_t* symtab) {
  return _elfu_first_section_by_type(e, SHT_SYMTAB, symtab);
}

bool elfu_get_dynsymtab(const elfu_t* e, elfu_section_t* dynsymtab) {
  return _elfu_first_section_by_type(e, SHT_DYNSYM, dynsymtab);
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

  elfu_shdr_t hdr = {};
  if (e->class == CLASS32) {
    Elf32_Shdr h32 = *(Elf32_Shdr*)(e->raw + off);

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
    hdr = *(elfu_shdr_t*)(e->raw + off);

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

  const auto section_start = hdr.sh_addr;
  const auto section_end = hdr.sh_addr + hdr.sh_size;
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

  if (*(strtab.data + strtab.hdr.sh_size) != 0)
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

bool elfu_has_err() {
  return g_err != ELFU_SUCCESS;
}

elfu_err_t elfu_get_err() {
  return g_err;
}
