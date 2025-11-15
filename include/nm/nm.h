#ifndef FT_NM_H
#define FT_NM_H

#include <elf.h>
#include <stddef.h>

typedef enum {
  CLASS32 = ELFCLASS32,
  CLASS64 = ELFCLASS64,
} nm_class_t;

typedef enum {
  ENDIANLITTLE,
  ENDIANBIG,
} nm_endian_t;

typedef struct {
  const char* ptr;
  size_t len;
} nm_string_t;

typedef struct {
  Elf64_Addr addr;
  nm_string_t name;
} nm_sym_t;

typedef struct {
  void* hdr;
} nm_section_t;

typedef struct {
  nm_section_t section;
} nm_strtab_section_t;

typedef struct {
  nm_section_t section;
} nm_symtab_section_t;

typedef struct {
  nm_section_t section;
} nm_dynsym_section_t;

typedef struct {
  nm_class_t bits;
  nm_endian_t endian;

  // The size of the object in bytes. To avoid reading out of bounds
  size_t size;
  // The mapped object
  void *mapped;

  // Sections

  // Holds the string table, each section/segment that needs a string provides
  // an index inside this table. Strings are null terminated.
  nm_strtab_section_t strtab;
  nm_dynsym_section_t dynsym;
  nm_symtab_section_t symtab;
} nm_object_t;

#endif