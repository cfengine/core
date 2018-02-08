#ifndef CFENGINE_CFNET_SERVER_CODE_H
#define CFENGINE_CFNET_SERVER_CODE_H

#include <platform.h>

int InitServer(size_t queue_size);
int WaitForIncoming(int sd);

#endif
