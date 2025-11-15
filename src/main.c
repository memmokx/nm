#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <nm/elfu.h>

#include <stdio.h>

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

  elfu_section_t shstrtab;
  if (!elfu_get_section(obj, ehdr.e_shstrndx, &shstrtab))
    goto err;

  for (size_t i = 0; i < ehdr.e_shnum; i++) {
    elfu_section_t section;
    if (!elfu_get_section(obj, i, &section))
      goto err;

    const auto sname = (const char*)(shstrtab.data + section.hdr.sh_name);
    printf("[%s:%s] -> %lu\n", name, sname, i);
  }

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