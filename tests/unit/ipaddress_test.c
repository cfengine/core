#include <setjmp.h>
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "cmockery.h"
#include "buffer.h"
#include "ip_address.c"
#include "ip_address.h"

static void test_ipv4(void **state)
{
    /*
     * This test will check the ipv4 parser. We will directly call to it, we will not check
     * the generic frontend.
     * Cases to test:
     * 0.0.0.0 Ok
     * 255.255.255.255 Ok
     * 1.1.1.1 Ok
     * 1.1.1.1:1 Ok
     * 1.2.3.4 Ok
     * 5.6.7.8:9 Ok
     * 10.0.0.9:0 Ok
     * 10.0.0.10:5308 Ok
     * 192.168.56.10:65535 Ok
     * 0 Fail
     * 0.1 Fail
     * 0.1.2 Fail
     * 0:0 Fail
     * 1.1.1.260 Fail
     * 1.1.260.1 Fail
     * 1.260.1.1 Fail
     * 260.1.1.1 Fail
     * 260.260.260.260 Fail
     * 1.1.1.1: Fail
     * 2.3.4.5:65536 Fail
     * a.b.c.d Fail
     */
    struct IPV4Address ipv4;
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(0, IPV4_parser("0.0.0.0", &ipv4));
    assert_int_equal(ipv4.octets[0], 0);
    assert_int_equal(ipv4.octets[1], 0);
    assert_int_equal(ipv4.octets[2], 0);
    assert_int_equal(ipv4.octets[3], 0);
    assert_int_equal(ipv4.port, 0);
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(0, IPV4_parser("255.255.255.255", &ipv4));
    assert_int_equal(ipv4.octets[0], 255);
    assert_int_equal(ipv4.octets[1], 255);
    assert_int_equal(ipv4.octets[2], 255);
    assert_int_equal(ipv4.octets[3], 255);
    assert_int_equal(ipv4.port, 0);
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(0, IPV4_parser("1.1.1.1", &ipv4));
    assert_int_equal(ipv4.octets[0], 1);
    assert_int_equal(ipv4.octets[1], 1);
    assert_int_equal(ipv4.octets[2], 1);
    assert_int_equal(ipv4.octets[3], 1);
    assert_int_equal(ipv4.port, 0);
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(0, IPV4_parser("1.1.1.1:1", &ipv4));
    assert_int_equal(ipv4.octets[0], 1);
    assert_int_equal(ipv4.octets[1], 1);
    assert_int_equal(ipv4.octets[2], 1);
    assert_int_equal(ipv4.octets[3], 1);
    assert_int_equal(ipv4.port, 1);
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(0, IPV4_parser("1.2.3.4", &ipv4));
    assert_int_equal(ipv4.octets[0], 1);
    assert_int_equal(ipv4.octets[1], 2);
    assert_int_equal(ipv4.octets[2], 3);
    assert_int_equal(ipv4.octets[3], 4);
    assert_int_equal(ipv4.port, 0);
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(0, IPV4_parser("5.6.7.8:9", &ipv4));
    assert_int_equal(ipv4.octets[0], 5);
    assert_int_equal(ipv4.octets[1], 6);
    assert_int_equal(ipv4.octets[2], 7);
    assert_int_equal(ipv4.octets[3], 8);
    assert_int_equal(ipv4.port, 9);
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(0, IPV4_parser("10.0.0.9:0", &ipv4));
    assert_int_equal(ipv4.octets[0], 10);
    assert_int_equal(ipv4.octets[1], 0);
    assert_int_equal(ipv4.octets[2], 0);
    assert_int_equal(ipv4.octets[3], 9);
    assert_int_equal(ipv4.port, 0);
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(0, IPV4_parser("10.0.0.10:5308", &ipv4));
    assert_int_equal(ipv4.octets[0], 10);
    assert_int_equal(ipv4.octets[1], 0);
    assert_int_equal(ipv4.octets[2], 0);
    assert_int_equal(ipv4.octets[3], 10);
    assert_int_equal(ipv4.port, 5308);
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(0, IPV4_parser("192.168.56.10:65535", &ipv4));
    assert_int_equal(ipv4.octets[0], 192);
    assert_int_equal(ipv4.octets[1], 168);
    assert_int_equal(ipv4.octets[2], 56);
    assert_int_equal(ipv4.octets[3], 10);
    assert_int_equal(ipv4.port, 65535);
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("0", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("0.1", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("0.1.2", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("0:0", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("1.1.1.260", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("1.1.260.1", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("1.260.1.1", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("260.1.1.1", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("260.260.260.260", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("1.1.1.1:", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("2.3.4.5:65536", &ipv4));
    memset(&ipv4, 0, sizeof(struct IPV4Address));
    assert_int_equal(-1, IPV4_parser("a.b.c.d", &ipv4));
}

static void test_char2hex(void **state)
{
    /*
     * Even if this routine is pretty simple, this is a sanity
     * check. This routine does not detect errors so we only
     * try valid conversions.
     * This routine does not expect the '0x' in front of numbers,
     * so our tests will be:
     * 0 .. 9
     * a .. f
     * A .. F
     */
    char i = '0';
    for (i = '0'; i <= '9'; ++i)
        assert_int_equal(i - '0', Char2Hex(0, i));
    for (i = 'a'; i <= 'f'; ++i)
        assert_int_equal(i - 'a' + 0x0A, Char2Hex(0, i));
}

static void test_ipv6(void **state)
{
    /*
     * This test will check the ipv6 parser. We will directly call to it, we will not check
     * the generic frontend.
     * Cases to test:
     * 0000:0000:0000:0000:0000:0000:0000:0000 Ok
     * 0:0:0:0:0:0:0:0 Ok
     * a:b:c:d::1 Ok
     * a:b:c:d:0:1:2:3 Ok
     * [a::b] Ok
     * [a:b:c:d:e:f:0:1]:8080 Ok
     * 0:1:2::4 Ok
     * 0::2:3:4 Ok
     * ::3:4 Ok
     * ::::::: Fail
     * A:B:C:D:E:F:0:1 Fail
     */
    struct IPV6Address ipv6;
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(0, IPV6_parser("0000:0000:0000:0000:0000:0000:0000:0000", &ipv6));
    assert_int_equal(ipv6.sixteen[0], 0);
    assert_int_equal(ipv6.sixteen[1], 0);
    assert_int_equal(ipv6.sixteen[2], 0);
    assert_int_equal(ipv6.sixteen[3], 0);
    assert_int_equal(ipv6.sixteen[4], 0);
    assert_int_equal(ipv6.sixteen[5], 0);
    assert_int_equal(ipv6.sixteen[6], 0);
    assert_int_equal(ipv6.sixteen[7], 0);
    assert_int_equal(ipv6.port, 0);
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(0, IPV6_parser("0:0:0:0:0:0:0:0", &ipv6));
    assert_int_equal(ipv6.sixteen[0], 0);
    assert_int_equal(ipv6.sixteen[1], 0);
    assert_int_equal(ipv6.sixteen[2], 0);
    assert_int_equal(ipv6.sixteen[3], 0);
    assert_int_equal(ipv6.sixteen[4], 0);
    assert_int_equal(ipv6.sixteen[5], 0);
    assert_int_equal(ipv6.sixteen[6], 0);
    assert_int_equal(ipv6.sixteen[7], 0);
    assert_int_equal(ipv6.port, 0);
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(0, IPV6_parser("a:b:c:d::1", &ipv6));
    assert_int_equal(ipv6.sixteen[0], 0x0a);
    assert_int_equal(ipv6.sixteen[1], 0x0b);
    assert_int_equal(ipv6.sixteen[2], 0x0c);
    assert_int_equal(ipv6.sixteen[3], 0x0d);
    assert_int_equal(ipv6.sixteen[4], 0);
    assert_int_equal(ipv6.sixteen[5], 0);
    assert_int_equal(ipv6.sixteen[6], 0);
    assert_int_equal(ipv6.sixteen[7], 1);
    assert_int_equal(ipv6.port, 0);
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(0, IPV6_parser("a:b:c:d:0:1:2:3", &ipv6));
    assert_int_equal(ipv6.sixteen[0], 0x0a);
    assert_int_equal(ipv6.sixteen[1], 0x0b);
    assert_int_equal(ipv6.sixteen[2], 0x0c);
    assert_int_equal(ipv6.sixteen[3], 0x0d);
    assert_int_equal(ipv6.sixteen[4], 0);
    assert_int_equal(ipv6.sixteen[5], 1);
    assert_int_equal(ipv6.sixteen[6], 2);
    assert_int_equal(ipv6.sixteen[7], 3);
    assert_int_equal(ipv6.port, 0);
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(0, IPV6_parser("[a::b]", &ipv6));
    assert_int_equal(ipv6.sixteen[0], 0x0a);
    assert_int_equal(ipv6.sixteen[1], 0);
    assert_int_equal(ipv6.sixteen[2], 0);
    assert_int_equal(ipv6.sixteen[3], 0);
    assert_int_equal(ipv6.sixteen[4], 0);
    assert_int_equal(ipv6.sixteen[5], 0);
    assert_int_equal(ipv6.sixteen[6], 0);
    assert_int_equal(ipv6.sixteen[7], 0x0b);
    assert_int_equal(ipv6.port, 0);
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(0, IPV6_parser("[a:b:c:d:e:f:0:1]:8080", &ipv6));
    assert_int_equal(ipv6.sixteen[0], 0x0a);
    assert_int_equal(ipv6.sixteen[1], 0x0b);
    assert_int_equal(ipv6.sixteen[2], 0x0c);
    assert_int_equal(ipv6.sixteen[3], 0x0d);
    assert_int_equal(ipv6.sixteen[4], 0x0e);
    assert_int_equal(ipv6.sixteen[5], 0x0f);
    assert_int_equal(ipv6.sixteen[6], 0);
    assert_int_equal(ipv6.sixteen[7], 1);
    assert_int_equal(ipv6.port, 8080);
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(0, IPV6_parser("0:1:2::4", &ipv6));
    assert_int_equal(ipv6.sixteen[0], 0);
    assert_int_equal(ipv6.sixteen[1], 1);
    assert_int_equal(ipv6.sixteen[2], 2);
    assert_int_equal(ipv6.sixteen[3], 0);
    assert_int_equal(ipv6.sixteen[4], 0);
    assert_int_equal(ipv6.sixteen[5], 0);
    assert_int_equal(ipv6.sixteen[6], 0);
    assert_int_equal(ipv6.sixteen[7], 4);
    assert_int_equal(ipv6.port, 0);
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(0, IPV6_parser("0::2:3:4", &ipv6));
    assert_int_equal(ipv6.sixteen[0], 0);
    assert_int_equal(ipv6.sixteen[1], 0);
    assert_int_equal(ipv6.sixteen[2], 0);
    assert_int_equal(ipv6.sixteen[3], 0);
    assert_int_equal(ipv6.sixteen[4], 0);
    assert_int_equal(ipv6.sixteen[5], 2);
    assert_int_equal(ipv6.sixteen[6], 3);
    assert_int_equal(ipv6.sixteen[7], 4);
    assert_int_equal(ipv6.port, 0);
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(0, IPV6_parser("::3:4", &ipv6));
    assert_int_equal(ipv6.sixteen[0], 0);
    assert_int_equal(ipv6.sixteen[1], 0);
    assert_int_equal(ipv6.sixteen[2], 0);
    assert_int_equal(ipv6.sixteen[3], 0);
    assert_int_equal(ipv6.sixteen[4], 0);
    assert_int_equal(ipv6.sixteen[5], 0);
    assert_int_equal(ipv6.sixteen[6], 3);
    assert_int_equal(ipv6.sixteen[7], 4);
    assert_int_equal(ipv6.port, 0);
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(-1, IPV6_parser(":::::::", &ipv6));
    memset(&ipv6, 0, sizeof(struct IPV6Address));
    assert_int_equal(-1, IPV6_parser("A:B:C:D:E:F:0:1", &ipv6));
}

static void test_generic_interface(void **state)
{
    /*
     * This test might seem short, but it is intentional.
     * All the parsing tests should be implemented directly
     * on the corresponding parser test. Keep this test as
     * lean as possible.
     */
    IPAddress *address = NULL;
    Buffer *buffer = NULL;

    buffer = BufferNew();
    assert_true(buffer != NULL);
    assert_true(BufferSet(buffer, "127.0.0.1", strlen("127.0.0.1")) > 0);
    address = IPAddressNew(buffer);
    assert_true(address != NULL);
    assert_int_equal(IP_ADDRESS_TYPE_IPV4, IPAddressType(address));
    assert_string_equal("127.0.0.1", BufferData(IPAddressGetAddress(address)));
    assert_int_equal(0, IPAddressGetPort(address));
    assert_int_equal(0, IPAddressDestroy(&address));

    assert_true(BufferSet(buffer, "127.0.0.1:8080", strlen("127.0.0.1:8080")) > 0);
    address = IPAddressNew(buffer);
    assert_true(address != NULL);
    assert_int_equal(IP_ADDRESS_TYPE_IPV4, IPAddressType(address));
    assert_string_equal("127.0.0.1", BufferData(IPAddressGetAddress(address)));
    assert_int_equal(8080, IPAddressGetPort(address));
    assert_int_equal(0, IPAddressDestroy(&address));

    assert_true(BufferSet(buffer, "0:1:2:3:4:5:6:7", strlen("0:1:2:3:4:5:6:7")) > 0);
    address = IPAddressNew(buffer);
    assert_true(address != NULL);
    assert_int_equal(IP_ADDRESS_TYPE_IPV6, IPAddressType(address));
    assert_string_equal("0:1:2:3:4:5:6:7", BufferData(IPAddressGetAddress(address)));
    assert_int_equal(0, IPAddressGetPort(address));
    assert_int_equal(0, IPAddressDestroy(&address));

    assert_true(BufferSet(buffer, "[0:1:2:3:4:5:6:7]:9090", strlen("[0:1:2:3:4:5:6:7]:9090")) > 0);
    address = IPAddressNew(buffer);
    assert_true(address != NULL);
    assert_int_equal(IP_ADDRESS_TYPE_IPV6, IPAddressType(address));
    assert_string_equal("0:1:2:3:4:5:6:7", BufferData(IPAddressGetAddress(address)));
    assert_int_equal(9090, IPAddressGetPort(address));
    assert_int_equal(0, IPAddressDestroy(&address));

    assert_int_equal(0, BufferDestroy(&buffer));
}

int main()
{
    const UnitTest tests[] = {
        unit_test(test_ipv4)
        , unit_test(test_char2hex)
        , unit_test(test_ipv6)
        , unit_test(test_generic_interface)
    };

    return run_tests(tests);
}
