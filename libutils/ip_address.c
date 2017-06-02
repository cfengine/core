/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#include <platform.h>

#include <stdint.h>
#include <ctype.h>
#include <ip_address.h>
#include <alloc.h>

struct IPV4Address {
    uint8_t octets[4];
    uint16_t port;
};
struct IPV6Address {
    uint16_t sixteen[8];
    uint16_t port;
};

struct IPAddress {
    void *address;
    int type;
};

#define Char2Dec(o, c) \
    (o * 10) + c - '0'

/*
 * Hexadecimal conversion is not as simple as decimal conversion,
 * so we implement it here.
 * We do not check for errors, we assume the caller has checked that
 * the characters are hexadecimal.
 */
static int Char2Hex(int beginning, char increment)
{
    int number = beginning;
    number *= 16;
    if (('a' <= increment) && (increment <= 'f'))
    {
        number += (increment - 'a' + 0x0A);
    }
    else if (('A' <= increment) && (increment <= 'F'))
    {
        number += (increment - 'A' + 0x0A);
    }
    else
    {
        number += (increment - '0');
    }
    return number;
}

/*
 * This function parses the source pointer and checks if it conforms to the
 * 0a0b0c0d or 0a0b0c0d:0e0f formats (commonly used in procfs)
 *
 * If address is not NULL and the address is IPV4, then the result is copied there.
 *
 * Returns 0 on success.
 */
static int IPV4_hex_parser(const char *source, struct IPV4Address *address)
{
    {
        // shortcut for the 0a0b0c0d format
        unsigned int a, b, c, d, pport = 0;
        if (strlen(source) == 8 &&
            sscanf(source, "%2x%2x%2x%2x", &a, &b, &c, &d) == 4)
        {
            address->octets[3] = a;
            address->octets[2] = b;
            address->octets[1] = c;
            address->octets[0] = d;
            address->port = pport;
            return 0;
        }

        // shortcut for the 0a0b0c0d:0e0f format
        if (strlen(source) == 8+1+4 &&
            sscanf(source, "%2x%2x%2x%2x:%4x", &a, &b, &c, &d, &pport) == 5)
        {
            address->octets[3] = a;
            address->octets[2] = b;
            address->octets[1] = c;
            address->octets[0] = d;
            address->port = pport;
            return 0;
        }
    }

    return -1;
}

/*
 * This function parses the source pointer and checks if it conforms to the
 * RFC 791.
 *
 * xxx.xxx.xxx.xxx[:ppppp]
 *
 * If address is not NULL and the address is IPV4, then the result is copied there.
 *
 * Returns 0 on success.
 */
static int IPV4_parser(const char *source, struct IPV4Address *address)
{
    char *p = NULL;
    int octet = 0;
    int port = 0;
    int period_counter = 0;
    int port_counter = 0;
    int char_counter = 0;
    bool is_period = false;
    bool is_digit = false;
    bool is_port = false;
    bool has_digit = false;

    /*
     * For simplicity sake we initialize address (if not NULL).
     */
    if (address)
    {
        int i = 0;
        for (i = 0; i < 4; ++i)
        {
            address->octets[i] = 0;
        }
        address->port = 0;
    }

    /*
     * IPV4 parsing has 6 states, of which:
     * 2 are end states
     * 4 are parsing states
     *
     * States 0 to 3 are purely address parsing. State 5
     * might never be reached if there is no port.
     * State 4 is the final state if everything went ok.
     * State 6 is reached in case of error.
     *
     * 0    1    2    3
     * |d   |d   |d   |d
     * | p  | p  | p  | done
     * 0 -> 1 -> 2 -> 3 ->  4
     * |    |    |    |     |
     * 7 <--+----+----+     |
     *     error      | ':' |
     *               _5-----+
     *             d \| done
     */
    int state = 0;
    bool state_change = false;
    for (p = (char *)source; *p != '\0'; ++p)
    {
        /*
         * Do some character recognition
         */
        is_digit = isdigit(*p);
        is_period = (*p == '.') ? 1 : 0;
        is_port = (*p == ':') ? 1 : 0;
        /*
         * Update the corresponding flags.
         */
        if (is_period)
        {
            period_counter++;
        }
        if (is_port)
        {
            port_counter++;
        }
        /*
         * Do the right operation depending on the state
         */
        switch (state)
        {
        case 0:
        case 1:
        case 2:
            /*
             * The three first states are the same.
             * XXX.XXX.XXX.xxx[:nnnnn]
             */
            if (is_digit)
            {
                octet = Char2Dec(octet, *p);
                has_digit = true;
            }
            else if (is_period)
            {
                if (address)
                {
                    address->octets[state] = octet;
                }
                state++;
                state_change = true;
            }
            else
            {
                state = 7;
                state_change = true;
            }
            break;
        case 3:
            /*
             * This case is different from the previous ones. A period here means error.
             * xxx.xxx.xxx.XXX[:nnnnn]
             */
            if (is_digit)
            {
                octet = Char2Dec(octet, *p);
                has_digit = true;
            }
            else if (is_port)
            {
                if (address)
                {
                    address->octets[state] = octet;
                }
                state = 5;
                state_change = true;
            }
            else
            {
                state = 7;
                state_change = true;
            }
            break;
        case 4:
            break;
        case 5:
            if (is_digit)
            {
                port = Char2Dec(port, *p);
            }
            else
            {
                state = 7;
                state_change = true;
            }
            break;
        case 6:
        default:
            return -1;
            break;
        }
        /*
         * It is important to the filtering before counting the characters.
         * Otherwise the counter will need to start from -1.
         */
        char_counter++;
        /*
         * Do some sanity checks, this should hold no matter
         * in which state of the state machine we are.
         */
        if (octet > 255)
        {
            return -1;
        }
        if (port > 65535)
        {
            return -1;
        }
        if (period_counter > 1)
        {
            return -1;
        }
        if (port_counter > 1)
        {
            return -1;
        }
        if (state_change)
        {
            /*
             * Check that we have digits, otherwise the transition is wrong.
             */
            if (!has_digit)
            {
                return -1;
            }
            /*
             * Reset all the variables.
             */
            char_counter = 0;
            octet = 0;
            port = 0;
            period_counter = 0;
            port_counter = 0;
            is_period = false;
            is_digit = false;
            is_port = false;
            has_digit = false;
            state_change = false;
        }
    }
    /*
     * These states are not end state, which mean we exited the loop because of an error
     */
    if ((state == 0) || (state == 1) || (state == 2))
    {
        return -1;
    }
    /*
     * If state is 3 then we exited the loop without copying the last octet.
     * This is because we didn't get to state 4.
     * Notice that we need to check if we had characters, it might be possible that we
     * have the following situation 'xxx.xxx.xxx.' which will fit the state change but not
     * produce a valid IP.
     */
    if (state == 3)
    {
        if (char_counter == 0)
        {
            return -1;
        }
        if (address)
        {
            address->octets[3] = octet;
        }
    }
    /*
     * If state is 5 then we exited the loop without copying the port.
     * This is because we hit a '\0'.
     * Notice that we need to check if we had characters, it might be possible that we
     * have the following situation 'xxx.xxx.xxx.xxx:' which will fit the state change but not
     * produce a valid IP.
     */
    if (state == 5)
    {
        if (char_counter == 0)
        {
            return -1;
        }
        if (address)
        {
            address->port = port;
        }
    }
    /*
     * If state is 6 then there was an error.
     */
    if (state == 6)
    {
        return -1;
    }
    return 0;
}

/*
 * This function parses the address and checks if it conforms to the
 * 0a0b0c0d0e0f0g0h or 0a0b0c0d0e0f0g0h:0i0j format (commonly used in procfs)
 *
 * Returns 0 on success.
 */
static int IPV6_hex_parser(const char *source, struct IPV6Address *address)
{
    {
        // shortcut for the 0a0b0c0d0e0f0g0h format
        unsigned int a, b, c, d, e, f, g, h, pport = 0;

        if (strlen(source) == 32 &&
            sscanf(source, "%4x%4x%4x%4x%4x%4x%4x%4x", &a, &b, &c, &d, &e, &f, &g, &h) == 8)
        {
            address->sixteen[0] = a;
            address->sixteen[1] = b;
            address->sixteen[2] = c;
            address->sixteen[3] = d;
            address->sixteen[4] = e;
            address->sixteen[5] = f;
            address->sixteen[6] = g;
            address->sixteen[7] = h;
            return 0;
        }

        if (strlen(source) == 32+1+4 &&
            sscanf(source, "%4x%4x%4x%4x%4x%4x%4x%4x:%4x", &a, &b, &c, &d, &e, &f, &g, &h, &pport) == 9)
        {
            address->sixteen[0] = a;
            address->sixteen[1] = b;
            address->sixteen[2] = c;
            address->sixteen[3] = d;
            address->sixteen[4] = e;
            address->sixteen[5] = f;
            address->sixteen[6] = g;
            address->sixteen[7] = h;
            address->port = pport;
            return 0;
        }
    }

    return -1;
}

/*
 * This function parses the address and checks if it conforms to the
 * RFCs 2373, 2460 and 5952.
 * We do not support Microsoft UNC encoding, i.e.
 * hhhh-hhhh-hhhh-hhhh-hhhh-hhhh-hhhh-hhhh.ipv6-literal.net
 * Despite following RFC 5292 we do not signal errors derived from bad
 * zero compression although this might change on time, so please do not
 * trust that we will honor address with wrong zero compression.
 *
 * Returns 0 on success.
 */
static int IPV6_parser(const char *source, struct IPV6Address *address)
{
    /*
     * IPV6 parsing is more complex than IPV4 parsing. There are a few ground rules:
     * - Leading zeros can be omitted.
     * - Fields that are just zeros can be abbreviated to one zero or completely omitted.
     * In the later case the following notation is used: '::'.
     * - Port number is specified in a special way:
     * [hhhh:hhhh:hhhh:hhhh:hhhh:hhhh:hhhh:hhhh]:ppppp
     * Notice that it is possible to have the '[' and ']' without specifying a port.
     *
     * Simplified state machine:
     *
     * _h      _h      _h      _h      _h      _h      _h      _h              _d
     * |/ ':'  |/ ':'  |/ ':'  |/ ':'  |/ ':'  |/ ':'  |/ ':'  |/ ']'    ':'   |/ done
     * 0------>1------>2------>3------>4------>5------>6------>7------>8------>9------>11
     * |\                                                      | done  |
     * -'['                                                    9<------+
     *
     * This is a simplified state machine since I assume that we keep the square brackets inside
     * the same state as hexadecimal digits, which in practice is not true.
     */
    char *p = NULL;
    int sixteen = 0;
    int unsorted_sixteen[6];
    int unsorted_pointer = 0;
    int bracket_expected = 0;
    int port = 0;
    int char_counter = 0;
    bool is_start_bracket = 0;
    bool is_end_bracket = 0;
    bool is_colon = 0;
    bool is_hexdigit = 0;
    bool is_upper_hexdigit = 0;
    bool is_digit = 0;
    int zero_compression = 0;
    int already_compressed = 0;
    int state = 0;
    bool state_change = false;

    /*
     * Initialize our container for unknown numbers.
     */
    for (unsorted_pointer = 0; unsorted_pointer < 6; ++unsorted_pointer)
    {
        unsorted_sixteen[unsorted_pointer] = 0;
    }
    unsorted_pointer = 0;
    /*
     * For simplicity sake we initialize address (if not NULL).
     */
    if (address)
    {
        int i = 0;
        for (i = 0; i < 8; ++i)
        {
            address->sixteen[i] = 0;
        }
        address->port = 0;
    }

    for (p = (char *)source; *p != '\0'; ++p)
    {
        /*
         * Take a closer look at the character
         */
        is_start_bracket = (*p == '[') ? 1 : 0;
        is_end_bracket = (*p == ']') ? 1 : 0;
        is_hexdigit = isxdigit(*p);
        is_digit = isdigit(*p);
        is_colon = (*p == ':') ? 1 : 0;
        if (is_hexdigit)
        {
            if (isalpha(*p))
            {
                is_upper_hexdigit = isupper(*p);
            }
        }

        switch (state)
        {
        case 0:
            /*
             * This case is slightly different because of the possible presence of '['.
             * Notice that '[' is only valid as the first character, anything else is
             * an error!
             */
            if (is_start_bracket)
            {
                if (char_counter == 0)
                {
                    bracket_expected = 1;
                }
                else
                {
                    state = 11;
                    state_change = true;
                }
            }
            else if (is_hexdigit)
            {
                /*
                 * RFC 5952 forbids upper case hex digits
                 */
                if (is_upper_hexdigit)
                {
                    state = 11;
                    state_change = true;
                }
                else
                {
                    sixteen = Char2Hex(sixteen, *p);
                }
            }
            else if (is_colon)
            {
                if (address)
                {
                    address->sixteen[0] = sixteen;
                }
                state = 1;
                state_change = true;
            }
            else
            {
                state = 11;
                state_change = true;
            }
            break;
        case 1:
            /*
             * This state is special since it cannot have a ']' as in the next
             * states.
             */
            if (is_hexdigit)
            {
                /*
                 * RFC 5952 forbids upper case hex digits
                 */
                if (is_upper_hexdigit)
                {
                    state = 11;
                    state_change = true;
                }
                else
                {
                    sixteen = Char2Hex(sixteen, *p);
                }
            }
            else if (is_colon)
            {
                if (char_counter == 0)
                {
                    /*
                     * This means 'X::Y...' which means zero compression.
                     * Flag it!
                     */
                    zero_compression = 1;
                    already_compressed = 1;
                }
                else
                {
                    if (address)
                    {
                        address->sixteen[state] = sixteen;
                    }
                }
                ++state;
                state_change = true;
            }
            else
            {
                state = 11;
                state_change = true;
            }
            break;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            if (is_hexdigit)
            {
                /*
                 * RFC 5952 forbids upper case hex digits
                 */
                if (is_upper_hexdigit)
                {
                    state = 11;
                    state_change = true;
                }
                else
                {
                    sixteen = Char2Hex(sixteen, *p);
                }
            }
            else if (is_colon)
            {
                if (char_counter == 0)
                {
                    if (already_compressed)
                    {
                        /*
                         * The '::' symbol can only occur once in a given address.
                         */
                        state = 11;
                        state_change = true;
                    }
                    else
                    {
                        /*
                         * This means '...:X::Y...' which means zero compression.
                         * Flag it!
                         */
                        zero_compression = 1;
                        already_compressed = 1;
                    }
                }
                else
                {
                    if (zero_compression)
                    {
                        /*
                         * If zero compression is enabled, then we cannot trust the position
                         * since we might compressed several fields. We store the value and
                         * look at them afterwards.
                         */
                        unsorted_sixteen[unsorted_pointer] = sixteen;
                        ++unsorted_pointer;
                    }
                    else
                    {
                        /*
                         * No zero compression, just assign the address and keep moving.
                         */
                        if (address)
                        {
                            address->sixteen[state] = sixteen;
                        }
                    }
                    ++state;
                    state_change = true;
                }
            }
            else if (is_end_bracket)
            {
                if (bracket_expected && zero_compression)
                {
                    bracket_expected = 0;
                    /*
                     * RFC 5952 says that we can end an address at any point after
                     * the second position (consequence of the zero compression).
                     * Therefore if we find a ']' we just jump to state 8.
                     */
                    unsorted_sixteen[unsorted_pointer] = sixteen;
                    ++unsorted_pointer;
                    state = 8;
                    state_change = true;
                }
                else
                {
                    /*
                     * Funny stuff, we got a ']' that we were not expecting.
                     * Politely walk back and signal the error.
                     */
                    state = 11;
                    state_change = true;
                }
            }
            else
            {
                state = 11;
                state_change = true;
            }
            break;
        case 7:
            /*
             * This case is special.
             */
            if (is_hexdigit)
            {
                /*
                 * RFC 5952 forbids uppercase hex digits
                 */
                if (is_upper_hexdigit)
                {
                    state = 11;
                    state_change = true;
                }
                else
                {
                    sixteen = Char2Hex(sixteen, *p);
                }
            }
            else if (is_end_bracket)
            {
                if (bracket_expected)
                {
                    bracket_expected = 0;
                    if (address)
                    {
                        address->sixteen[state] = sixteen;
                    }
                    /*
                     * The last possible position for a sixteen is number 8.
                     */
                    ++state;
                    state_change = true;
                }
                else
                {
                    /*
                     * Funny stuff, we got a ']' that we were not expecting.
                     * Politely walk back and signal the error.
                     */
                    state = 11;
                    state_change = true;
                }
            }
            else
            {
                state = 11;
                state_change = true;
            }
            break;
        case 8:
            if (is_colon)
            {
                ++state;
                state_change = true;
            }
            else
            {
                state = 11;
                state_change = true;
            }
            break;
        case 9:
            if (is_digit)
            {
                port = Char2Dec(port, *p);
            }
            else
            {
                state = 11;
                state_change = true;
            }
            break;
        case 10:
            break;
        case 11:
        default:
            return -1;
            break;
        }
        char_counter++;
        if (sixteen > 0xFFFF)
        {
            return -1;
        }
        if (port > 65535)
        {
            return -1;
        }
        if (state_change)
        {
            sixteen = 0;
            port = 0;
            char_counter = 0;
            is_start_bracket = false;
            is_end_bracket = false;
            is_colon = false;
            is_hexdigit = false;
            is_upper_hexdigit = false;
            is_digit = 0;
            state_change = false;
        }
    }
    /*
     * Look at the end state and return accordingly.
     */
    if ((state == 0) || (state == 1))
    {
        /*
         * These states are not final states, so if we exited is because something went wrong.
         */
        return -1;
    }
    /*
     * Thanks to RFC5952 the final states can be intermediate states. This because of zero compression,
     * which means that the following address 1:0:0:0:0:0:0:1 can be written as
     * 1::1. Not to mention more exotic varieties such as 1:0:0:0:0:0:0:0 1:: or
     * 0:0:0:0:0:0:0:0 ::
     * The first intermediate state that can exit is 2, since even the smallest of all address('::')
     * will have at least two ':'.
     * Another exotic case is 'X:0:0:Y:0:0:0:Z' which becomes 'X::Y:0:0:0:Z' or X:0:0:Y::Z because the symbol '::'
     * can appear only once in a given address.
     */
    if ((state == 2) || (state == 3) || (state == 4) || (state == 5) || (state == 6))
    {
        /*
         * We check first for non-closed brackets.
         * Then we check if there is a number that has not been added to our array.
         * Finally we move to zero compression.
         */
        if (bracket_expected)
        {
            return -1;
        }
        unsorted_sixteen[unsorted_pointer] = sixteen;
        ++unsorted_pointer;
        if (zero_compression)
        {
            /*
             * If there is no address, then we can just return :-)
             */
            if (address)
            {
                /*
                 * We need to find the rightful positions for those numbers.
                 * We use a simple trick:
                 * We know how many unsorted addresses we have from unsorted pointer,
                 * and we know that once zero_compression is activated we do not fill
                 * any more numbers to the address structure. Therefore the right way
                 * to do this is to take the array of unsorted_sixteen and start assigning
                 * numbers backwards.
                 */
                int i = 0;
                for (i = 0; i < unsorted_pointer; ++i)
                {
                    address->sixteen[7 - i] = unsorted_sixteen[unsorted_pointer - i - 1];
                }
            }
        }
        else
        {
            /*
             * We cannot end up here without zero compression, or an error.
             */
            return -1;
        }
    }
    if (state == 7)
    {
        /*
         * This state corresponds to the final state of a simple ipv6 address, i.e.
         * xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx
         * We exited because we ran out of characters. Let's check that we have something
         * before assigning things.
         */
        if (char_counter == 0)
        {
            /*
             * The last field was empty, signal the error.
             */
            return -1;
        }
        if (bracket_expected)
        {
            /*
             * We were expecting a bracket but it never came, so this is an error.
             */
            return -1;
        }
        if (address)
        {
            address->sixteen[7] = sixteen;
        }
    }
    if (state == 8)
    {
        /*
         * This state corresponds to the final state if brackets were present.
         * We still need to check if zero compression was activated and copy
         * the values if so.
         */
        if (zero_compression)
        {
            /*
             * If there is no address, then we can just return :-)
             */
            if (address)
            {
                /*
                 * We need to find the rightful positions for those numbers.
                 * We use a simple trick:
                 * We know how many unsorted addresses we have from unsorted pointer,
                 * and we know that once zero_compression is activated we do not fill
                 * any more numbers to the address structure. Therefore the right way
                 * to do this is to take the array of unsorted_sixteen and start assigning
                 * numbers backwards.
                 */
                int i = 0;
                for (i = 0; i < unsorted_pointer; ++i)
                {
                    address->sixteen[7 - i] = unsorted_sixteen[i];
                }
            }
        }
    }
    if (state == 9)
    {
        /*
         * This state corresponds to the final state if we had brackets around us.
         * This is usually used to append a port number to the address, so we check
         * if we have a port and then assign it.
         */
        if (char_counter == 0)
        {
            /*
             * The last field was empty, signal the error.
             */
            return -1;
        }
        if (address)
        {
            address->port = port;
        }
    }
    if (state == 11)
    {
        /*
         * Error state
         */
        return -1;
    }
    return 0;
}

IPAddress *IPAddressNew(Buffer *source)
{
    if (!source || !BufferData(source))
    {
        return NULL;
    }
    IPAddress *address = NULL;
    const char *pad = BufferData(source);
    struct IPV4Address *ipv4 = NULL;
    struct IPV6Address *ipv6 = NULL;
    ipv4 = (struct IPV4Address *)xmalloc(sizeof(struct IPV4Address));
    ipv6 = (struct IPV6Address *)xmalloc(sizeof(struct IPV6Address));

    if (IPV4_parser(pad, ipv4) == 0)
    {
        free (ipv6);
        address = (IPAddress *)xmalloc(sizeof(IPAddress));
        address->type = IP_ADDRESS_TYPE_IPV4;
        address->address = (void *)ipv4;
    }
    else if (IPV6_parser(pad, ipv6) == 0)
    {
        free (ipv4);
        address = (IPAddress *)xmalloc(sizeof(IPAddress));
        address->type = IP_ADDRESS_TYPE_IPV6;
        address->address = (void *)ipv6;
    }
    else
    {
        /*
         * It was not a valid IP address.
         */
        free (ipv4);
        free (ipv6);
        return NULL;
    }
    return address;
}

IPAddress *IPAddressNewHex(Buffer *source)
{
    if (!source || !BufferData(source))
    {
        return NULL;
    }
    IPAddress *address = NULL;
    const char *pad = BufferData(source);
    struct IPV4Address *ipv4 = NULL;
    struct IPV6Address *ipv6 = NULL;
    ipv4 = (struct IPV4Address *)xmalloc(sizeof(struct IPV4Address));
    ipv6 = (struct IPV6Address *)xmalloc(sizeof(struct IPV6Address));

    if (IPV4_hex_parser(pad, ipv4) == 0)
    {
        free (ipv6);
        address = (IPAddress *)xmalloc(sizeof(IPAddress));
        address->type = IP_ADDRESS_TYPE_IPV4;
        address->address = (void *)ipv4;
    }
    else if (IPV6_hex_parser(pad, ipv6) == 0)
    {
        free (ipv4);
        address = (IPAddress *)xmalloc(sizeof(IPAddress));
        address->type = IP_ADDRESS_TYPE_IPV6;
        address->address = (void *)ipv6;
    }
    else
    {
        /*
         * It was not a valid IP address.
         */
        free (ipv4);
        free (ipv6);
        return NULL;
    }
    return address;
}

int IPAddressDestroy(IPAddress **address)
{
    if (!address || !(*address))
    {
        return 0;
    }
    if ((*address)->address)
    {
        free ((*address)->address);
    }
    free (*address);
    *address = NULL;
    return 0;
}

int IPAddressType(IPAddress *address)
{
    if (!address)
    {
        return -1;
    }
    return address->type;
}

Buffer *IPAddressGetAddress(IPAddress *address)
{
    if (!address)
    {
        return NULL;
    }
    Buffer *buffer = NULL;
    int result = 0;

    if (address->type == IP_ADDRESS_TYPE_IPV4)
    {
        struct IPV4Address *ipv4 = (struct IPV4Address *)address->address;
        buffer = BufferNew();
#if BIG_ENDIAN
        result = BufferPrintf(buffer, "%u.%u.%u.%u", ipv4->octets[0], ipv4->octets[1], ipv4->octets[2], ipv4->octets[3]);
#elif LITTLE_ENDIAN
        result = BufferPrintf(buffer, "%u.%u.%u.%u", ipv4->octets[3], ipv4->octets[2], ipv4->octets[1], ipv4->octets[0]);
#else
#warning "Unrecognized endianness, assuming big endian"
        result = BufferPrintf(buffer, "%u.%u.%u.%u", ipv4->octets[0], ipv4->octets[1], ipv4->octets[2], ipv4->octets[3]);
#endif
    }
    else if (address->type == IP_ADDRESS_TYPE_IPV6)
    {
        struct IPV6Address *ipv6 = (struct IPV6Address *)address->address;
        buffer = BufferNew();
#if BIG_ENDIAN
        result = BufferPrintf(buffer, "%x:%x:%x:%x:%x:%x:%x:%x", ipv6->sixteen[0], ipv6->sixteen[1], ipv6->sixteen[2],
                              ipv6->sixteen[3], ipv6->sixteen[4], ipv6->sixteen[5], ipv6->sixteen[6], ipv6->sixteen[7]);
#elif LITTLE_ENDIAN
        result = BufferPrintf(buffer, "%x:%x:%x:%x:%x:%x:%x:%x", ipv6->sixteen[7], ipv6->sixteen[6], ipv6->sixteen[5],
                              ipv6->sixteen[4], ipv6->sixteen[3], ipv6->sixteen[2], ipv6->sixteen[1], ipv6->sixteen[0]);
#else
#warning "Unrecognized endianness, assuming big endian"
        result = BufferPrintf(buffer, "%x:%x:%x:%x:%x:%x:%x:%x", ipv6->sixteen[0], ipv6->sixteen[1], ipv6->sixteen[2],
                              ipv6->sixteen[3], ipv6->sixteen[4], ipv6->sixteen[5], ipv6->sixteen[6], ipv6->sixteen[7]);
#endif
    }
    else
    {
        buffer = NULL;
    }
    if (result < 0)
    {
        BufferDestroy(buffer);
        return NULL;
    }
    return buffer;
}

int IPAddressGetPort(IPAddress *address)
{
    if (!address)
    {
        return -1;
    }
    int port = -1;
    if (address->type == IP_ADDRESS_TYPE_IPV4)
    {
        struct IPV4Address *ipv4 = (struct IPV4Address *)address->address;
        port = ipv4->port;
    }
    else if (address->type == IP_ADDRESS_TYPE_IPV6)
    {
        struct IPV6Address *ipv6 = (struct IPV6Address *)address->address;
        port = ipv6->port;
    }
    else
    {
        return -1;
    }
    return port;
}

/*
 * Comparison for IPV4 addresses
 */
static int IPV4Compare(struct IPV4Address *a, struct IPV4Address *b)
{
    int i = 0;
    for (i = 0; i < 4; ++i)
    {
        if (a->octets[i] != b->octets[i])
        {
            return 0;
        }
    }
    return 1;
}

/*
 * Comparison for IPV6 addresses
 */
static int IPV6Compare(struct IPV6Address *a, struct IPV6Address *b)
{
    int i = 0;
    for (i = 0; i < 8; ++i)
    {
        if (a->sixteen[i] != b->sixteen[i])
        {
            return 0;
        }
    }
    return 1;
}

int IPAddressIsEqual(IPAddress *a, IPAddress *b)
{
    /*
     * We do not support IPV4 versus IPV6 comparisons.
     * This is trickier than what it seems, since even the IPV6 representation of an IPV6 address is not
     * clear yet.
     */
     if (!a || !b)
     {
         return -1;
     }
     if (a->type != b->type)
     {
         return -1;
     }
     if (a->type == IP_ADDRESS_TYPE_IPV4)
     {
         return IPV4Compare((struct IPV4Address *)a->address, (struct IPV4Address *)b->address);
     }
     else if (a->type == IP_ADDRESS_TYPE_IPV6)
     {
         return IPV6Compare((struct IPV6Address *)a->address, (struct IPV6Address *)b->address);
     }
     return -1;
}

/*
 * Sorting comparison for IPV4 addresses
 */
static int IPV4CompareLess(struct IPV4Address *a, struct IPV4Address *b)
{
    int i = 0;
    for (i = 0; i < 4; ++i)
    {
        if (a->octets[i] > b->octets[i])
        {
            return 0;
        }
        else if (a->octets[i] < b->octets[i])
        {
            return 1;
        }
    }

    return 0;
}

/*
 * Sorting comparison for IPV6 addresses
 */
static int IPV6CompareLess(struct IPV6Address *a, struct IPV6Address *b)
{
    int i = 0;
    for (i = 0; i < 8; ++i)
    {
        if (a->sixteen[i] > b->sixteen[i])
        {
            return 0;
        }
        else if (a->sixteen[i] < b->sixteen[i])
        {
            return 1;
        }
    }

    return 0;
}

int IPAddressCompareLess(IPAddress *a, IPAddress *b)
{
    /*
     * We do not support IPV4 versus IPV6 comparisons.
     * This is trickier than what it seems, since even the IPV6 representation of an IPV6 address is not
     * clear yet.
     */
     if (!a || !b)
     {
         return 1;
     }

     // Sort IPv4 BEFORE any other types
     if (a->type == IP_ADDRESS_TYPE_IPV4 && a->type != b->type)
     {
         return 1;
     }

     if (b->type == IP_ADDRESS_TYPE_IPV4 && a->type != b->type)
     {
         return 0;
     }

     if (a->type == IP_ADDRESS_TYPE_IPV4 && b->type == IP_ADDRESS_TYPE_IPV4)
     {
         return IPV4CompareLess((struct IPV4Address *)a->address, (struct IPV4Address *)b->address);
     }

     if (a->type == IP_ADDRESS_TYPE_IPV6 && b->type == IP_ADDRESS_TYPE_IPV6)
     {
         return IPV6CompareLess((struct IPV6Address *)a->address, (struct IPV6Address *)b->address);
     }

     return -1;
}

bool IPAddressIsIPAddress(Buffer *source, IPAddress **address)
{
    if (!source || !BufferData(source))
    {
        return false;
    }
    bool create_object = false;
    if (address)
    {
        create_object = true;
    }
    const char *pad = BufferData(source);
    struct IPV4Address *ipv4 = NULL;
    struct IPV6Address *ipv6 = NULL;
    ipv4 = (struct IPV4Address *)xmalloc(sizeof(struct IPV4Address));
    ipv6 = (struct IPV6Address *)xmalloc(sizeof(struct IPV6Address));
    if (IPV4_parser(pad, ipv4) == 0)
    {
        free (ipv6);
        if (create_object)
        {
            *address = (IPAddress *)xmalloc(sizeof(IPAddress));
            (*address)->type = IP_ADDRESS_TYPE_IPV4;
            (*address)->address = (void *)ipv4;
        }
        else
        {
            /*
             * We know it is a valid IPV4 address and we know we don't need the
             * IPV4 structure.
             */
            free (ipv4);
        }
    }
    else if (IPV6_parser(pad, ipv6) == 0)
    {
        free (ipv4);
        if (create_object)
        {
            *address = (IPAddress *)xmalloc(sizeof(IPAddress));
            (*address)->type = IP_ADDRESS_TYPE_IPV6;
            (*address)->address = (void *)ipv6;
        }
        else
        {
            /*
             * We know it is a valid IPV6 address and we know we don't need the
             * IPV6 structure.
             */
            free (ipv6);
        }
    }
    else
    {
        /*
         * It was not a valid IP address.
         */
        free (ipv4);
        free (ipv6);
        return false;
    }
    return true;
}

