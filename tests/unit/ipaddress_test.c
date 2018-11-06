#include <test.h>

#include <string.h>
#include <platform.h>
#include <buffer.h>
#include <ip_address.c>
#include <ip_address.h>

static void test_ipv4(void)
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

static void test_char2hex(void)
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

static void test_ipv6(void)
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

static void test_generic_interface(void)
{
    /*
     * This test might seem short, but it is intentional.
     * All the parsing tests should be implemented directly
     * on the corresponding parser test. Keep this test as
     * lean as possible.
     */
    IPAddress *address = NULL;
    Buffer *buffer = NULL;
    Buffer *tmp_buffer = NULL;

    buffer = BufferNew();
    assert_true(buffer != NULL);
    BufferSet(buffer, "127.0.0.1", strlen("127.0.0.1"));
    address = IPAddressNew(buffer);
    assert_true(address != NULL);
    assert_int_equal(IP_ADDRESS_TYPE_IPV4, IPAddressType(address));
    tmp_buffer = IPAddressGetAddress(address);
    assert_string_equal("127.0.0.1", BufferData(tmp_buffer));
    DESTROY_AND_NULL(BufferDestroy, tmp_buffer);
    assert_int_equal(0, IPAddressGetPort(address));
    assert_int_equal(0, IPAddressDestroy(&address));

    BufferSet(buffer, "127.0.0.1:8080", strlen("127.0.0.1:8080"));
    address = IPAddressNew(buffer);
    assert_true(address != NULL);
    assert_int_equal(IP_ADDRESS_TYPE_IPV4, IPAddressType(address));
    tmp_buffer = IPAddressGetAddress(address);
    assert_string_equal("127.0.0.1", BufferData(tmp_buffer));
    DESTROY_AND_NULL(BufferDestroy, tmp_buffer);
    assert_int_equal(8080, IPAddressGetPort(address));
    assert_int_equal(0, IPAddressDestroy(&address));

    BufferSet(buffer, "0:1:2:3:4:5:6:7", strlen("0:1:2:3:4:5:6:7"));
    address = IPAddressNew(buffer);
    assert_true(address != NULL);
    assert_int_equal(IP_ADDRESS_TYPE_IPV6, IPAddressType(address));
    tmp_buffer = IPAddressGetAddress(address);
    assert_string_equal("0:1:2:3:4:5:6:7", BufferData(tmp_buffer));
    DESTROY_AND_NULL(BufferDestroy, tmp_buffer);
    assert_int_equal(0, IPAddressGetPort(address));
    assert_int_equal(0, IPAddressDestroy(&address));

    BufferSet(buffer, "[0:1:2:3:4:5:6:7]:9090", strlen("[0:1:2:3:4:5:6:7]:9090"));
    address = IPAddressNew(buffer);
    assert_true(address != NULL);
    assert_int_equal(IP_ADDRESS_TYPE_IPV6, IPAddressType(address));
    tmp_buffer = IPAddressGetAddress(address);
    assert_string_equal("0:1:2:3:4:5:6:7", BufferData(tmp_buffer));
    DESTROY_AND_NULL(BufferDestroy, tmp_buffer);
    assert_int_equal(9090, IPAddressGetPort(address));
    assert_int_equal(0, IPAddressDestroy(&address));

    BufferDestroy(buffer);
}

static void test_ipv4_address_comparison(void)
{
    /*
     * We test different IPV4 combinations:
     * 1.1.1.1 vs 1.1.1.1         -> equal
     * 1.2.3.4 vs 1.1.1.1         -> not equal
     * 1.2.3.4 vs 1.2.1.1         -> not equal
     * 1.2.3.4 vs 1.2.3.1         -> not equal
     * 2.2.3.4 vs 1.2.3.4         -> not equal
     * 1.2.3.4 vs 1.2.3.4         -> equal
     * 1.2.3.4 vs NULL            -> error
     * 1.2.3.4 vs 1:2:3:4:5:6:7:8 -> error
     */
    IPAddress *a = NULL;
    IPAddress *b = NULL;
    Buffer *bufferA = NULL;
    Buffer *bufferB = NULL;

    bufferA = BufferNew();
    assert_true(bufferA != NULL);
    BufferSet(bufferA, "1.1.1.1", strlen("1.1.1.1"));
    a = IPAddressNew(bufferA);
    assert_true(a != NULL);

    bufferB = BufferNew();
    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1.1.1.1", strlen("1.1.1.1"));
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_true(IPAddressIsEqual(a, b));

    BufferSet(bufferA, "1.2.3.4", strlen("1.2.3.4"));
    assert_int_equal(IPAddressDestroy(&a), 0);
    a = IPAddressNew(bufferA);
    assert_true(a != NULL);

    assert_false(IPAddressIsEqual(a, b));

    BufferSet(bufferB, "1.2.1.1", strlen("1.2.1.1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_false(IPAddressIsEqual(a, b));

    BufferSet(bufferB, "1.2.3.1", strlen("1.2.3.1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_false(IPAddressIsEqual(a, b));

    BufferSet(bufferA, "2.2.3.4", strlen("2.2.3.4"));
    assert_int_equal(IPAddressDestroy(&a), 0);
    a = IPAddressNew(bufferA);
    assert_true(a != NULL);

    BufferSet(bufferB, "1.2.3.4", strlen("1.2.3.4"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_false(IPAddressIsEqual(a, b));

    BufferSet(bufferA, "1.2.3.4", strlen("1.2.3.4"));
    assert_int_equal(IPAddressDestroy(&a), 0);
    a = IPAddressNew(bufferA);
    assert_true(a != NULL);

    assert_true(IPAddressIsEqual(a, b));

    assert_int_equal(IPAddressIsEqual(a, NULL), -1);
    assert_int_equal(IPAddressIsEqual(NULL, a), -1);

    BufferSet(bufferA, "1:2:3:4:5:6:7:8", strlen("1:2:3:4:5:6:7:8"));
    assert_int_equal(IPAddressDestroy(&a), 0);
    a = IPAddressNew(bufferA);
    assert_true(a != NULL);

    assert_int_equal(IPAddressIsEqual(a, b), -1);
    assert_int_equal(IPAddressIsEqual(b, a), -1);

    assert_int_equal(IPAddressDestroy(&a), 0);
    assert_int_equal(IPAddressDestroy(&b), 0);

    BufferDestroy(bufferA);
    BufferDestroy(bufferB);
}

static void test_ipv6_address_comparison(void)
{
    /*
     * We test different IPV6 combinations:
     * 1:1:1:1:1:1:1:1 vs 1:1:1:1:1:1:1:1 -> equal
     * 1:2:3:4:5:6:7:8 vs 1:1:1:1:1:1:1:1 -> not equal
     * 1:2:3:4:5:6:7:8 vs 1:2:1:1:1:1:1:1 -> not equal
     * 1:2:3:4:5:6:7:8 vs 1:2:3:1:1:1:1:1 -> not equal
     * 1:2:3:4:5:6:7:8 vs 1:2:3:4:1:1:1:1 -> not equal
     * 1:2:3:4:5:6:7:8 vs 1:2:3:4:5:1:1:1 -> not equal
     * 1:2:3:4:5:6:7:8 vs 1:2:3:4:5:6:1:1 -> not equal
     * 1:2:3:4:5:6:7:8 vs 1:2:3:4:5:6:7:1 -> not equal
     * 2:2:3:4:5:6:7:8 vs 1:2:3:4:5:6:7:8 -> not equal
     * 1:2:3:4:5:6:7:8 vs 1:2:3:4:5:6:7:8 -> equal
     * Exotic variants
     * 1:0:0:0:0:0:0:1 vs 1::1            -> equal
     * 1:1:0:0:0:0:0:1 vs 1::1            -> not equal
     * 1:1:0:0:0:0:0:1 vs 1:1::1          -> equal
     * 1:0:0:0:0:0:1:1 vs 1::1:1          -> equal
     * Error conditions
     * 1::1:1 vs NULL                     -> error
     * 1::1:1 vs 1.2.3.4                  -> error
     */

    IPAddress *a = NULL;
    IPAddress *b = NULL;
    Buffer *bufferA = NULL;
    Buffer *bufferB = NULL;

    bufferA = BufferNew();
    assert_true(bufferA != NULL);
    BufferSet(bufferA, "1:1:1:1:1:1:1:1", strlen("1:1:1:1:1:1:1:1"));
    a = IPAddressNew(bufferA);
    assert_true(a != NULL);

    bufferB = BufferNew();
    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1:1:1:1:1:1:1:1", strlen("1:1:1:1:1:1:1:1"));
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_true(IPAddressIsEqual(a, b));

    assert_true(bufferA != NULL);
    BufferSet(bufferA, "1:2:3:4:5:6:7:8", strlen("1:1:1:1:1:1:1:1"));
    assert_int_equal(IPAddressDestroy(&a), 0);
    a = IPAddressNew(bufferA);
    assert_true(a != NULL);

    assert_false(IPAddressIsEqual(a, b));

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1:2:1:1:1:1:1:1", strlen("1:2:1:1:1:1:1:1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_false(IPAddressIsEqual(a, b));

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1:2:3:1:1:1:1:1", strlen("1:2:3:1:1:1:1:1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_false(IPAddressIsEqual(a, b));

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1:2:3:4:1:1:1:1", strlen("1:2:3:4:1:1:1:1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_false(IPAddressIsEqual(a, b));

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1:2:3:4:5:1:1:1", strlen("1:2:3:4:5:1:1:1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_false(IPAddressIsEqual(a, b));

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1:2:3:4:5:6:1:1", strlen("1:2:3:4:5:6:1:1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_false(IPAddressIsEqual(a, b));

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1:2:3:4:5:6:7:1", strlen("1:2:3:4:5:6:7:1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_false(IPAddressIsEqual(a, b));

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "2:2:3:4:5:6:7:8", strlen("2:2:3:4:5:6:7:8"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_false(IPAddressIsEqual(a, b));

    assert_true(bufferA != NULL);
    BufferSet(bufferA, "1:0:0:0:0:0:0:1", strlen("1:0:0:0:0:0:0:1"));
    assert_int_equal(IPAddressDestroy(&a), 0);
    a = IPAddressNew(bufferA);
    assert_true(a != NULL);

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1::1", strlen("1::1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_true(IPAddressIsEqual(a, b));

    assert_true(bufferA != NULL);
    BufferSet(bufferA, "1:1:0:0:0:0:0:1", strlen("1:1:0:0:0:0:0:1"));
    assert_int_equal(IPAddressDestroy(&a), 0);
    a = IPAddressNew(bufferA);
    assert_true(a != NULL);

    assert_false(IPAddressIsEqual(a, b));

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1:1::1", strlen("1:1::1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_true(IPAddressIsEqual(a, b));

    assert_true(bufferA != NULL);
    BufferSet(bufferA, "1::1:1", strlen("1::1:1"));
    assert_int_equal(IPAddressDestroy(&a), 0);
    a = IPAddressNew(bufferA);
    assert_true(a != NULL);

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1:0:0:0:0:0:1:1", strlen("1:0:0:0:0:0:1:1"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_true(IPAddressIsEqual(a, b));

    assert_int_equal(IPAddressIsEqual(a, NULL), -1);

    assert_true(bufferB != NULL);
    BufferSet(bufferB, "1.2.3.4", strlen("1.2.3.4"));
    assert_int_equal(IPAddressDestroy(&b), 0);
    b = IPAddressNew(bufferB);
    assert_true(b != NULL);

    assert_int_equal(IPAddressIsEqual(a, b), -1);

    BufferDestroy(bufferA);
    BufferDestroy(bufferB);
    assert_int_equal(IPAddressDestroy(&a), 0);
    assert_int_equal(IPAddressDestroy(&b), 0);
}

static void test_isipaddress(void)
{
    /*
     * This test is just a summary of the other tests.
     * We just check that this interface works accordingly, most of the
     * functionality has already been tested.
     * 1.2.3.4         -> ok
     * 1.2..3          -> not
     * 1.a.2.3         -> not
     * 256.255.255.255 -> not
     * 255.255.255.255 -> ok
     * 1:0:0:0:0:0:0:1 -> ok
     * 1:1:1:1:0:1:1:1 -> ok
     * a:b:c:d:e:f:0:1 -> ok
     * a:b:c:d:e:f:g:h -> not
     * ffff:ffff:fffff:0:0:0:0:1 -> not
     */
    IPAddress *address = NULL;
    Buffer *bufferAddress = NULL;

    bufferAddress = BufferNew();
    assert_true (bufferAddress != NULL);

    BufferSet(bufferAddress, "1.2.3.4", strlen("1.2.3.4"));
    assert_true(IPAddressIsIPAddress(bufferAddress, NULL));
    assert_true(IPAddressIsIPAddress(bufferAddress, &address));
    assert_true(address != NULL);
    assert_int_equal(IPAddressType(address), IP_ADDRESS_TYPE_IPV4);
    BufferClear(bufferAddress);
    assert_int_equal(IPAddressDestroy(&address), 0);

    BufferSet(bufferAddress, "1.2..3", strlen("1.2..3"));
    assert_false(IPAddressIsIPAddress(bufferAddress, NULL));
    assert_false(IPAddressIsIPAddress(bufferAddress, &address));
    assert_true(address == NULL);
    BufferClear(bufferAddress);

    BufferSet(bufferAddress, "1.a.2.3", strlen("1.a.2.3"));
    assert_false(IPAddressIsIPAddress(bufferAddress, NULL));
    assert_false(IPAddressIsIPAddress(bufferAddress, &address));
    assert_true(address == NULL);
    BufferClear(bufferAddress);

    BufferSet(bufferAddress, "256.255.255.255", strlen("256.255.255.255"));
    assert_false(IPAddressIsIPAddress(bufferAddress, NULL));
    assert_false(IPAddressIsIPAddress(bufferAddress, &address));
    assert_true(address == NULL);
    BufferClear(bufferAddress);

    BufferSet(bufferAddress, "255.255.255.255", strlen("255.255.255.255"));
    assert_true(IPAddressIsIPAddress(bufferAddress, NULL));
    assert_true(IPAddressIsIPAddress(bufferAddress, &address));
    assert_true(address != NULL);
    assert_int_equal(IPAddressType(address), IP_ADDRESS_TYPE_IPV4);
    BufferClear(bufferAddress);
    assert_int_equal(IPAddressDestroy(&address), 0);

    BufferSet(bufferAddress, "1:0:0:0:0:0:0:1", strlen("1:0:0:0:0:0:0:1"));
    assert_true(IPAddressIsIPAddress(bufferAddress, NULL));
    assert_true(IPAddressIsIPAddress(bufferAddress, &address));
    assert_true(address != NULL);
    assert_int_equal(IPAddressType(address), IP_ADDRESS_TYPE_IPV6);
    BufferClear(bufferAddress);
    assert_int_equal(IPAddressDestroy(&address), 0);

    BufferSet(bufferAddress, "1:1:1:1:0:1:1:1", strlen("1:1:1:1:0:1:1:1"));
    assert_true(IPAddressIsIPAddress(bufferAddress, NULL));
    assert_true(IPAddressIsIPAddress(bufferAddress, &address));
    assert_true(address != NULL);
    assert_int_equal(IPAddressType(address), IP_ADDRESS_TYPE_IPV6);
    BufferClear(bufferAddress);
    assert_int_equal(IPAddressDestroy(&address), 0);

    BufferSet(bufferAddress, "a:b:c:d:e:f:0:1", strlen("a:b:c:d:e:f:0:1"));
    assert_true(IPAddressIsIPAddress(bufferAddress, NULL));
    assert_true(IPAddressIsIPAddress(bufferAddress, &address));
    assert_true(address != NULL);
    assert_int_equal(IPAddressType(address), IP_ADDRESS_TYPE_IPV6);
    BufferClear(bufferAddress);
    assert_int_equal(IPAddressDestroy(&address), 0);

    BufferSet(bufferAddress, "a:b:c:d:e:f:g:h", strlen("a:b:c:d:e:f:g:h"));
    assert_false(IPAddressIsIPAddress(bufferAddress, NULL));
    assert_false(IPAddressIsIPAddress(bufferAddress, &address));
    BufferClear(bufferAddress);

    BufferSet(bufferAddress, "ffff:ffff:fffff:0:0:0:0:1", strlen("ffff:ffff:fffff:0:0:0:0:1"));
    assert_false(IPAddressIsIPAddress(bufferAddress, NULL));
    assert_false(IPAddressIsIPAddress(bufferAddress, &address));
    BufferClear(bufferAddress);

    BufferDestroy(bufferAddress);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_ipv4)
        , unit_test(test_char2hex)
        , unit_test(test_ipv6)
        , unit_test(test_generic_interface)
        , unit_test(test_ipv4_address_comparison)
        , unit_test(test_ipv6_address_comparison)
        , unit_test(test_isipaddress)
    };

    return run_tests(tests);
}
