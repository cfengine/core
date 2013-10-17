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
versions of CFEngine, the applicable Commerical Open Source License
(COSL) may apply to this file if you as a licensee so wish it. See
included file COSL.txt.
*/

# include "rpl_utsname.h"
# include "cf3.defs.h"
# include "cf3.extern.h"
# include "platform.h"


uts_nodename_t vsysname_nodename = { .initialised = 0, .nodename = {0} };

const char *get_utsname_nodename()
{
    if (vsysname_nodename.initialised == 0)
    {
        return "";
    }
    else
    {
        return vsysname_nodename.nodename;
    }
}

int init_utsname_nodename()
{
    if (vsysname_nodename.initialised == 0)
    {  
        char dnsname[RPL_UTSNAME_BUFSIZE] = "";
        char fqn[RPL_UTSNAME_BUFSIZE];
            
# if defined (__hpux)
        if (gethostname(fqn, sizeof(fqn)) != -1)
        {
            struct hostent *hp;

            if ((hp = gethostbyname(fqn)))
            {
                strncpy(dnsname, hp->h_name, RPL_UTSNAME_BUFSIZE);

                int hnlen = (int)((strchr(dnsname, '.')) - dnsname) + 1;
    
                strlcpy(vsysname_nodename.nodename, dnsname, hnlen);    
            }
            else
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }
# else     
        strlcpy(vsysname_nodename.nodename, VSYSNAME.nodename, sizeof(VSYSNAME.nodename));
# endif  
        vsysname_nodename.initialised = 1;
    }
    return 0;
}
