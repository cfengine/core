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

#include "files_interfaces.h"

#include "env_context.h"
#include "promises.h"
#include "dir.h"
#include "files_names.h"
#include "files_hashes.h"
#include "files_copy.h"
#include "item_lib.h"
#include "vars.h"
#include "matching.h"
#include "cfstream.h"
#include "client_code.h"
#include "logging.h"
#include "string_lib.h"
#include "rlist.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

int cfstat(const char *path, struct stat *buf)
{
#ifdef __MINGW32__
    return NovaWin_stat(path, buf);
#else
    return stat(path, buf);
#endif
}

/*********************************************************************/

int cf_lstat(EvalContext *ctx, char *file, struct stat *buf, Attributes attr, Promise *pp)
{
    int res;

    if ((attr.copy.servers == NULL) || (strcmp(attr.copy.servers->item, "localhost") == 0))
    {
        res = lstat(file, buf);
        CheckForFileHoles(buf, pp);
        return res;
    }
    else
    {
        return cf_remote_stat(ctx, file, buf, "link", attr, pp);
    }
}

/*********************************************************************/


/*********************************************************************/

ssize_t CfReadLine(char *buff, size_t size, FILE *fp)
{
    buff[0] = '\0';
    buff[size - 1] = '\0';      /* mark end of buffer */

    //error checking
    if (!fp || ferror(fp))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! NULL or corrupt inputs to CfReadLine");
        return -1;
    }

    if (fgets(buff, size, fp) == NULL)
    {
        *buff = '\0';           /* EOF */
        return 0;
    }
    else
    {
        char *tmp;

        if ((tmp = strrchr(buff, '\n')) != NULL)
        {
            /* remove newline */
            *tmp = '\0';
            return tmp - buff;
        }
        else
        {
            /* The line was too long and truncated so, discard probable remainder */
            while (true)
            {
                if (feof(fp))
                {
                    break;
                }

                char ch = fgetc(fp);

                if (ch == '\n')
                {
                    break;
                }
            }

            return size - 1;
        }
    }
}

/*******************************************************************/

