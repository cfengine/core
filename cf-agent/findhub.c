#include "findhub.h"
#include "atexit.h"
#include "string_lib.h"

List *hublist = NULL; 

static void AtExitDlClose(void);
static int CompareHosts(void  *a, void *b);

bool isIPv6(const char *address)
{
    if (strchr(address, ':') == NULL)
    {
        return false;
    }

    return true;
}

void client_callback(AvahiClient *c,
                     AvahiClientState state,
                     void *userdata)
{
    assert(c);

    if (state == AVAHI_CLIENT_FAILURE)
    {
        fprintf(stderr, "Server connection failure: %s\n", avahi_strerror_ptr(avahi_client_errno_ptr(c)));
        avahi_simple_poll_quit_ptr(spoll);
    }
}

void browse_callback(AvahiServiceBrowser *b,
                     AvahiIfIndex interface,
                     AvahiProtocol protocol,
                     AvahiBrowserEvent event,
                     const char *name,
                     const char *type,
                     const char *domain,
                     AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
                     void *userdata)
{
    AvahiClient *c = userdata;
    assert(b);

    switch(event)
    {
    case AVAHI_BROWSER_FAILURE:
        fprintf(stderr, "Error: %s\n", avahi_strerror_ptr(avahi_client_errno_ptr(avahi_service_browser_get_client_ptr(b))));
        avahi_simple_poll_quit_ptr(spoll);
        return;

    case AVAHI_BROWSER_NEW:
        if (!(avahi_service_resolver_new_ptr(c, interface, protocol, name ,type, domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, c)))
        {
            fprintf(stderr, "Failed to resolve service '%s': %s\n", name, avahi_strerror_ptr(avahi_client_errno_ptr(c)));
        }
        break;

    case AVAHI_BROWSER_REMOVE:
        break;

    case AVAHI_BROWSER_ALL_FOR_NOW:
        avahi_simple_poll_quit_ptr(spoll);
        break;

    case AVAHI_BROWSER_CACHE_EXHAUSTED:
        break;
    }
}

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
                      )
{
    HostProperties *hostprop = calloc(1, sizeof(HostProperties));
    assert(r);
    char a[AVAHI_ADDRESS_STR_MAX];
    switch(event)
    {
    case AVAHI_RESOLVER_FAILURE:
        fprintf(stderr, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror_ptr(avahi_client_errno_ptr(avahi_service_resolver_get_client_ptr(r))));
        break;

    case AVAHI_RESOLVER_FOUND:
        avahi_address_snprint_ptr(a, sizeof(a), address);

        strncpy(hostprop->Hostname, host_name, 4096);
        strncpy(hostprop->IPAddress, a, AVAHI_ADDRESS_STR_MAX);
        hostprop->Port = port;
        ListAppend(hublist, hostprop);
        break;
    }

    avahi_service_resolver_free_ptr(r);
}

void PrintList(List *list)
{
    ListIterator *i = NULL;

    if (ListIteratorGet(list, &i) != 0)
    {
        printf("Not able to get iterator\n");
        return;
    }

    printf("\n\n==============================================\n");
    
    while (i)
    {
        HostProperties *hostprop = (HostProperties *)i->current->payload;

        printf("CFEngine Hub:\n"
               "Hostname: %s\n"
               "IP Address: %s\n"
               "Port: %d\n",
               hostprop->Hostname,
               hostprop->IPAddress,
               hostprop->Port);
        printf("==============================================\n");
        if (ListIteratorNext(i) == -1)
            break;
    }
    printf("\n\n");
    ListIteratorDestroy(&i);
}

List *ListHubs(void)
{
    AvahiClient *client = NULL;
    AvahiServiceBrowser *sb = NULL;
    int error;

    ListNew(&hublist, &CompareHosts, NULL, &free);

    spoll = NULL;
    avahi_handle = NULL;

    RegisterAtExitFunction(&AtExitDlClose);

    if (loadavahi() == 1)
    {
        printf("Avahi not found!!!\n");
        return NULL;
    }

    if (!(spoll = avahi_simple_poll_new_ptr()))
    {
        fprintf(stderr, "Failed to create simple poll object. \n");

        if (spoll)
        {
            avahi_simple_poll_free_ptr(spoll);
        }
        return NULL;
    }

    client = avahi_client_new_ptr(avahi_simple_poll_get_ptr(spoll), 0, client_callback, NULL, &error);

    if (!client)
    {
        fprintf(stderr, "Failed to create client: %s\n", avahi_strerror_ptr(error));
        if (client)
        {
            avahi_client_free_ptr(client);
        }

        if (spoll)
        {
            avahi_simple_poll_free_ptr(spoll);
        }

        return NULL;
    }

    if (!(sb = avahi_service_browser_new_ptr(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_cfenginehub._tcp", NULL, 0, browse_callback, client)))
    {
        fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
        
        if (spoll)
        {
            avahi_simple_poll_free_ptr(spoll);
        }

        if (client)
        {
            avahi_client_free_ptr(client);
        }

        if (sb)
        {
            avahi_service_browser_free_ptr(sb);
        }

        return NULL;
    }

    avahi_simple_poll_loop_ptr(spoll);

    if (sb)
        avahi_service_browser_free_ptr(sb);
    if (client)
        avahi_client_free_ptr(client);
    if (spoll)
        avahi_simple_poll_free_ptr(spoll);
    
    return hublist;
}

static void AtExitDlClose(void)
{
    if (avahi_handle != NULL)
    {
        dlclose(avahi_handle);
    }
}

static int CompareHosts(void *a, void *b)
{
    return StringSafeCompare(((HostProperties*)a)->Hostname, ((HostProperties*)b)->Hostname);
}
