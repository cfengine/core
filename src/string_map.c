#include "cf3.defs.h"
#include "prototypes3.h"

#include "string_map.h"

TYPED_MAP_DEFINE(String, char *, char *,
                 (MapHashFn)&OatHash,
                 (MapKeyEqualFn)&StringSafeEqual,
                 &free,
                 &free)

