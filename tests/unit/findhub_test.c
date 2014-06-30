#include <test.h>

#include <string.h>
#include <findhub.h>
#include <misc_lib.h>                                          /* xsnprintf */

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/address.h>


void (*avahi_simple_poll_quit_ptr)(AvahiSimplePoll *);
char* (*avahi_address_snprint_ptr)(char *, size_t , const AvahiAddress *);
int (*avahi_service_resolver_free_ptr)(AvahiServiceResolver *);
int (*avahi_client_errno_ptr)(AvahiClient *);
const char* (*avahi_strerror_ptr)(int);
AvahiServiceResolver* (*avahi_service_resolver_new_ptr)(AvahiClient *, AvahiIfIndex, AvahiProtocol, const char *, const char *, const char *, AvahiProtocol, AvahiLookupFlags, AvahiServiceResolverCallback, void *);
AvahiClient* (*avahi_service_browser_get_client_ptr)(AvahiServiceBrowser *);
AvahiClient* (*avahi_service_resolver_get_client_ptr)(AvahiServiceResolver *);
AvahiSimplePoll* (*avahi_simple_poll_new_ptr)();
const AvahiPoll* (*avahi_simple_poll_get_ptr)(AvahiSimplePoll *s);
AvahiClient* (*avahi_client_new_ptr)(const AvahiPoll *, AvahiClientFlags, AvahiClientCallback, void *, int *);
int (*avahi_simple_poll_loop_ptr)(AvahiSimplePoll *);
int (*avahi_service_browser_free_ptr)(AvahiServiceBrowser *);
void (*avahi_client_free_ptr)(AvahiClient *client);
void (*avahi_simple_poll_free_ptr)(AvahiSimplePoll *);
AvahiServiceBrowser* (*avahi_service_browser_new_ptr)(AvahiClient *, AvahiIfIndex, AvahiProtocol, const char *, const char *, AvahiLookupFlags, AvahiServiceBrowserCallback, void*);

int hostcount;

void *dlopen(const char *filename, int flag)
{
    return (void*)1;
}

void *dlsym(void *handle, const char *symbol)
{
    if (strcmp(symbol, "avahi_simple_poll_quit") == 0)
    {
        return &avahi_simple_poll_quit;
    }
    else if (strcmp(symbol, "avahi_address_snprint") == 0)
    {
        return &avahi_address_snprint;
    }
    else if (strcmp(symbol, "avahi_service_resolver_free") == 0)
    {
        return &avahi_service_resolver_free;
    }
    else if (strcmp(symbol, "avahi_client_errno") == 0)
    {
        return &avahi_client_errno;
    }
    else if (strcmp(symbol, "avahi_strerror") == 0)
    {
        return &avahi_strerror;
    }
    else if (strcmp(symbol, "avahi_service_resolver_new") == 0)
    {
        return &avahi_service_resolver_new;
    }
    else if (strcmp(symbol, "avahi_service_browser_get_client") == 0)
    {
        return &avahi_service_browser_get_client;
    }
    else if (strcmp(symbol, "avahi_service_resolver_get_client") == 0)
    {
        return &avahi_service_resolver_get_client;
    }
    else if (strcmp(symbol, "avahi_simple_poll_new") == 0)
    {
        return &avahi_simple_poll_new;
    }
    else if (strcmp(symbol, "avahi_simple_poll_get") == 0)
    {
        return &avahi_simple_poll_get;
    }
    else if (strcmp(symbol, "avahi_client_new") == 0)
    {
        return &avahi_client_new;
    }
    else if (strcmp(symbol, "avahi_simple_poll_loop") == 0)
    {
        return &avahi_simple_poll_loop;
    }
    else if (strcmp(symbol, "avahi_service_browser_free") == 0)
    {
        return &avahi_service_browser_free;
    }
    else if (strcmp(symbol, "avahi_client_free") == 0)
    {
        return &avahi_client_free;
    }
    else if (strcmp(symbol, "avahi_simple_poll_free") == 0)
    {
        return &avahi_simple_poll_free;
    }
    else if (strcmp(symbol, "avahi_service_browser_new") == 0)
    {
        return &avahi_service_browser_new;
    }
    
    return NULL;
}

int dlclose(void *handle)
{
    return 0;
}

AvahiSimplePoll *avahi_simple_poll_new()
{
    AvahiSimplePoll *sp = (AvahiSimplePoll*)1;
    
    return sp;
}

void avahi_simple_poll_free(AvahiSimplePoll *poll)
{

}

const AvahiPoll *avahi_simple_poll_get(AvahiSimplePoll *s)
{
    AvahiPoll *p = { 0 };

    return p;
}

void avahi_simple_poll_quit(AvahiSimplePoll *poll)
{

}

char *avahi_address_snprint(char *buffer, size_t size, const AvahiAddress *address)
{
    xsnprintf(buffer, size, "10.0.0.100");

    return NULL;
}

int avahi_service_resolver_free(AvahiServiceResolver *resolver)
{
   return 0;
}

int avahi_client_errno(AvahiClient *c)
{
    return 1;
}

const char *avahi_strerror(int error)
{
    return "Avahi error occured";
}

AvahiServiceResolver *avahi_service_resolver_new(AvahiClient *c, AvahiIfIndex index, AvahiProtocol protocol, const char * s1, const char *s2, const char *s3, 
                                                 AvahiProtocol protocol2, AvahiLookupFlags flags, AvahiServiceResolverCallback callback, void *data)
{
    return (AvahiServiceResolver *)1;
}

AvahiClient *avahi_service_browser_get_client(AvahiServiceBrowser *sb)
{
    return (AvahiClient *)1;
}

AvahiClient *avahi_service_resolver_get_client(AvahiServiceResolver *sr)
{
    return (AvahiClient *)1;
}

AvahiClient *avahi_client_new(const AvahiPoll *poll, AvahiClientFlags cf, AvahiClientCallback callback, void *data, int *stat)
{
    if (hostcount == 4)
    {
        return NULL;
    }

    return (AvahiClient *)1;
}

int avahi_simple_poll_loop(AvahiSimplePoll *sp)
{
    AvahiAddress *addr = calloc(1, sizeof(AvahiAddress));
    AvahiServiceResolver *sr = { (AvahiServiceResolver*)1 };
    switch(hostcount)
    {
    case 1:
        resolve_callback(sr, 0, 0, AVAHI_RESOLVER_FOUND, "cfenginehub", "tcp", "local", "host1", addr, 5308, NULL, 0, NULL);
        return 0;

    case 2:
        resolve_callback(sr, 0, 0, AVAHI_RESOLVER_FOUND, "cfenginehub", "tcp", "local", "host1", addr, 5308, NULL, 0, NULL);
        resolve_callback(sr, 0, 0, AVAHI_RESOLVER_FOUND, "cfenginehub", "tcp", "local", "host2", addr, 1234, NULL, 0, NULL);
        resolve_callback(sr, 0, 0, AVAHI_RESOLVER_FOUND, "cfenginehub", "tcp", "local", "host3", addr, 4321, NULL, 0, NULL);
        return 0;

    default:
        free(addr);
    };

    return 0;
}

int avahi_service_browser_free(AvahiServiceBrowser *sb)
{
    return 0;
}

void avahi_client_free(AvahiClient *c)
{

}

AvahiServiceBrowser *avahi_service_browser_new(AvahiClient *c, AvahiIfIndex index, AvahiProtocol protocol, const char *s1, const char *s2, AvahiLookupFlags flags, 
                                               AvahiServiceBrowserCallback callback, void *data)
{
    AvahiServiceBrowser *browser = (AvahiServiceBrowser *)1;
    
    return browser;
}

static void test_noHubsFound(void)
{
    List *list = NULL;
    
    hostcount = 0;

    assert_int_equal(ListHubs(&list), 0);
    assert_int_not_equal(list, NULL);

    ListDestroy(&list);
}

static void test_oneHubFound(void)
{
    List *list = NULL;

    hostcount = 1;

    assert_int_equal(ListHubs(&list), 1);
    assert_int_not_equal(list, NULL);
    
    ListIterator *i = NULL;
    i = ListIteratorGet(list);
    assert_true(i != NULL);
    HostProperties *host = (HostProperties *)ListIteratorData(i);
    
    assert_int_equal(host->Port,5308);
    assert_string_equal(host->Hostname, "host1");
    assert_string_equal(host->IPAddress, "10.0.0.100");

    ListIteratorDestroy(&i);
    ListDestroy(&list);
}

static void test_multipleHubsFound(void)
{
    List *list = NULL;

    hostcount = 2;

    assert_int_equal(ListHubs(&list), 3);
    assert_int_not_equal(list, NULL);
    
    ListIterator *i = NULL;
    i = ListIteratorGet(list);
    
    HostProperties *host1 = (HostProperties *)ListIteratorData(i); 
    assert_int_not_equal(ListIteratorNext(i), -1);
    HostProperties *host2 = (HostProperties *)ListIteratorData(i); 
    assert_int_not_equal(ListIteratorNext(i), -1);
    HostProperties *host3 = (HostProperties *)ListIteratorData(i); 

    assert_int_equal(host1->Port, 5308);
    assert_string_equal(host1->Hostname, "host1");
    assert_string_equal(host1->IPAddress, "10.0.0.100");

    assert_int_equal(host2->Port, 1234);
    assert_string_equal(host2->Hostname, "host2");
    assert_string_equal(host2->IPAddress, "10.0.0.100");

    assert_int_equal(host3->Port, 4321);
    assert_string_equal(host3->Hostname, "host3");
    assert_string_equal(host3->IPAddress, "10.0.0.100");

    ListIteratorDestroy(&i);
    ListDestroy(&list);
}

static void test_errorOccurred(void)
{
    List *list = NULL;

    hostcount = 4;

    assert_int_equal(ListHubs(&list), -1);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
          unit_test(test_noHubsFound),
          unit_test(test_oneHubFound),
          unit_test(test_multipleHubsFound),
          unit_test(test_errorOccurred)
    };

    return run_tests(tests);
}
