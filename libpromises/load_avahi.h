#ifndef LOAD_AVAHI_H
#define LOAD_AVAHI_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/address.h>

void *checkavahi();
bool load_avahi_simple_poll_quit(AvahiSimplePoll *s);
bool load_avahi_service_resolver_free(AvahiServiceResolver *r);
char *get_avahi_error(AvahiClient *c);
bool load_avahi_address_snprint(char * ret_s, size_t lenght, const AvahiAddress *a);
AvahiServiceResolver *load_avahi_service_resolver_new(AvahiClient *client,
                                                  AvahiIfIndex interface,
                                                  AvahiProtocol protocol,
                                                  const char *name,
                                                  const char *type,
                                                  const char *domain,
                                                  AvahiProtocol aprotocol,
                                                  AvahiLookupFlags flags,
                                                  AvahiServiceResolverCallback callback,
                                                  void *userdata);
AvahiClient *load_avahi_service_browser_get_client(AvahiServiceBrowser *browser);
AvahiClient *load_avahi_service_resolver_get_client(AvahiServiceResolver *resolver);
const char *load_avahi_strerror(int errorcode);
AvahiSimplePoll *load_avahi_simple_poll_new(void);
const AvahiPoll *load_avahi_simple_poll_get(AvahiSimplePoll *s);
AvahiClient *load_avahi_client_new(const AvahiPoll *poll_api, AvahiClientFlags flags, AvahiClientCallback callback, void *userdata, int *errorcode);
int load_avahi_simple_poll_loop(AvahiSimplePoll *s);
int load_avahi_service_browser_free(AvahiServiceBrowser *sb);
void load_avahi_client_free(AvahiClient *client);
void load_avahi_simple_poll_free(AvahiSimplePoll *s);
AvahiServiceBrowser *load_avahi_service_browser_new(AvahiClient *client,
                                                    AvahiIfIndex interface,
                                                    AvahiProtocol protocol,
                                                    const char *type,
                                                    const char *domain,
                                                    AvahiLookupFlags flags,
                                                    AvahiServiceBrowserCallback callback,
                                                    void *userdata);

#endif // LOAD_AVAHI_H
