#ifndef NM_OPT_H
#define NM_OPT_H

#include <stdint.h>

#define OPT_END (-1)
#define OPT_UNKNOWN (-2)

typedef struct {
  bool lut[UINT8_MAX];
  int argc;
  int argp;
} opt_t;

opt_t nm_opt(const char* flags);
int opt_next(opt_t* o, int argc, char** argv);

#endif
