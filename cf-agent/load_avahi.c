#include "load_avahi.h"
#include "files_interfaces.h"

#include <stdlib.h>

static const char *paths[3] = {
    "/usr/lib/x86_64-linux-gnu/libavahi-client.so.3",
    "/usr/lib/libavahi-client.so.3",
    "/usr/lib64/libavahi-client.so.3"
};

static const char *getavahipath();

int loadavahi()
{
    const char *path = getavahipath();

    avahi_handle = dlopen(path, RTLD_LAZY);

    if (!avahi_handle)
    {
        return -1;
    }

    avahi_simple_poll_quit_ptr = dlsym(avahi_handle, "avahi_simple_poll_quit");
    avahi_address_snprint_ptr = dlsym(avahi_handle, "avahi_address_snprint");
    avahi_service_resolver_free_ptr = dlsym(avahi_handle, "avahi_service_resolver_free");
    avahi_client_errno_ptr = dlsym(avahi_handle, "avahi_client_errno");
    avahi_strerror_ptr = dlsym(avahi_handle, "avahi_strerror");
    avahi_service_resolver_new_ptr = dlsym(avahi_handle, "avahi_service_resolver_new");
    avahi_service_browser_get_client_ptr = dlsym(avahi_handle, "avahi_service_browser_get_client");
    avahi_service_resolver_get_client_ptr = dlsym(avahi_handle, "avahi_service_resolver_get_client");
    avahi_simple_poll_new_ptr = dlsym(avahi_handle, "avahi_simple_poll_new");
    avahi_simple_poll_get_ptr = dlsym(avahi_handle, "avahi_simple_poll_get");
    avahi_client_new_ptr = dlsym(avahi_handle, "avahi_client_new");
    avahi_simple_poll_loop_ptr = dlsym(avahi_handle, "avahi_simple_poll_loop");
    avahi_service_browser_free_ptr = dlsym(avahi_handle, "avahi_service_browser_free");
    avahi_client_free_ptr = dlsym(avahi_handle, "avahi_client_free");
    avahi_simple_poll_free_ptr = dlsym(avahi_handle, "avahi_simple_poll_free");
    avahi_service_browser_new_ptr = dlsym(avahi_handle, "avahi_service_browser_new");

    return 0;
}

static const char *getavahipath()
{
    const char *env = getenv("AVAHI_PATH");
    struct stat sb;

    if (cfstat(env, &sb) == 0)
    {
        return env;
    }

    for (int i = 0; i < 3; i++)
    {
        if (cfstat(paths[i], &sb) == 0)
        {
            return paths[i];
        }
    }

    return NULL;
}
