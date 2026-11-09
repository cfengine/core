/*
  Copyright 2024 Northern.tech AS

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

#include <addr_lib.h>
#include <cfnet.h>

#include <logging.h>
#include <string_lib.h>

#define CF_ADDRSIZE 128

/* Notation used by an address pattern, see FuzzySetMatch() */
typedef enum
{
    PATTERN_PREFIX, // "10.1.2.3", "10.1.2"
    PATTERN_CIDR,   // "10.1.2.0/24", "2001:db8::/32"
    PATTERN_RANGE,  // "10.1.2.1-50", "2001:db8:0-ff::1"
} PatternNotation;

/**
 * Determines the notation of an address pattern.
 *
 * @return false (and logs) if CIDR and range notation are mixed
 */
static bool GetPatternNotation(const char *pattern, PatternNotation *notation)
{
    const bool is_cidr = (strchr(pattern, '/') != NULL);
    const bool is_range = (strchr(pattern, '-') != NULL);

    if (is_cidr && is_range)
    {
        Log(LOG_LEVEL_ERR,
            "Cannot mix CIDR notation with xxx-yyy range notation '%s'", pattern);
        return false;
    }

    *notation = is_cidr ? PATTERN_CIDR : (is_range ? PATTERN_RANGE : PATTERN_PREFIX);
    return true;
}

/******************************************************************************/
/* Matching                                                                   */
/******************************************************************************/

/**
 * Plain string prefix match, at octet boundaries, i.e. "10.1.2" matches
 * "10.1.2.3", but "10.1.2.1" does not match "10.1.2.12".
 */
static bool MatchPrefix(const char *pattern, const char *address)
{
    const size_t len = strlen(pattern);
    return strncmp(pattern, address, len) == 0 &&
           (address[len] == '\0' || address[len] == '.');
}

static bool MatchIPv4CIDR(const char *pattern, const char *address)
{
    struct sockaddr_in addr1, addr2;
    char network[CF_ADDRSIZE];
    unsigned long mask;

    // Checks that the addr to match is a valid address
    // else bogus addresses will always match /0 range
    if (inet_pton(AF_INET, address, &addr2.sin_addr) != 1)
    {
        Log(LOG_LEVEL_ERR, "Invalid reference IPv4: %s", address);
        return false;
    }

    network[0] = '\0';
    int ret = sscanf(pattern, "%16[^/]/%lu", network, &mask);
    if (ret != 2 || mask > 32)
    {
        Log(LOG_LEVEL_ERR, "Invalid IPv4 CIDR: %s", pattern);
        return false;
    }
    else if (mask == 0)
    {
        return true;                     /* /0 CIDR matches everything */
    }

    inet_pton(AF_INET, network, &addr1.sin_addr);

    unsigned long a1 = htonl(addr1.sin_addr.s_addr);
    unsigned long a2 = htonl(addr2.sin_addr.s_addr);

    unsigned long shift = 32 - mask;
    assert(shift < 32);                /* Undefined behaviour if 32 */

    return (a1 >> shift) == (a2 >> shift);
}

static bool MatchIPv6CIDR(const char *pattern, const char *address)
{
    struct sockaddr_in6 addr1 = {0};
    struct sockaddr_in6 addr2 = {0};
    char network[CF_ADDRSIZE];
    unsigned long mask;

    network[0] = '\0';
    int ret = sscanf(pattern, "%40[^/]/%lu", network, &mask);
    if (ret != 2 || mask > 128)
    {
        Log(LOG_LEVEL_ERR, "Invalid IPv6 CIDR: %s", pattern);
        return false;
    }
    int blocks = mask / 8;
    int remaining_bits = mask % 8;

    addr1.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, network, &addr1.sin6_addr) != 1)
    {
        return false;
    }
    addr2.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, address, &addr2.sin6_addr) != 1)
    {
        return false;
    }

    for (int i = 0; i < blocks; i++)
    {
        if (addr1.sin6_addr.s6_addr[i] != addr2.sin6_addr.s6_addr[i])
        {
            return false;
        }
    }

    if (remaining_bits > 0)
    {
        uint8_t bitmask = (uint8_t)(0xFF << (8 - remaining_bits));

        if ((addr1.sin6_addr.s6_addr[blocks] & bitmask) !=
            (addr2.sin6_addr.s6_addr[blocks] & bitmask))
        {
            return false;
        }
    }
    return true;
}

static bool MatchIPv4Range(const char *pattern, const char *address)
{
    long from = -1, to = -1, cmp = -1;
    char buffer1[64], buffer2[64];

    const char *sp1 = pattern;
    const char *sp2 = address;

    for (int i = 0; i < 4; i++)
    {
        buffer1[0] = '\0';
        sscanf(sp1, "%63[^.]", buffer1);
        buffer1[63] = '\0';

        if (strlen(buffer1) == 0)
        {
            break;
        }

        sp1 += strlen(buffer1);
        if (*sp1 == '.')
        {
            sp1++;
        }

        buffer2[0] = '\0';
        sscanf(sp2, "%63[^.]", buffer2);
        buffer2[63] = '\0';

        if (strlen(buffer2) == 0)
        {
            /* address has fewer octets than the pattern: no match */
            return false;
        }

        sp2 += strlen(buffer2);
        if (*sp2 == '.')
        {
            sp2++;
        }

        if (strstr(buffer1, "-"))
        {
            sscanf(buffer1, "%ld-%ld", &from, &to);
            sscanf(buffer2, "%ld", &cmp);

            if ((from < 0) || (to < 0))
            {
                Log(LOG_LEVEL_DEBUG, "Couldn't read range");
                return false;
            }

            if ((from > cmp) || (cmp > to))
            {
                Log(LOG_LEVEL_DEBUG, "Out of range %ld > %ld > %ld, range '%s'", from, cmp, to, buffer2);
                return false;
            }
        }
        else
        {
            sscanf(buffer1, "%ld", &from);
            sscanf(buffer2, "%ld", &cmp);

            if (from != cmp)
            {
                Log(LOG_LEVEL_DEBUG, "Unequal");
                return false;
            }
        }

        Log(LOG_LEVEL_DEBUG, "Matched octet '%s' with '%s'", buffer1, buffer2);
    }

    Log(LOG_LEVEL_DEBUG, "Matched IP range");
    return true;
}

static bool MatchIPv6Range(const char *pattern, const char *address)
{
    long from = -1, to = -1, cmp = -1;
    char buffer1[64], buffer2[64];

    const char *sp1 = pattern;
    const char *sp2 = address;

    for (int i = 0; i < 8; i++)
    {
        buffer1[0] = '\0';
        sscanf(sp1, "%63[^:]", buffer1);
        buffer1[63] = '\0';

        sp1 += strlen(buffer1);
        if (*sp1 == ':')
        {
            sp1++;
        }

        buffer2[0] = '\0';
        sscanf(sp2, "%63[^:]", buffer2);
        buffer2[63] = '\0';

        sp2 += strlen(buffer2);
        if (*sp2 == ':')
        {
            sp2++;
        }

        if (strstr(buffer1, "-"))
        {
            sscanf(buffer1, "%lx-%lx", &from, &to);
            sscanf(buffer2, "%lx", &cmp);

            if (from < 0 || to < 0)
            {
                return false;
            }

            if ((from >= cmp) || (cmp > to))
            {
                Log(LOG_LEVEL_DEBUG, "%lx < %lx < %lx", from, cmp, to);
                return false;
            }
        }
        else
        {
            sscanf(buffer1, "%ld", &from);
            sscanf(buffer2, "%ld", &cmp);

            if (from != cmp)
            {
                return false;
            }
        }
    }

    return true;
}

/******************************************************************************/
/* Public API                                                                 */
/******************************************************************************/

/* Match two IP strings - with : or . in hex or decimal
   s1 is the test string, and s2 is the reference e.g.
   FuzzySetMatch("128.39.74.10/23","128.39.75.56") == 0

   s1 may be a CIDR ("10.0.0.0/8", "2001:db8::/32"), a range
   ("10.0.0.1-50", "2001:db8:0-ff::1") or an address prefix ("10.1.2").

   Returns 0 on match. */

/* TODO rename to AddrSubnetMatch() */
int FuzzySetMatch(const char *s1, const char *s2)
{
    if (strcmp(s1, s2) == 0)
    {
        return 0;
    }

    const bool is_v4 = (strchr(s1, '.') != NULL || strchr(s2, '.') != NULL);
    const bool is_v6 = (strchr(s1, ':') != NULL || strchr(s2, ':') != NULL);

    if (is_v4 && is_v6)
    {
        return -1; // Mixed IPv4 and IPv6
    }

    PatternNotation notation;
    if (!GetPatternNotation(s1, &notation))
    {
        return -1;
    }

    if (!is_v4 && !is_v6)
    {
        Log(LOG_LEVEL_ERR, "Not a valid address range - or not a fully qualified name '%s'", s1);
        return -1;
    }

    bool match = false;
    switch (notation)
    {
    case PATTERN_PREFIX:
        match = MatchPrefix(s1, s2);
        break;
    case PATTERN_CIDR:
        match = is_v4 ? MatchIPv4CIDR(s1, s2) : MatchIPv6CIDR(s1, s2);
        break;
    case PATTERN_RANGE:
        match = is_v4 ? MatchIPv4Range(s1, s2) : MatchIPv6Range(s1, s2);
        break;
    }

    return match ? 0 : -1;
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
            buffer1[63] = '\0';

            if (strlen(buffer1) == 0)
            {
                break;
            }

            sp1 += strlen(buffer1);
            if (*sp1 == '.')
            {
                sp1++;
            }

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

        if (mask > 15)
        {
            Log(LOG_LEVEL_ERR, "IPv6 CIDR mask is too large");
            return false;
        }
    }

    return true;
}


bool FuzzyHostParse(const char *arg2)
{
    long start = -1, end = -1;
    if (sscanf(arg2, "%ld-%ld", &start, &end) != 2)
    {
        Log(LOG_LEVEL_ERR,
            "HostRange syntax error: second arg should have X-Y format where X and Y are decimal numbers");
        return false;
    }

    return true;
}

int FuzzyHostMatch(const char *arg0, const char *arg1, const char *refhost)
{
    char *sp, refbase[1024];
    long cmp = -1, start = -1, end = -1;
    char buf1[CF_BUFSIZE], buf2[CF_BUFSIZE];

    strlcpy(refbase, refhost, sizeof(refbase));
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

/**
 * Parses "hostname:port" or "[hostname]:port", where hostname may also be
 * IPv4 or IPv6 address string.
 *
 * @param hostname will point to the hostname, or NULL if no or empty hostname
 * @param port will point to the port, or NULL if no or empty port
 * @return the type of hostname: ADDRESS_TYPE_IPV4 if it starts with a digit,
 *         ADDRESS_TYPE_IPV6 if it looks like an IPv6 address, otherwise
 *         (hostname, invalid or too long) ADDRESS_TYPE_OTHER
 * @WARNING modifies #s to '\0' terminate hostname if followed by port.
 */
AddressType ParseHostPort(char *s, char **hostname, char **port)
{
    s = TrimWhitespace(s);
    if (NULL_OR_EMPTY(s))
    {
        *hostname = NULL;
        *port     = NULL;
        return ADDRESS_TYPE_OTHER;
    }

    char *host = s;
    char *port_str = NULL;
    bool is_ipv6 = false;
    bool classify = true;

    if (s[0] == '[')               // [host or ip]:port
    {
        host = s + 1;
        char *closing = strchr(host, ']');
        if (closing != NULL)
        {
            *closing = '\0';
            is_ipv6 = (strchr(host, ':') != NULL);
            if (closing[1] == ':')
            {
                port_str = closing + 2;
            }
        }
        else
        {
            classify = false;      // Unterminated bracket
        }
    }
    else
    {
        char *first_colon = strchr(s, ':');
        char *first_dot   = strchr(s, '.');

        if (first_colon != NULL)
        {
            if (first_dot != NULL && first_dot < first_colon)
            {
                // "1.2.3.4:80" or "cfengine.com:80"
                *first_colon = '\0';
                port_str = first_colon + 1;
            }
            else if (strchr(first_colon + 1, ':') == NULL)
            {
                // "localhost:80" or "localhost:"
                *first_colon = '\0';
                port_str = first_colon + 1;
                classify = false;
            }
            else                   // Multiple colons: IPv6 address, no port
            {
                is_ipv6 = true;
            }
        }
    }

    *hostname = (host[0] != '\0') ? host : NULL;
    *port     = (port_str != NULL && port_str[0] != '\0') ? port_str : NULL;

    if (!classify || *hostname == NULL || strlen(*hostname) >= CF_MAX_IP_LEN)
    {
        return ADDRESS_TYPE_OTHER;
    }
    if (is_ipv6)
    {
        return ADDRESS_TYPE_IPV6;
    }

    return isdigit((unsigned char) (*hostname)[0])
        ? ADDRESS_TYPE_IPV4
        : ADDRESS_TYPE_OTHER;
}
