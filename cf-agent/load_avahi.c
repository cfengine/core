#include "load_avahi.h"

int loadavahi()
{
    char path[256] = { 0 };
#if __x86_64__
    snprintf(path, 256, "/usr/lib/x86_64-linux-gnu/libavahi-client.so.3");
#endif

#ifndef __x86_64__
    snprintf(path, 256, "/usr/lib/libavahi-client.so.3");
#endif
    static bool tried;
    if (!tried)
    {
        avahi_handle = dlopen(path, RTLD_LAZY);

        if (!avahi_handle)
        {
            tried = false;
            return 1;
        }

        tried = true;

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

    return 1;
}
