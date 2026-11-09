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

#define IPV4_OCTETS 4
#define IPV6_GROUPS 8

/* Large enough for the binary form of both IPv4 and IPv6 addresses */
#define BINARY_ADDR_SIZE sizeof(struct in6_addr)

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
        return ParseIPv6Groups(pattern, end, groups, IPV6_GROUPS, &count) &&
               count == IPV6_GROUPS;
    }

    if (strstr(gap + 1, "::") != NULL)
    {
        return false; // More than one "::" (or ":::")
    }

    // "::" stands for at least one zero group
    size_t head_count, tail_count;
    Interval tail[IPV6_GROUPS];
    if (!ParseIPv6Groups(pattern, gap, groups, IPV6_GROUPS - 1, &head_count) ||
        !ParseIPv6Groups(gap + 2, end, tail, IPV6_GROUPS - 1 - head_count, &tail_count))
    {
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
 * Parses a CIDR pattern "ADDRESS/BITS" of the given family.
 *
 * @param address receives the binary network address (BINARY_ADDR_SIZE bytes)
 */
static bool ParseCIDR(const char *pattern, int family,
                      unsigned char *address, unsigned long *bits)
{
    assert(family == AF_INET || family == AF_INET6);

    const char *slash = strchr(pattern, '/');
    if (slash == NULL)
    {
        return false;
    }

    char address_str[CF_MAX_IP_LEN];
    const size_t address_len = slash - pattern;
    if (address_len >= sizeof(address_str))
    {
        return false;
    }
    memcpy(address_str, pattern, address_len);
    address_str[address_len] = '\0';

    const unsigned long max_bits = (family == AF_INET) ? 32 : 128;
    return inet_pton(family, address_str, address) == 1 &&
           ParseNumber(slash + 1, slash + strlen(slash), 10, max_bits, bits);
}

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

/* True if the first bits of a and b are equal */
static bool LeadingBitsEqual(const unsigned char *a, const unsigned char *b,
                             unsigned long bits)
{
    const size_t whole_bytes = bits / 8;
    const unsigned int remaining_bits = bits % 8;

    if (memcmp(a, b, whole_bytes) != 0)
    {
        return false;
    }
    if (remaining_bits == 0)
    {
        return true;
    }

    const unsigned char mask = (unsigned char) (0xFF << (8 - remaining_bits));
    return (a[whole_bytes] & mask) == (b[whole_bytes] & mask);
}

static bool MatchCIDR(const char *pattern, int family, const char *address)
{
    const char *family_name = (family == AF_INET) ? "IPv4" : "IPv6";

    unsigned char network[BINARY_ADDR_SIZE];
    unsigned long bits;
    if (!ParseCIDR(pattern, family, network, &bits))
    {
        Log(LOG_LEVEL_ERR, "Invalid %s CIDR: %s", family_name, pattern);
        return false;
    }

    // Checks that the address to match is valid, else bogus addresses
    // would always match a /0 range
    unsigned char addr[BINARY_ADDR_SIZE];
    if (inet_pton(family, address, addr) != 1)
    {
        Log(LOG_LEVEL_ERR, "Invalid reference %s: %s", family_name, address);
        return false;
    }

    return LeadingBitsEqual(network, addr, bits);
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
        if (!ParseNumber(start, end, 10, 255, &value) ||
            value < octets[i].from || value > octets[i].to)
        {
            return false;
        }

        if (*end == '\0')
        {
            return (i + 1 == count); // Address must have at least count octets
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
        match = MatchCIDR(s1, is_v4 ? AF_INET : AF_INET6, s2);
        break;
    case PATTERN_RANGE:
        match = is_v4 ? MatchIPv4Range(s1, s2) : MatchIPv6Range(s1, s2);
        break;
    }

    return match ? 0 : -1;
}

/**
 * Checks that s is an address pattern understood by FuzzySetMatch().
 */
bool FuzzyMatchParse(const char *s)
{
    const bool is_v4 = (strchr(s, '.') != NULL);
    const bool is_v6 = (strchr(s, ':') != NULL);

    if (is_v4 && is_v6)
    {
        Log(LOG_LEVEL_ERR, "Mixture of IPv6 and IPv4 addresses in '%s'", s);
        return false;
    }

    if (!is_v4 && !is_v6)
    {
        Log(LOG_LEVEL_ERR, "'%s' is not an IP address, range or subnet", s);
        return false;
    }

    PatternNotation notation;
    if (!GetPatternNotation(s, &notation))
    {
        return false;
    }

    bool valid = false;
    if (notation == PATTERN_CIDR)
    {
        unsigned char network[BINARY_ADDR_SIZE];
        unsigned long bits;
        valid = ParseCIDR(s, is_v4 ? AF_INET : AF_INET6, network, &bits);
    }
    else if (is_v4) // Range or prefix
    {
        Interval octets[IPV4_OCTETS];
        valid = (ParseIPv4Pattern(s, octets) > 0);
    }
    else if (notation == PATTERN_RANGE) // IPv6
    {
        Interval groups[IPV6_GROUPS];
        valid = ParseIPv6RangePattern(s, groups);
    }
    else // IPv6 full address, or prefix of 1-7 groups
    {
        struct in6_addr addr;
        Interval groups[IPV6_GROUPS - 1];
        size_t count;
        valid = inet_pton(AF_INET6, s, &addr) == 1 ||
                (ParseIPv6Groups(s, s + strlen(s), groups, IPV6_GROUPS - 1, &count) &&
                 count > 0);
    }

    if (!valid)
    {
        Log(LOG_LEVEL_ERR, "Invalid IP address, range or subnet '%s'", s);
    }
    return valid;
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

/**
 * Checks whether refhost is prefix followed by a number within range,
 * e.g. FuzzyHostMatch("host", "1-10", "HOST5") == 0.
 * The comparison of the prefix is case-insensitive.
 *
 * Returns 0 on match.
 */
int FuzzyHostMatch(const char *prefix, const char *range, const char *refhost)
{
    // Split refhost into base name and trailing number, e.g. "host12"
    const size_t len = strlen(refhost);
    size_t base_len = len;
    while (base_len > 0 && isdigit((unsigned char) refhost[base_len - 1]))
    {
        base_len--;
    }

    if (base_len == 0 || base_len == len)
    {
        return 1; // No base name, or no number
    }

    unsigned long number;
    if (!ParseNumber(refhost + base_len, refhost + len, 10, LONG_MAX, &number))
    {
        return 1; // Number too large
    }

    long start = -1, end = -1;
    sscanf(range, "%ld-%ld", &start, &end);
    if ((long) number < start || (long) number > end)
    {
        return 1;
    }

    if (strlen(prefix) != base_len ||
        !StringEqualN_IgnoreCase(prefix, refhost, base_len))
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
 * @return the type of hostname: ADDRESS_TYPE_IPV4 if it is a valid IPv4
 *         address, ADDRESS_TYPE_IPV6 if it looks like an IPv6 address,
 *         otherwise (hostname, invalid or too long) ADDRESS_TYPE_OTHER
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
            // "1.2.3.4:80", "cfengine.com:80", "localhost:80" or "localhost:"
            if ((first_dot != NULL && first_dot < first_colon) ||
                strchr(first_colon + 1, ':') == NULL)
            {
                *first_colon = '\0';
                port_str = first_colon + 1;
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

    struct in_addr addr;
    return (inet_pton(AF_INET, *hostname, &addr) == 1)
        ? ADDRESS_TYPE_IPV4
        : ADDRESS_TYPE_OTHER;
}
