#include "findhub.h"
#include "atexit.h"
#include "string_lib.h"
#include "logging.h"
#include "misc_lib.h"

List *hublist = NULL; 

static void AtExitDlClose(void);
static int CompareHosts(const void  *a, const void *b);

void client_callback(AvahiClient *c,
                     AvahiClientState state,
                     void *userdata)
{
    assert(c);

    if (state == AVAHI_CLIENT_FAILURE)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Server connection failure %s", avahi_strerror_ptr(avahi_client_errno_ptr(c)));
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
        CfOut(OUTPUT_LEVEL_ERROR, "", "Avahi browser error: %s", avahi_strerror_ptr(avahi_client_errno_ptr(avahi_service_browser_get_client_ptr(b))));
        avahi_simple_poll_quit_ptr(spoll);
        return;

    case AVAHI_BROWSER_NEW:
        if (!(avahi_service_resolver_new_ptr(c, interface, protocol, name ,type, domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, c)))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Failed to resolve service: '%s': %s", name, avahi_strerror_ptr(avahi_client_errno_ptr(c)));
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
    HostProperties *hostprop = xcalloc(1, sizeof(HostProperties));
    assert(r);
    char a[AVAHI_ADDRESS_STR_MAX];
    switch(event)
    {
    case AVAHI_RESOLVER_FAILURE:
        CfOut(OUTPUT_LEVEL_ERROR, "", "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s",
              name, type, domain, avahi_strerror_ptr(avahi_client_errno_ptr(avahi_service_resolver_get_client_ptr(r))));
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

    i = ListIteratorGet(list);
    if (!i)
    {
        ProgrammingError("Unable to get iterator for hub list");
        return;
    }

    do
    {
        HostProperties *hostprop = (HostProperties *)ListIteratorData(i);

        CfOut(OUTPUT_LEVEL_REPORTING, "", "\nCFEngine Policy Server:\n"
                                "Hostname: %s\n"
                                "IP Address: %s\n"
                                "Port: %d\n",
                                hostprop->Hostname,
                                hostprop->IPAddress,
                                hostprop->Port);
    } while (ListIteratorNext(i) != -1);

    ListIteratorDestroy(&i);
}

int ListHubs(List **list)
{
    AvahiClient *client = NULL;
    AvahiServiceBrowser *sb = NULL;
    int error;

    hublist = ListNew(&CompareHosts, NULL, &free);
    if (!hublist)
    {
        return -1;
    }

    spoll = NULL;
    avahi_handle = NULL;

    RegisterAtExitFunction(&AtExitDlClose);

    if (loadavahi() == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Avahi was not found");
        return -1;
    }

    if (!(spoll = avahi_simple_poll_new_ptr()))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Failed to create simple poll object.");

        if (spoll)
        {
            avahi_simple_poll_free_ptr(spoll);
        }
        return -1;
    }

    client = avahi_client_new_ptr(avahi_simple_poll_get_ptr(spoll), 0, client_callback, NULL, &error);

    if (!client)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Failed to create client %s", avahi_strerror_ptr(error));

        if (client)
        {
            avahi_client_free_ptr(client);
        }

        if (spoll)
        {
            avahi_simple_poll_free_ptr(spoll);
        }

        return -1;
    }

    if (!(sb = avahi_service_browser_new_ptr(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_cfenginehub._tcp", NULL, 0, browse_callback, client)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Failed to create service browser: %s", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
        
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

        return -1;
    }

    avahi_simple_poll_loop_ptr(spoll);

    if (sb)
        avahi_service_browser_free_ptr(sb);
    if (client)
        avahi_client_free_ptr(client);
    if (spoll)
        avahi_simple_poll_free_ptr(spoll);
    
    *list = hublist;

    return ListCount(*list);
}

static void AtExitDlClose(void)
{
    if (avahi_handle != NULL)
    {
        dlclose(avahi_handle);
    }
}

static int CompareHosts(const void *a, const void *b)
{
    return StringSafeCompare(((HostProperties*)a)->Hostname, ((HostProperties*)b)->Hostname);
}
