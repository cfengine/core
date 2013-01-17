#ifndef FINDHUB_PRIV_H
#define FINDHUB_PRIV_H

#include "load_avahi.h"
#include "list.h"

AvahiSimplePoll *spoll;

typedef struct
{
    char Hostname[4096];
    char IPAddress[AVAHI_ADDRESS_STR_MAX];
    uint16_t Port;
} HostProperties;

void resolve_callback(AvahiServiceResolver *r,
                      AVAHI_GCC_UNUSED AvahiIfIndex interface,
                      AVAHI_GCC_UNUSED AvahiProtocol protocol,
                      AvahiResolverEvent event,
                      const char *name,
                      const char *type,
                      const char *domain,
                      const char *host_name,
                      const AvahiAddress *address,
                      uint16_t port,
                      AvahiStringList *txt,
                      AvahiLookupFlags flags,
                      AVAHI_GCC_UNUSED void* userdata
                      );
void browse_callback(AvahiServiceBrowser *b,
                     AvahiIfIndex interface,
                     AvahiProtocol protocol,
                     AvahiBrowserEvent event,
                     const char *name,
                     const char *type,
                     const char *domain,
                     AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
                     void *userdata);
void client_callback(AvahiClient *c,
                     AvahiClientState state,
                     AVAHI_GCC_UNUSED void *userdata);
#endif // FINDHUB_PRIV_H
