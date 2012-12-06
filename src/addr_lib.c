/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "addr_lib.h"

#include "cfstream.h"
#include "string_lib.h"

#define CF_ADDRSIZE 128

/* Match two IP strings - with : or . in hex or decimal
   s1 is the test string, and s2 is the reference e.g.
   FuzzySetMatch("128.39.74.10/23","128.39.75.56") == 0

   Returns 0 on match. */

int FuzzySetMatch(const char *s1, const char *s2)
{
    short isCIDR = false, isrange = false, isv6 = false, isv4 = false;
    char address[CF_ADDRSIZE];
    int mask;
    unsigned long a1, a2;

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
        CfOut(cf_error, "", "Cannot mix CIDR notation with xxx-yyy range notation: %s", s1);
        return -1;
    }

    if (!(isv6 || isv4))
    {
        CfOut(cf_error, "", "Not a valid address range - or not a fully qualified name: %s", s1);
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
            int shift;

            address[0] = '\0';
            mask = 0;
            sscanf(s1, "%16[^/]/%d", address, &mask);
            shift = 32 - mask;

            sockaddr_pton(AF_INET, address, &addr1);
            sockaddr_pton(AF_INET, s2, &addr2);

            a1 = htonl(addr1.sin_addr.s_addr);
            a2 = htonl(addr2.sin_addr.s_addr);

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
            char buffer1[CF_MAX_IP_LEN], buffer2[CF_MAX_IP_LEN];

            const char *sp1 = s1;
            const char *sp2 = s2;

            for (i = 0; i < 4; i++)
            {
                buffer1[0] = '\0';
                sscanf(sp1, "%[^.]", buffer1);

                if (strlen(buffer1) == 0)
                {
                    break;
                }

                sp1 += strlen(buffer1) + 1;
                sscanf(sp2, "%[^.]", buffer2);
                sp2 += strlen(buffer2) + 1;

                if (strstr(buffer1, "-"))
                {
                    sscanf(buffer1, "%ld-%ld", &from, &to);
                    sscanf(buffer2, "%ld", &cmp);

                    if ((from < 0) || (to < 0))
                    {
                        CfDebug("Couldn't read range\n");
                        return -1;
                    }

                    if ((from > cmp) || (cmp > to))
                    {
                        CfDebug("Out of range %ld > %ld > %ld (range %s)\n", from, cmp, to, buffer2);
                        return -1;
                    }
                }
                else
                {
                    sscanf(buffer1, "%ld", &from);
                    sscanf(buffer2, "%ld", &cmp);

                    if (from != cmp)
                    {
                        CfDebug("Unequal\n");
                        return -1;
                    }
                }

                CfDebug("Matched octet %s with %s\n", buffer1, buffer2);
            }

            CfDebug("Matched IP range\n");
            return 0;
        }
    }

#if defined(HAVE_GETADDRINFO)
    if (isv6)
    {
        int i;

        if (isCIDR)
        {
            int blocks;
            struct sockaddr_in6 addr1, addr2;

            address[0] = '\0';
            mask = 0;
            sscanf(s1, "%40[^/]/%d", address, &mask);
            blocks = mask / 8;

            if (mask % 8 != 0)
            {
                CfOut(cf_error, "", "Cannot handle ipv6 masks which are not 8 bit multiples (fix me)");
                return -1;
            }

            sockaddr_pton(AF_INET6, address, &addr1);
            sockaddr_pton(AF_INET6, s2, &addr2);

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
            char buffer1[CF_MAX_IP_LEN], buffer2[CF_MAX_IP_LEN];

            const char *sp1 = s1;
            const char *sp2 = s2;

            for (i = 0; i < 8; i++)
            {
                sscanf(sp1, "%[^:]", buffer1);
                sp1 += strlen(buffer1) + 1;
                sscanf(sp2, "%[^:]", buffer2);
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
                        CfDebug("%lx < %lx < %lx\n", from, cmp, to);
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
#endif

    return -1;
}

int FuzzyHostParse(char *arg1, char *arg2)
{
    long start = -1, end = -1;
    int n;

    n = sscanf(arg2, "%ld-%ld", &start, &end);

    if (n != 2)
    {
        CfOut(cf_error, "",
              "HostRange syntax error: second arg should have X-Y format where X and Y are decimal numbers");
        return false;
    }

    return true;
}

int FuzzyHostMatch(char *arg0, char *arg1, char *refhost)
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

int FuzzyMatchParse(char *s)
{
    char *sp;
    short isCIDR = false, isrange = false, isv6 = false, isv4 = false, isADDR = false;
    char address[CF_ADDRSIZE];
    int mask, count = 0;

    CfDebug("Check ParsingIPRange(%s)\n", s);

    for (sp = s; *sp != '\0'; sp++)     /* Is this an address or hostname */
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
        CfOut(cf_error, "", "Mixture of IPv6 and IPv4 addresses");
        return false;
    }

    if (isCIDR && isrange)
    {
        CfOut(cf_error, "", "Cannot mix CIDR notation with xx-yy range notation");
        return false;
    }

    if (isv4 && isCIDR)
    {
        if (strlen(s) > 4 + 3 * 4 + 1 + 2)      /* xxx.yyy.zzz.mmm/cc */
        {
            CfOut(cf_error, "", "IPv4 address looks too long");
            return false;
        }

        address[0] = '\0';
        mask = 0;
        sscanf(s, "%16[^/]/%d", address, &mask);

        if (mask < 8)
        {
            CfOut(cf_error, "", "Mask value %d in %s is less than 8", mask, s);
            return false;
        }

        if (mask > 30)
        {
            CfOut(cf_error, "", "Mask value %d in %s is silly (> 30)", mask, s);
            return false;
        }
    }

    if (isv4 && isrange)
    {
        long i, from = -1, to = -1;
        char *sp1, buffer1[CF_MAX_IP_LEN];

        sp1 = s;

        for (i = 0; i < 4; i++)
        {
            buffer1[0] = '\0';
            sscanf(sp1, "%[^.]", buffer1);
            sp1 += strlen(buffer1) + 1;

            if (strstr(buffer1, "-"))
            {
                sscanf(buffer1, "%ld-%ld", &from, &to);

                if ((from < 0) || (to < 0))
                {
                    CfOut(cf_error, "", "Error in IP range - looks like address, or bad hostname");
                    return false;
                }

                if (to < from)
                {
                    CfOut(cf_error, "", "Bad IP range");
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
            CfOut(cf_error, "", "IPv6 address looks too short");
            return false;
        }

        if (strlen(s) > 42)
        {
            CfOut(cf_error, "", "IPv6 address looks too long");
            return false;
        }

        address[0] = '\0';
        mask = 0;
        sscanf(s, "%40[^/]/%d", address, &mask);

        if (mask % 8 != 0)
        {
            CfOut(cf_error, "", "Cannot handle ipv6 masks which are not 8 bit multiples (fix me)");
            return false;
        }

        if (mask > 15)
        {
            CfOut(cf_error, "", "IPv6 CIDR mask is too large");
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
