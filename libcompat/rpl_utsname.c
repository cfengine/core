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
# include "cf3.extern.h"
# include "platform.h"


const char *get_utsname_nodename()
{
    static bool initialised = false;
    static char vsysname_nodename[CF_BUFSIZE];
  
  
    if (!initialised)
    {
        if (sizeof(VSYSNAME.nodename) == 9)
        {
            char dnsname[CF_BUFSIZE] = "";
            char fqn[CF_BUFSIZE];
            char nodename[CF_BUFSIZE];

            if (gethostname(fqn, sizeof(fqn)) != -1)
            {
                struct hostent *hp;

                if ((hp = gethostbyname(fqn)))
                {
                    strncpy(dnsname, hp->h_name, CF_MAXVARSIZE);
  
                    int hnlen = (int)((strchr(dnsname, '.')) - dnsname) + 1;
    
                    if (strlen(nodename) < hnlen && (strstr(dnsname, nodename)))
                    {
                        strlcpy(vsysname_nodename, dnsname, hnlen);
                    }  
                }
            }
        }
        else
        {
            strlcpy(vsysname_nodename, VSYSNAME.nodename, sizeof(VSYSNAME.nodename));
        }
        initialised = true;
    }
  
    return vsysname_nodename;
}


            