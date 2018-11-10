#ifndef __LMDUMP_H__
#define __LMDUMP_H__

#include <platform.h>

typedef enum
{
    LMDUMP_KEYS_ASCII,
    LMDUMP_VALUES_ASCII,
    LMDUMP_VALUES_HEX,
    LMDUMP_SIZES,
    LMDUMP_UNKNOWN
} lmdump_mode;

lmdump_mode lmdump_char_to_mode(char mode);
int lmdump(lmdump_mode mode, const char *file);
int lmdump_main(int argc, char * argv[]);

#endif
