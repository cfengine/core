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


#include "../src/conf.h"
#include "../src/cf.defs.h"
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#ifndef STDC_HEADERS
extern int errno;
#endif

/* Write LEN bytes at PTR to descriptor DESC, retrying if interrupted.
   Return LEN upon success, write's (negative) error code otherwise.  */

int cf_full_write (desc,ptr,len)

int desc;
char *ptr;
size_t len;

{ int total_written;

total_written = 0;

while (len > 0)
   {
   int written = write(desc,ptr,len);
   
   if (written < 0)
      {
      if (errno == EINTR)
         {
         continue;
         }
      
      return written;
      }
   
   total_written += written;
   ptr += written;
   len -= written;
   }

return total_written;
}
