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


# define BUFSIZE 257

const char *get_utsname_nodename()
{
    static int initialised = 0;
    static char vsysname_nodename[BUFSIZE];
  
  
    if (initialised == 0)
    {  
        char dnsname[BUFSIZE] = "";
        char fqn[BUFSIZE];
        char nodename[BUFSIZE];
	    
# if defined (__hpux)
        if (gethostname(fqn, sizeof(fqn)) != -1)
        {
            struct hostent *hp;

            if ((hp = gethostbyname(fqn)))
            {
                strncpy(dnsname, hp->h_name, BUFSIZE);
  
                int hnlen = (int)((strchr(dnsname, '.')) - dnsname) + 1;
    
                if (strlen(nodename) < hnlen && (strstr(dnsname, nodename)))
                {
                    strlcpy(vsysname_nodename, dnsname, hnlen);
                }  
            }
        }
# else     
        strlcpy(vsysname_nodename, VSYSNAME.nodename, sizeof(VSYSNAME.nodename));
# endif  
        initialised = 1;
    }
    return vsysname_nodename;
}
