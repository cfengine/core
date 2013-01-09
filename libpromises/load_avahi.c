#include "load_avahi.h"

void *checkavahi()
{
    char path[256] = { 0 };
#if __x86_64__
    snprintf(path, 256, "/usr/lib/x86_64-linux-gnu/libavahi-client.so.3");
#endif

#ifndef __x86_64__
    snprintf(path, 256, "/usr/lib/libavahi-client.so.3");
#endif

    void *handle = dlopen(path, RTLD_LAZY);

    if (!handle)
    {
        return NULL;
    }

    return handle;
}

bool load_avahi_simple_poll_quit(AvahiSimplePoll *s)
{
    void (*avahi_simple_poll_quit_handle)(AvahiSimplePoll *);
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return false;
    }

    avahi_simple_poll_quit_handle = dlsym(lib_handle, "avahi_simple_poll_quit");
    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return false;
    }

    (*avahi_simple_poll_quit_handle)(s);

    dlclose(lib_handle);

    return true;
}

bool load_avahi_service_resolver_free(AvahiServiceResolver *r)
{
    int (*avahi_service_resolver_free_handle)(AvahiServiceResolver *);

    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return false;
    }

    avahi_service_resolver_free_handle = dlsym(lib_handle, "avahi_service_resolver_free");

    char *error = dlerror();
    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return false;
    }

    (*avahi_service_resolver_free_handle)(r);

    dlclose(lib_handle);

    return true;
}

char *get_avahi_error(AvahiClient *c)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return false;
    }

    int (*avahi_client_errno_handle)(AvahiClient *);

    avahi_client_errno_handle = dlsym(lib_handle, "avahi_client_errno");
    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return NULL;
    }

    int errcode = (*avahi_client_errno_handle)(c);

    char* (*avahi_strerror_handle)(int);

    avahi_strerror_handle = dlsym(lib_handle, "avahi_strerror");
    error = dlerror();
    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return NULL;
    }

    char *errorstring = (*avahi_strerror_handle)(errcode);

    dlclose(lib_handle);

    return errorstring;
}

bool load_avahi_address_snprint(char *ret_s, size_t lenght, const AvahiAddress *a)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return false;
    }

    char* (*avahi_address_snprint_handle)(char *, size_t, const AvahiAddress *);

    avahi_address_snprint_handle = dlsym(lib_handle, "avahi_address_snprint");

    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return false;
    }

    (*avahi_address_snprint_handle)(ret_s, lenght, a);

    dlclose(lib_handle);

    return true;
}

AvahiServiceResolver *load_avahi_service_resolver_new(AvahiClient *client,
                                                      AvahiIfIndex interface,
                                                      AvahiProtocol protocol,
                                                      const char *name,
                                                      const char *type,
                                                      const char *domain,
                                                      AvahiProtocol aprotocol,
                                                      AvahiLookupFlags flags,
                                                      AvahiServiceResolverCallback callback,
                                                      void *userdata)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return NULL;
    }

    AvahiServiceResolver* (*avahi_service_resolver_new_handle)(AvahiClient *, AvahiIfIndex , AvahiProtocol, const char *, const char *, const char *, AvahiProtocol, AvahiLookupFlags, AvahiServiceResolverCallback, void*);

    avahi_service_resolver_new_handle = dlsym(lib_handle, "avahi_service_resolver_new");

    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return NULL;
    }

    AvahiServiceResolver *retval = (*avahi_service_resolver_new_handle)(client, interface, protocol, name, type, domain, aprotocol, flags, callback, userdata);

    dlclose(lib_handle);

    return retval;
}

AvahiClient *load_avahi_service_browser_get_client(AvahiServiceBrowser *browser)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return NULL;
    }

    AvahiClient* (*avahi_service_browser_get_client_handle)(AvahiServiceBrowser *);

    avahi_service_browser_get_client_handle = dlsym(lib_handle, "avahi_service_browser_get_client");

    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return NULL;
    }

    AvahiClient *retval = (*avahi_service_browser_get_client_handle)(browser);

    dlclose(lib_handle);

    return retval;
}

AvahiClient *load_avahi_service_resolver_get_client(AvahiServiceResolver *resolver)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return NULL;
    }

    AvahiClient* (*avahi_service_resolver_get_client_handle)(AvahiServiceResolver *);

    avahi_service_resolver_get_client_handle = dlsym(lib_handle, "avahi_service_resolver_get_client");

    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return NULL;
    }

    AvahiClient *retval = (*avahi_service_resolver_get_client_handle)(resolver);

    dlclose(lib_handle);

    return retval;
}

const char *load_avahi_strerror(int errorcode)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return NULL;
    }

    const char* (*avahi_strerror_handle)(int);
    avahi_strerror_handle = dlsym(lib_handle, "avahi_strerror");

    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return NULL;
    }

    const char *retval = (*avahi_strerror_handle)(errorcode);

    return retval;
}

AvahiSimplePoll *load_avahi_simple_poll_new(void)
{
   void *lib_handle = checkavahi();

   if (lib_handle == NULL)
   {
       return NULL;
   }

   AvahiSimplePoll* (*avahi_simple_poll_new_handle)();

   avahi_simple_poll_new_handle = dlsym(lib_handle, "avahi_simple_poll_new");

   char *error = dlerror();

   if (error != NULL)
   {
       fprintf(stderr, "%s\n", error);
       dlclose(lib_handle);
       return NULL;
   }

   AvahiSimplePoll *retval = (*avahi_simple_poll_new_handle)();

   return retval;
}

const AvahiPoll *load_avahi_simple_poll_get(AvahiSimplePoll *s)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return NULL;
    }

    const AvahiPoll* (*avahi_simple_poll_get_handle)(AvahiSimplePoll *);

    avahi_simple_poll_get_handle = dlsym(lib_handle, "avahi_simple_poll_get");

    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return NULL;
    }

    const AvahiPoll *retval = (*avahi_simple_poll_get_handle)(s);

    dlclose(lib_handle);

    return retval;
}

AvahiClient *load_avahi_client_new(const AvahiPoll *poll_api, AvahiClientFlags flags, AvahiClientCallback callback, void *userdata, int *errorcode)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return NULL;
    }

    AvahiClient* (*avahi_client_new_handle)(const AvahiPoll *, AvahiClientFlags, AvahiClientCallback, void* ,int*);

    avahi_client_new_handle = dlsym(lib_handle, "avahi_client_new");

    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return NULL;
    }

    AvahiClient *retval = (*avahi_client_new_handle)(poll_api, flags, callback, userdata, errorcode);

    dlclose(lib_handle);

    return retval;
}

int load_avahi_simple_poll_loop(AvahiSimplePoll *s)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return -1;
    }

    int (*avahi_simple_poll_loop_handle)(AvahiSimplePoll *);

    avahi_simple_poll_loop_handle = dlsym(lib_handle, "avahi_simple_poll_loop");

    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return -1;
    }

    int retval = (*avahi_simple_poll_loop_handle)(s);

    dlclose(lib_handle);

    return retval;
}

int load_avahi_service_browser_free(AvahiServiceBrowser *sb)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return -1;
    }

    int (*avahi_service_browser_free_handle)(AvahiServiceBrowser *);

    avahi_service_browser_free_handle = dlsym(lib_handle, "avahi_service_browser_free");

    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return -1;
    }

    int retval = (*avahi_service_browser_free_handle)(sb);

    dlclose(lib_handle);

    return retval;
}

void load_avahi_client_free(AvahiClient *client)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return;
    }

    void (*avahi_client_free_handle)(AvahiClient *);

    avahi_client_free_handle = dlsym(lib_handle, "avahi_client_free");

    char *error = dlerror();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return;
    }

    (*avahi_client_free_handle)(client);

    dlclose(lib_handle);
}

void load_avahi_simple_poll_free(AvahiSimplePoll *s)
{
    void *lib_handle = checkavahi();

    if (lib_handle == NULL)
    {
        return;
    }

    void (*avahi_simple_poll_free_handle)(AvahiSimplePoll *);

    avahi_simple_poll_free_handle = dlsym(lib_handle, "avahi_simple_poll_free");

    char *error = checkavahi();

    if (error != NULL)
    {
        fprintf(stderr, "%s\n", error);
        dlclose(lib_handle);
        return;
    }

    (*avahi_simple_poll_free_handle)(s);

    dlclose(lib_handle);
}

AvahiServiceBrowser *load_avahi_service_browser_new(AvahiClient *client,
                                                    AvahiIfIndex interface,
                                                    AvahiProtocol protocol,
                                                    const char *type,
                                                    const char *domain,
                                                    AvahiLookupFlags flags,
                                                    AvahiServiceBrowserCallback callback,
                                                    void *userdata)
{
    void *lib_handle = checkavahi();
    if (lib_handle == NULL)
    {
        return NULL;
    }

    AvahiServiceBrowser* (*avahi_service_browser_new_handle)(AvahiClient *, AvahiIfIndex, AvahiProtocol, const char *, const char *, AvahiLookupFlags, AvahiServiceBrowserCallback, void *);

    avahi_service_browser_new_handle = dlsym(lib_handle, "avahi_service_browser_new");

    char *error = dlerror();

    if (error != NULL)
    {
        printf("error:%s\n", error);
        dlclose(lib_handle);
        return NULL;
    }

    AvahiServiceBrowser *retval = (*avahi_service_browser_new_handle)(client, interface, protocol, type, domain, flags, callback, userdata);

    dlclose(lib_handle);

    return retval;
}
