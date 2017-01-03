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

#include <addr_lib.h>
#include <cfnet.h>

#include <logging.h>
#include <string_lib.h>

#define CF_ADDRSIZE 128
#define CF_MAXVARSIZE 1024

/* Match two IP strings - with : or . in hex or decimal
   s1 is the test string, and s2 is the reference e.g.
   FuzzySetMatch("128.39.74.10/23","128.39.75.56") == 0

   Returns 0 on match. */

/* TODO rename to AddrSubnetMatch() */
int FuzzySetMatch(const char *s1, const char *s2)
{
    short isCIDR = false, isrange = false, isv6 = false, isv4 = false;
    char address[CF_ADDRSIZE];

    if (strcmp(s1, s2) == 0)
    {
        return 0;
    }

    if (strstr(s1, "/") != 0)
    {
        isCIDR = true;
    }

    if (strstr(s1, "-") != 0)
    {
        isrange = true;
    }

    if (strstr(s1, ".") != 0)
    {
        isv4 = true;
    }

    if (strstr(s1, ":") != 0)
    {
        isv6 = true;
    }

    if (strstr(s2, ".") != 0)
    {
        isv4 = true;
    }

    if (strstr(s2, ":") != 0)
    {
        isv6 = true;
    }

    if (isv4 && isv6)
    {
        /* This is just wrong */
        return -1;
    }

    if (isCIDR && isrange)
    {
        Log(LOG_LEVEL_ERR, "Cannot mix CIDR notation with xxx-yyy range notation '%s'", s1);
        return -1;
    }

    if (!(isv6 || isv4))
    {
        Log(LOG_LEVEL_ERR, "Not a valid address range - or not a fully qualified name '%s'", s1);
        return -1;
    }

    if (!(isrange || isCIDR))
    {
        if (strlen(s2) > strlen(s1))
        {
            if (*(s2 + strlen(s1)) != '.')
            {
                return -1;      // Because xxx.1 should not match xxx.12 in the same octet
            }
        }

        return strncmp(s1, s2, strlen(s1));     /* do partial string match */
    }

    if (isv4)
    {
        if (isCIDR)
        {
            struct sockaddr_in addr1, addr2;
            unsigned long mask;

            address[0] = '\0';
            int ret = sscanf(s1, "%16[^/]/%lu", address, &mask);
            if (ret != 2 || mask > 32)
            {
                Log(LOG_LEVEL_ERR, "Invalid IPv4 CIDR: %s", s1);
                return -1;
            }
            else if (mask == 0)
            {
                return 0;                     /* /0 CIDR matches everything */
            }

            inet_pton(AF_INET, address, &addr1.sin_addr);
            inet_pton(AF_INET, s2, &addr2.sin_addr);

            unsigned long a1 = htonl(addr1.sin_addr.s_addr);
            unsigned long a2 = htonl(addr2.sin_addr.s_addr);

            unsigned long shift = 32 - mask;
            assert(shift < 32);                /* Undefined behaviour if 32 */

            a1 = a1 >> shift;
            a2 = a2 >> shift;

            if (a1 == a2)
            {
                return 0;
            }
            else
            {
                return -1;
            }
        }
        else
        {
            long i, from = -1, to = -1, cmp = -1;
            char buffer1[64], buffer2[64];

            const char *sp1 = s1;
            const char *sp2 = s2;

            for (i = 0; i < 4; i++)
            {
                buffer1[0] = '\0';
                sscanf(sp1, "%63[^.]", buffer1);
                buffer1[63] = '\0';

                if (strlen(buffer1) == 0)
                {
                    break;
                }

                sp1 += strlen(buffer1) + 1;

                sscanf(sp2, "%63[^.]", buffer2);
                buffer2[63] = '\0';

                sp2 += strlen(buffer2) + 1;

                if (strstr(buffer1, "-"))
                {
                    sscanf(buffer1, "%ld-%ld", &from, &to);
                    sscanf(buffer2, "%ld", &cmp);

                    if ((from < 0) || (to < 0))
                    {
                        Log(LOG_LEVEL_DEBUG, "Couldn't read range");
                        return -1;
                    }

                    if ((from > cmp) || (cmp > to))
                    {
                        Log(LOG_LEVEL_DEBUG, "Out of range %ld > %ld > %ld, range '%s'", from, cmp, to, buffer2);
                        return -1;
                    }
                }
                else
                {
                    sscanf(buffer1, "%ld", &from);
                    sscanf(buffer2, "%ld", &cmp);

                    if (from != cmp)
                    {
                        Log(LOG_LEVEL_DEBUG, "Unequal");
                        return -1;
                    }
                }

                Log(LOG_LEVEL_DEBUG, "Matched octet '%s' with '%s'", buffer1, buffer2);
            }

            Log(LOG_LEVEL_DEBUG, "Matched IP range");
            return 0;
        }
    }

    if (isv6)
    {
        int i;

        if (isCIDR)
        {
            int blocks;
            struct sockaddr_in6 addr1 = {0};
            struct sockaddr_in6 addr2 = {0};
            unsigned long mask;

            address[0] = '\0';
            int ret = sscanf(s1, "%40[^/]/%lu", address, &mask);
            if (ret != 2 || mask > 128)
            {
                Log(LOG_LEVEL_ERR, "Invalid IPv6 CIDR: %s", s1);
                return -1;
            }
            blocks = mask / 8;

            if (mask % 8 != 0)
            {
                Log(LOG_LEVEL_ERR, "Cannot handle ipv6 masks which are not 8 bit multiples (fix me)");
                return -1;
            }

            addr1.sin6_family = AF_INET6;
            inet_pton(AF_INET6, address, &addr1.sin6_addr);
            addr2.sin6_family = AF_INET6;
            inet_pton(AF_INET6, s2, &addr2.sin6_addr);

            for (i = 0; i < blocks; i++)        /* blocks < 16 */
            {
                if (addr1.sin6_addr.s6_addr[i] != addr2.sin6_addr.s6_addr[i])
                {
                    return -1;
                }
            }
            return 0;
        }
        else
        {
            long i, from = -1, to = -1, cmp = -1;
            char buffer1[64], buffer2[64];

            const char *sp1 = s1;
            const char *sp2 = s2;

            for (i = 0; i < 8; i++)
            {
                sscanf(sp1, "%63[^:]", buffer1);
                buffer1[63] = '\0';

                sp1 += strlen(buffer1) + 1;

                sscanf(sp2, "%63[^:]", buffer2);
                buffer2[63] = '\0';

                sp2 += strlen(buffer2) + 1;

                if (strstr(buffer1, "-"))
                {
                    sscanf(buffer1, "%lx-%lx", &from, &to);
                    sscanf(buffer2, "%lx", &cmp);

                    if (from < 0 || to < 0)
                    {
                        return -1;
                    }

                    if ((from >= cmp) || (cmp > to))
                    {
                        Log(LOG_LEVEL_DEBUG, "%lx < %lx < %lx", from, cmp, to);
                        return -1;
                    }
                }
                else
                {
                    sscanf(buffer1, "%ld", &from);
                    sscanf(buffer2, "%ld", &cmp);

                    if (from != cmp)
                    {
                        return -1;
                    }
                }
            }

            return 0;
        }
    }

    return -1;
}

bool FuzzyHostParse(const char *arg2)
{
    long start = -1, end = -1;
    int n;

    n = sscanf(arg2, "%ld-%ld", &start, &end);

    if (n != 2)
    {
        Log(LOG_LEVEL_ERR,
              "HostRange syntax error: second arg should have X-Y format where X and Y are decimal numbers");
        return false;
    }

    return true;
}

int FuzzyHostMatch(const char *arg0, const char *arg1, const char *refhost)
{
    char *sp, refbase[CF_MAXVARSIZE];
    long cmp = -1, start = -1, end = -1;
    char buf1[CF_BUFSIZE], buf2[CF_BUFSIZE];

    strlcpy(refbase, refhost, CF_MAXVARSIZE);
    sp = refbase + strlen(refbase) - 1;

    while (isdigit((int) *sp))
    {
        sp--;
    }

    sp++;
    sscanf(sp, "%ld", &cmp);
    *sp = '\0';

    if (cmp < 0)
    {
        return 1;
    }

    if (strlen(refbase) == 0)
    {
        return 1;
    }

    sscanf(arg1, "%ld-%ld", &start, &end);

    if ((cmp < start) || (cmp > end))
    {
        return 1;
    }

    strlcpy(buf1, refbase, CF_BUFSIZE);
    strlcpy(buf2, arg0, CF_BUFSIZE);

    ToLowerStrInplace(buf1);
    ToLowerStrInplace(buf2);

    if (strcmp(buf1, buf2) != 0)
    {
        return 1;
    }

    return 0;
}

bool FuzzyMatchParse(const char *s)
{
    short isCIDR = false, isrange = false, isv6 = false, isv4 = false, isADDR = false;
    char address[CF_ADDRSIZE];
    int mask, count = 0;

    for (const char *sp = s; *sp != '\0'; sp++)     /* Is this an address or hostname */
    {
        if (!isxdigit((int) *sp))
        {
            isADDR = false;
            break;
        }

        if (*sp == ':')         /* Catches any ipv6 address */
        {
            isADDR = true;
            break;
        }

        if (isdigit((int) *sp)) /* catch non-ipv4 address - no more than 3 digits */
        {
            count++;
            if (count > 3)
            {
                isADDR = false;
                break;
            }
        }
        else
        {
            count = 0;
        }
    }

    if (!isADDR)
    {
        return true;
    }

    if (strstr(s, "/") != 0)
    {
        isCIDR = true;
    }

    if (strstr(s, "-") != 0)
    {
        isrange = true;
    }

    if (strstr(s, ".") != 0)
    {
        isv4 = true;
    }

    if (strstr(s, ":") != 0)
    {
        isv6 = true;
    }

    if (isv4 && isv6)
    {
        Log(LOG_LEVEL_ERR, "Mixture of IPv6 and IPv4 addresses");
        return false;
    }

    if (isCIDR && isrange)
    {
        Log(LOG_LEVEL_ERR, "Cannot mix CIDR notation with xx-yy range notation");
        return false;
    }

    if (isv4 && isCIDR)
    {
        if (strlen(s) > 4 + 3 * 4 + 1 + 2)      /* xxx.yyy.zzz.mmm/cc */
        {
            Log(LOG_LEVEL_ERR, "IPv4 address looks too long");
            return false;
        }

        address[0] = '\0';
        mask = 0;
        sscanf(s, "%16[^/]/%d", address, &mask);

        if (mask < 8)
        {
            Log(LOG_LEVEL_ERR, "Mask value %d in '%s' is less than 8", mask, s);
            return false;
        }

        if (mask > 30)
        {
            Log(LOG_LEVEL_ERR, "Mask value %d in '%s' is silly (> 30)", mask, s);
            return false;
        }
    }

    if (isv4 && isrange)
    {
        long i, from = -1, to = -1;
        char buffer1[64];

        const char *sp1 = s;

        for (i = 0; i < 4; i++)
        {
            buffer1[0] = '\0';
            sscanf(sp1, "%63[^.]", buffer1);
            sp1 += strlen(buffer1) + 1;

            if (strstr(buffer1, "-"))
            {
                sscanf(buffer1, "%ld-%ld", &from, &to);

                if ((from < 0) || (to < 0))
                {
                    Log(LOG_LEVEL_ERR, "Error in IP range - looks like address, or bad hostname");
                    return false;
                }

                if (to < from)
                {
                    Log(LOG_LEVEL_ERR, "Bad IP range");
                    return false;
                }

            }
        }
    }

    if (isv6 && isCIDR)
    {
        char address[CF_ADDRSIZE];
        int mask;

        if (strlen(s) < 20)
        {
            Log(LOG_LEVEL_ERR, "IPv6 address looks too short");
            return false;
        }

        if (strlen(s) > 42)
        {
            Log(LOG_LEVEL_ERR, "IPv6 address looks too long");
            return false;
        }

        address[0] = '\0';
        mask = 0;
        sscanf(s, "%40[^/]/%d", address, &mask);

        if (mask % 8 != 0)
        {
            Log(LOG_LEVEL_ERR, "Cannot handle ipv6 masks which are not 8 bit multiples (fix me)");
            return false;
        }

        if (mask > 15)
        {
            Log(LOG_LEVEL_ERR, "IPv6 CIDR mask is too large");
            return false;
        }
    }

    return true;
}

/* FIXME: handle 127.0.0.2, 127.255.255.254, ::1,
 * 0000:0000:0000:0000:0000:0000:0000:0001, 0:00:000:0000:000:00:0:1, 0::1 and
 * other variants
 */

bool IsLoopbackAddress(const char *address)
{
    if(strcmp(address, "localhost") == 0)
    {
        return true;
    }

    if(strcmp(address, "127.0.0.1") == 0)
    {
        return true;
    }

    return false;
}

char *TrimWhitespace(char *s)
{
    if ( NULL_OR_EMPTY(s) )
    {
        return NULL;
    }

    // Leading whitespace:
    while (isspace(s[0]))
    {
        ++s;
    }

    // Empty string (only whitespace):
    if (s[0] == '\0')
    {
        return NULL;
    }

    // Trailing whitespace:
    char *end = s + strlen(s) - 1; // Last byte before '\0'
    while ( isspace(end[0]) )
    {
        --end;
    }
    end[1] = '\0';   // Null terminate string after last non space char

    return s;
}

/**
 * Simple check to avoid writing to illegal memory addresses.
 * NOT a proper test for valid IP.
 */
static AddressType AddressTypeCheckValidity(char *s, AddressType address_type)
{
    if(NULL_OR_EMPTY(s))
    {
        return ADDRESS_TYPE_OTHER;
    }
    if(strlen(s) >= CF_MAX_IP_LEN)
    {
        return ADDRESS_TYPE_OTHER;
    }
    return address_type;
}

/**
 * Parses "hostname:port" or "[hostname]:port", where hostname may also be
 * IPv4 or IPv6 address string.
 *
 * @param hostname will point to the hostname, or NULL if no or empty hostname
 * @param port will point to the port, or NULL if no or empty port
 * @WARNING modifies #s to '\0' terminate hostname if followed by port.
 */
AddressType ParseHostPort(char *s, char **hostname, char **port)
{
    s = TrimWhitespace(s);
    if ( NULL_OR_EMPTY(s) )
    {
        *hostname = NULL;
        *port     = NULL;
        return ADDRESS_TYPE_OTHER;
    }

    AddressType address_type = ADDRESS_TYPE_OTHER;
    char *h, *p; // hostname, port temporaries

    h = s;
    p = NULL;

    char *first_colon = strchr(s, ':');
    char *first_dot   = strchr(s, '.');

    if (s[0] == '[')     // [host or ip]:port
    {
        h = s + 1;
        p = strchr(h, ']');
        if (p != NULL)
        {
            if (first_colon != NULL && first_colon < p)
            {
                address_type = ADDRESS_TYPE_IPV6;
            }
            else if (isdigit(h[0]))
            {
                address_type = ADDRESS_TYPE_IPV4;
            } // (else it's other by default)

            *p = '\0';        // '\0' terminate host name
            if (p[1] == ':')  // move port* forward
            {
                p += 2;
            }
        }
    }
    else if (first_colon == NULL)    // localhost, 192.168.0.1
    {
        if (isdigit(h[0]))
        {
            address_type = ADDRESS_TYPE_IPV4;
        }
    }
    else if (first_dot == NULL || first_colon < first_dot)
    {
        // If only one colon: (cfengine.com:222 or localhost:)
        if (strchr(first_colon + 1, ':') == NULL)
        {
            *first_colon = '\0';
            p = first_colon + 1;
        }
        else // Multiple colons:
        {
            address_type = ADDRESS_TYPE_IPV6;
        }
    }
    else // (first_dot < first_colon) : IPv4 or hostname
    {
        p = strchr(h, ':');
        if (p != NULL)
        {
            *p = '\0'; // '\0'-terminate hostname
            p++;
        }
        if (isdigit(h[0]))
        {
            address_type = ADDRESS_TYPE_IPV4;
        }
    }

    *hostname =              (h[0] != '\0') ? h : NULL;
    *port     = (p != NULL && p[0] != '\0') ? p : NULL;

    return AddressTypeCheckValidity(*hostname, address_type);
}
