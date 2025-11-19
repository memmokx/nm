# nm

Implementation of the `nm` command from binutils.

As re-creating `strcoll` is out of scope for this project, invocations of the original `nm` binary MUST set the
`LC_ALL` variable to `C`

## Allowed functions

- open, close
- mmap, munmap
- write
- fstat
- malloc, free
- exit
- perror, strerror
- getpagesize

## Mandatory part

Reproduce the functionality of `nm` without any flags, it should handle any ELF object in 32 and 64 bits. (no archives)

## Bonus part

Support the following flags:

- `-a`
- `-g`
- `-u`
- `-r`
- `-p`
- `-D` 