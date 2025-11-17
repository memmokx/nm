#include <nm/opt.h>
#include <stddef.h>

opt_t nm_opt(const char* flags) {
  opt_t opt = {};

  for (size_t i = 0; flags[i]; i++)
    opt.lut[(unsigned char)flags[i]] = true;

  return opt;
}

int opt_next(opt_t* o, int argc, char** argv) {
  if (o->argc == 0)
    o->argc = 1;
  if (o->argc == argc)
    goto end;

  const auto arg = argv[o->argc];
  if (arg[0] != '-' || !arg[1])
    goto end;
  // -- end of flags
  if (arg[1] == '-' && !arg[2]) {
    o->argc++;
    goto end;
  }

  if (o->argp == 0)
    o->argp = 1;
  const auto opt = arg[o->argp++];
  if (!arg[o->argp]) {
    o->argc++;
    o->argp = -1;
  }

  if (!o->lut[(unsigned char)opt])
    return OPT_UNKNOWN;
  return opt;

end:
  return OPT_END;
}