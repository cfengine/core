/*
   Copyright 2018 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <findhub.h>
#include <cleanup.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <logging.h>
#include <alloc.h>

List *hublist = NULL; 

static void AtExitDlClose(void);
static int CompareHosts(const void  *a, const void *b);

void client_callback(AvahiClient *c,
                     AvahiClientState state,
                     AVAHI_GCC_UNUSED void *userdata)
{
    assert(c);

    if (state == AVAHI_CLIENT_FAILURE)
    {
        Log(LOG_LEVEL_ERR, "Server connection failure '%s'", avahi_strerror_ptr(avahi_client_errno_ptr(c)));
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
        Log(LOG_LEVEL_ERR, "Avahi browser error '%s'", avahi_strerror_ptr(avahi_client_errno_ptr(avahi_service_browser_get_client_ptr(b))));
        avahi_simple_poll_quit_ptr(spoll);
        return;

    case AVAHI_BROWSER_NEW:
        if ( !(avahi_service_resolver_new_ptr(c, interface, protocol, name,
                                             type, domain, AVAHI_PROTO_UNSPEC, 0,
                                             (AvahiServiceResolverCallback) resolve_callback, c)) )
        {
            Log(LOG_LEVEL_ERR, "Failed to resolve service '%s', error '%s'", name, avahi_strerror_ptr(avahi_client_errno_ptr(c)));
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
                      AVAHI_GCC_UNUSED AvahiStringList *txt,
                      AVAHI_GCC_UNUSED AvahiLookupFlags flags,
                      AVAHI_GCC_UNUSED void* userdata)
{
    HostProperties *hostprop = xcalloc(1, sizeof(HostProperties));
    assert(r);
    char a[AVAHI_ADDRESS_STR_MAX];
    switch(event)
    {
    case AVAHI_RESOLVER_FAILURE:
        Log(LOG_LEVEL_ERR, "In Avahi resolver, failed to resolve service '%s' of type '%s' in domain '%s' (error: '%s')",
              name, type, domain, avahi_strerror_ptr(avahi_client_errno_ptr(avahi_service_resolver_get_client_ptr(r))));
        break;

    case AVAHI_RESOLVER_FOUND:
        avahi_address_snprint_ptr(a, sizeof(a), address);

        strlcpy(hostprop->Hostname, host_name, 4096);
        strlcpy(hostprop->IPAddress, a, AVAHI_ADDRESS_STR_MAX);
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

        Log(LOG_LEVEL_NOTICE, "CFEngine Policy Server: hostname '%s', IP address '%s', port %d",
            hostprop->Hostname, hostprop->IPAddress, hostprop->Port);
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

    RegisterCleanupFunction(&AtExitDlClose);

    if (loadavahi() == -1)
    {
        Log(LOG_LEVEL_ERR, "Avahi was not found");
        return -1;
    }

    if (!(spoll = avahi_simple_poll_new_ptr()))
    {
        Log(LOG_LEVEL_ERR, "Failed to create simple poll object.");

        if (spoll)
        {
            avahi_simple_poll_free_ptr(spoll);
        }
        return -1;
    }

    client = avahi_client_new_ptr(avahi_simple_poll_get_ptr(spoll), 0, client_callback, NULL, &error);

    if (!client)
    {
        Log(LOG_LEVEL_ERR, "Failed to create client '%s'", avahi_strerror_ptr(error));

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
        Log(LOG_LEVEL_ERR, "Failed to create service browser '%s'", avahi_strerror_ptr(avahi_client_errno_ptr(client)));
        
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
