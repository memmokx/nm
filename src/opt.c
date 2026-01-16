#include <nm/opt.h>
#include <stddef.h>

opt_t nm_opt(const char* flags) {
  opt_t opt = {};

  for (size_t i = 0; flags[i]; i++)
    opt.lut[(unsigned char)flags[i]] = true;

  return opt;
}

int opt_next(opt_t* o, int argc, char** argv) {
  while (o->argc < argc) {
    const auto arg = argv[o->argc];

    if (o->argp != 0) {
      const auto opt = arg[o->argp++];
      if (!arg[o->argp]) {
        o->argc++;
        o->argp = 0;
      }

      if (!o->lut[(unsigned char)opt])
        return OPT_UNKNOWN;

      return opt;
    }

    // -- means end of flags
    if (arg[0] == '-' && arg[1] == '-' && !arg[2]) {
      o->argc++;
      o->flagend = true;
      continue;
    }

    // no more flags to process OR the value doesnt look like a flag
    if (o->flagend || arg[0] != '-' || !arg[1]) {
      // shift file
      for (int i = o->argc; i > o->args; i--)
        argv[i] = argv[i - 1];

      argv[o->args++] = arg;
      o->argc++;
      continue;
    }

    o->argp = 1;
    const auto opt = arg[o->argp++];
    if (!arg[o->argp]) {
      o->argc++;
      o->argp = 0;
    }

    if (!o->lut[(unsigned char)opt])
      return OPT_UNKNOWN;

    return opt;
  }

  return OPT_END;
}
