#ifndef CFENGINE_CFNET_SERVER_CODE_H
#define CFENGINE_CFNET_SERVER_CODE_H

#include <platform.h>

int InitServer(size_t queue_size, char *bind_address);
int WaitForIncoming(int sd, time_t tm_sec);

#endif
