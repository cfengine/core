#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>                                        /* strlen,strchr */
#include <stddef.h>                                             /* size_t */


char *strchrnul(const char *s, int c)
{
    char *p = strchr(s, c);

    if (p == NULL)
    {
        return (char *)(s + strlen(s));
    }
    else
    {
        return p;
    }
}
