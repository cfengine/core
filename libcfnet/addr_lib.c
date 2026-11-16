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
#define IPV4_OCTETS 4
#define IPV6_GROUPS 8

/* Inclusive interval matched by one octet/group of a range pattern,
 * e.g. "10-20", or "7" which is the interval 7-7. */
typedef struct
{
    unsigned long from;
    unsigned long to;
} Interval;

/* Notation used by an address pattern, see FuzzySetMatch() */
typedef enum
{
    PATTERN_PREFIX, // "10.1.2.3", "10.1.2"
    PATTERN_CIDR,   // "10.1.2.0/24", "2001:db8::/32"
    PATTERN_RANGE,  // "10.1.2.1-50", "2001:db8:0-ff::1"
} PatternNotation;

/******************************************************************************/
/* Parsing helpers                                                            */
/******************************************************************************/

/**
 * Parses the unsigned number in [start, end), written in the given base
 * (10 or 16). Unlike strtoul(), no sign, whitespace or "0x" is accepted.
 *
 * @return false if the input is empty, has invalid digits or exceeds max
 */
static bool ParseNumber(const char *start, const char *end, int base,
                        unsigned long max, unsigned long *value)
{
    assert(start != NULL);
    assert(end != NULL);
    assert(start <= end);
    assert(base == 10 || base == 16);

    if (start == end)
    {
        return false;
    }

    unsigned long result = 0;
    for (const char *c = start; c < end; c++)
    {
        const unsigned char ch = *c;
        unsigned long digit;
        if (isdigit(ch))
        {
            digit = ch - '0';
        }
        else if (base == 16 && isxdigit(ch))
        {
            digit = tolower(ch) - 'a' + 10;
        }
        else
        {
            return false;
        }

        if (result > (max - digit) / base)
        {
            return false;
        }
        result = result * base + digit;
    }

    *value = result;
    return true;
}

/**
 * Parses "N" or "FROM-TO" in [start, end) into an interval.
 *
 * @return false if a number is invalid, exceeds max, or FROM > TO
 */
static bool ParseInterval(const char *start, const char *end, int base,
                          unsigned long max, Interval *interval)
{
    assert(interval != NULL);

    const char *dash = memchr(start, '-', end - start);
    if (dash == NULL)
    {
        if (!ParseNumber(start, end, base, max, &interval->from))
        {
            return false;
        }
        interval->to = interval->from;
        return true;
    }

    return ParseNumber(start, dash, base, max, &interval->from) &&
           ParseNumber(dash + 1, end, base, max, &interval->to) &&
           interval->from <= interval->to;
}

/**
 * Parses an IPv4 pattern of 1 to 4 '.'-separated octets, each of which is a
 * decimal number or range, e.g. "10.0.0.1-50", "10.1-5" or "10.1.2.3".
 *
 * @return the number of octets parsed, or 0 if the pattern is invalid
 */
static size_t ParseIPv4Pattern(const char *pattern, Interval octets[IPV4_OCTETS])
{
    const char *start = pattern;
    for (size_t count = 0; count < IPV4_OCTETS; count++)
    {
        const char *end = start + strcspn(start, ".");
        if (!ParseInterval(start, end, 10, 255, &octets[count]))
        {
            return 0;
        }

        if (*end == '\0')
        {
            return count + 1;
        }
        start = end + 1;
    }

    return 0; // More than 4 octets
}

/**
 * Parses up to max ':'-separated hex groups (numbers or ranges) in
 * [start, end). An empty input yields 0 groups.
 */
static bool ParseIPv6Groups(const char *start, const char *end,
                            Interval *groups, size_t max, size_t *count)
{
    assert(start <= end);

    *count = 0;
    if (start == end)
    {
        return true;
    }

    while (*count < max)
    {
        const char *sep = memchr(start, ':', end - start);
        const char *group_end = (sep != NULL) ? sep : end;
        if (!ParseInterval(start, group_end, 16, 0xFFFF, &groups[*count]))
        {
            return false;
        }
        (*count)++;

        if (sep == NULL)
        {
            return true;
        }
        start = sep + 1;
    }

    return false; // Too many groups
}

/**
 * Parses an IPv6 range pattern into exactly 8 groups, expanding "::",
 * e.g. "2001:db8:1-5::1".
 */
static bool ParseIPv6RangePattern(const char *pattern, Interval groups[IPV6_GROUPS])
{
    const char *end = pattern + strlen(pattern);
    const char *gap = strstr(pattern, "::");
    size_t count;

    if (gap == NULL)
    {
        if (!ParseIPv6Groups(pattern, end, groups, IPV6_GROUPS, &count) ||
            count != IPV6_GROUPS)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Invalid IPv6 range '%s': expected 8 valid groups, or '::'",
                pattern);
            return false;
        }
        return true;
    }

    if (strstr(gap + 1, "::") != NULL)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Invalid IPv6 range '%s': '::' may only appear once", pattern);
        return false;
    }

    // "::" stands for at least one zero group
    size_t head_count, tail_count;
    Interval tail[IPV6_GROUPS];
    if (!ParseIPv6Groups(pattern, gap, groups, IPV6_GROUPS - 1, &head_count) ||
        !ParseIPv6Groups(gap + 2, end, tail, IPV6_GROUPS - 1 - head_count, &tail_count))
    {
        Log(LOG_LEVEL_VERBOSE,
            "Invalid IPv6 range '%s': invalid group, or more than 7 groups besides '::'",
            pattern);
        return false;
    }

    const size_t zero_count = IPV6_GROUPS - head_count - tail_count;
    for (size_t i = head_count; i < head_count + zero_count; i++)
    {
        groups[i] = (Interval) { .from = 0, .to = 0 };
    }
    memcpy(groups + head_count + zero_count, tail, tail_count * sizeof(*tail));
    return true;
}

/**
 * Determines the notation of an address pattern: CIDR if it contains a '/',
 * range if it contains a '-', otherwise prefix.
 *
 * @param pattern the address pattern, e.g. "10.1.2.0/24"
 * @param notation receives the notation of the pattern
 * @return true on success, false (and logs an error) if the pattern mixes
 *         CIDR and range notation, in which case notation is not set
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

    if (is_cidr)
    {
        *notation = PATTERN_CIDR;
    }
    else if (is_range)
    {
        *notation = PATTERN_RANGE;
    }
    else
    {
        *notation = PATTERN_PREFIX;
    }
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

/**
 * Matches an IPv4 range pattern. A pattern with fewer than 4 octets matches
 * on the leading octets only, e.g. "10.1-5" matches "10.3.7.8".
 */
static bool MatchIPv4Range(const char *pattern, const char *address)
{
    Interval octets[IPV4_OCTETS];
    const size_t count = ParseIPv4Pattern(pattern, octets);
    if (count == 0)
    {
        Log(LOG_LEVEL_DEBUG, "Invalid IPv4 range '%s'", pattern);
        return false;
    }

    const char *start = address;
    for (size_t i = 0; i < count; i++)
    {
        const char *end = start + strcspn(start, ".");
        unsigned long value;
        if (!ParseNumber(start, end, 10, 255, &value))
        {
            Log(LOG_LEVEL_VERBOSE, "Invalid IPv4 address '%s'", address);
            return false;
        }
        if (value < octets[i].from || value > octets[i].to)
        {
            return false;
        }

        if (*end == '\0')
        {
            if (i + 1 < count)
            {
                Log(LOG_LEVEL_VERBOSE,
                    "IPv4 address '%s' has fewer octets than range '%s'",
                    address, pattern);
                return false;
            }
            return true;
        }
        start = end + 1;
    }

    return true;
}

static bool MatchIPv6Range(const char *pattern, const char *address)
{
    Interval groups[IPV6_GROUPS];
    if (!ParseIPv6RangePattern(pattern, groups))
    {
        Log(LOG_LEVEL_DEBUG, "Invalid IPv6 range '%s'", pattern);
        return false;
    }

    struct in6_addr addr;
    if (inet_pton(AF_INET6, address, &addr) != 1)
    {
        return false;
    }

    for (size_t i = 0; i < IPV6_GROUPS; i++)
    {
        const unsigned long value = (addr.s6_addr[2 * i] << 8) | addr.s6_addr[2 * i + 1];
        if (value < groups[i].from || value > groups[i].to)
        {
            return false;
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


/**
 * Parses "hostname:port" or "[hostname]:port", where hostname may also be
 * IPv4 or IPv6 address string.
 *
 * @param hostname will point to the hostname, or NULL if no or empty hostname
 * @param port will point to the port, or NULL if no or empty port
 * @return the type of hostname: ADDRESS_TYPE_IPV4 if it starts with a digit,
 *         ADDRESS_TYPE_IPV6 if it contains a ':' (not validated), otherwise
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
