#ifndef ELFU_H
#define ELFU_H

#include <elf.h>
#include <stddef.h>

// ELF reader inspired by libelf
// Handle different endian & bit sizes. It is incomplete as its only goal is to
// handle ELF binaries that are in the scope of the subject.

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef struct _elfu_t elfu_t;

typedef enum {
  ELFU_SUCCESS = 0,
  ELFU_MAP_FAILED,
  // A syscall failed, errno is set with the correct error.
  ELFU_SYS_ERR,
  ELFU_IS_DIR,
  ELFU_NOTA_FILE,
  ELFU_OUT_OF_MEMORY,
  ELFU_INVALID_ARG,
  ELFU_MALFORMED,
  ELFU_UNKNOWN_FORMAT,
} elfu_err_t;

typedef enum {
  CLASSNONE = ELFCLASSNONE,
  CLASS32 = ELFCLASS32,
  CLASS64 = ELFCLASS64,
} elfu_class_t;

typedef enum {
  ENDIAN_NONE = ELFDATANONE,
  ENDIAN_LITTLE = ELFDATA2LSB,
  ENDIAN_BIG = ELFDATA2MSB,
} elfu_endian_t;

#ifdef ELFU_PRIVATE

typedef struct {
  Elf32_Half e_type;      /* Object file type */
  Elf32_Half e_machine;   /* Architecture */
  Elf32_Word e_version;   /* Object file version */
  Elf32_Addr e_entry;     /* Entry point virtual address */
  Elf32_Off e_phoff;      /* Program header table file offset */
  Elf32_Off e_shoff;      /* Section header table file offset */
  Elf32_Word e_flags;     /* Processor-specific flags */
  Elf32_Half e_ehsize;    /* ELF header size in bytes */
  Elf32_Half e_phentsize; /* Program header table entry size */
  Elf32_Half e_phnum;     /* Program header table entry count */
  Elf32_Half e_shentsize; /* Section header table entry size */
  Elf32_Half e_shnum;     /* Section header table entry count */
  Elf32_Half e_shstrndx;  /* Section header string table index */
} _elfu32_ehdr_t;

typedef struct {
  u8 magic[4];
  u8 class;
  u8 data;
  u8 version;
  u8 abi;
  u8 abi_version;

  u8 _padding[7];
} elf_ident_t;

static_assert(sizeof(elf_ident_t) == EI_NIDENT);

#endif

typedef struct {
  Elf64_Half e_type;      /* Object file type */
  Elf64_Half e_machine;   /* Architecture */
  Elf64_Word e_version;   /* Object file version */
  Elf64_Addr e_entry;     /* Entry point virtual address */
  Elf64_Off e_phoff;      /* Program header table file offset */
  Elf64_Off e_shoff;      /* Section header table file offset */
  Elf64_Word e_flags;     /* Processor-specific flags */
  Elf64_Half e_ehsize;    /* ELF header size in bytes */
  Elf64_Half e_phentsize; /* Program header table entry size */
  Elf64_Half e_phnum;     /* Program header table entry count */
  Elf64_Half e_shentsize; /* Section header table entry size */
  Elf64_Half e_shnum;     /* Section header table entry count */
  Elf64_Half e_shstrndx;  /* Section header string table index */
} elfu_ehdr_t;

typedef Elf64_Shdr elfu_shdr_t;
typedef Elf64_Sym elfu_isym_t;

typedef struct {
  const char* name;
  const char* version;  // Version string, nullptr if the symbol is not dynamic
  bool version_hidden;

  uint64_t sh_addr;  // The linked section address
  elfu_isym_t sym;
} elfu_sym_t;

typedef struct {
  elfu_shdr_t hdr;  // Section header

  const u8* data;  // Pointer to the section data

  const elfu_t* elf;
} elfu_section_t;

typedef struct _elfu_t {
  elfu_class_t class;     // Object class (32bit / 64bit)
  elfu_endian_t endian;   // Object endian
  elfu_endian_t hendian;  // Host endian

  elfu_ehdr_t ehdr;  // The ELF header

  // The raw object bytes, mapped from the file.
  uint8_t* raw;
  size_t fsize;
  size_t offset;

  struct {
    bool ehdr : 1;
  } flags;
} elfu_t;

enum {
  ELFU_VER_NONE = 0,
  ELFU_VER_NEED = 1,
  ELFU_VER_DEF = 2,
};

typedef struct {
  elfu_section_t versym;
  elfu_section_t verneed;
  elfu_section_t verdef;

  u8 flags;
} elfu_version_t;

typedef struct {
  size_t total;
  size_t cursor;

  bool has_version;
  elfu_version_t version;

  const elfu_t* elf;
  const elfu_section_t* symtab;
} elfu_sym_iter_t;

/*!
 * This function will allocate a new \c elfu_t object and map the passed object in memory.
 * @param fd The file descriptor of the ELF object.
 * @return A new \c elfu_t object on success. \c nullptr on failure, and sets the
 * appropriate error that can be retrieved with \c elfu_get_err.
 */
elfu_t* elfu_new(int fd);

/*!
 * Retrieve the ELF object header.
 * @param e The \c elfu_t object.
 * @param ehdr[out] The \c elfu_ehdr_t to fill.
 * @return Whether the operation was successful.
 */
bool elfu_get_ehdr(const elfu_t* e, elfu_ehdr_t* ehdr);

/*!
 * Retrieve a section by its index.
 * @param e The \c elfu_t object.
 * @param index The section index.
 * @param section[out] The \c elfu_section_t to fill.
 * @return Whether the operation was successful.
 * The operation can fail for multiple reasons:
 * - The index is out of bounds.
 * - The section address is no within the object bounds.
 */
bool elfu_get_section(const elfu_t* e, size_t index, elfu_section_t* section);

/*!
 * Retrieve the first \c SHT_SYMTAB section in the object.
 * @param e The \c elfu_t object.
 * @param symtab[out] The \c elfu_section_t to fill once found.
 * @return \c true if found, \c false otherwise. It will also return \c false on error.
 */
bool elfu_get_symtab(const elfu_t* e, elfu_section_t* symtab);

/*!
 * Retrieve the first \c SHT_DYNSYM section in the object.
 * @param e The \c elfu_t object.
 * @param dynsymtab[out] The \c elfu_section_t to fill once found.
 * @return \c true if found, \c false otherwise. It will also return \c false on error.
 */
bool elfu_get_dynsymtab(const elfu_t* e, elfu_section_t* dynsymtab);

/*!
 * Retrieve the name associated to the given section index.
 * @param e The \c elfu_t object.
 * @param index The section index.
 * @return A pointer to the string on success. \c nullptr on failure.
 */
const char* elfu_get_section_name(const elfu_t* e, size_t index);

/*!
 * This function retrieve a string from a string table section.
 * @param e The \c elfu_t object.
 * @param index The section index of the string table.
 * @param str The string offset within the string table.
 * @return A pointer to the string on success. \c nullptr on failure.
 */
const char* elfu_strptr(const elfu_t* e, size_t index, size_t str);

/*!
 * Initialize a symbol iterator for the given symbol table.
 * @param e The \c elfu_t object.
 * @param symtab The symbol table section.
 * @param i[out] The \c elfu_sym_iter_t to initialize.
 * @return Whether the operation was successful.
 */
bool elfu_get_sym_iter(const elfu_t* e, const elfu_section_t* symtab, elfu_sym_iter_t* i);

/*!
 * Retrieve the next symbol from the symbol iterator.
 * @param i The \c elfu_sym_iter_t iterator.
 * @param sym[out] The \c elfu_sym_t to fill.
 * @return Whether a symbol was retrieved. \c false if there is no symbol left or an error occurred.
 */
bool elfu_sym_iter_next(elfu_sym_iter_t* i, elfu_sym_t* sym);

/*!
 * @return Whether the library is in an error state or not.
 */
bool elfu_has_err();

/*!
 * @return The last error that occurred.
 */
elfu_err_t elfu_get_err();

/*!
 * Reset the error state.
 */
void elfu_reset_err();

/*!
 * Destroy an \c elfu_t object and unmap the object from memory.
 * @param e A pointer to an \c elfu_t object pointer to destroy. Set to \c nullptr after destruction.
 */
void elfu_destroy(elfu_t** e);

#endif
