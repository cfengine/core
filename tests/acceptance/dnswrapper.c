#include <netdb.h>
#include <stdlib.h>

struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type)
{
    h_errno = HOST_NOT_FOUND;
    return NULL;
}
