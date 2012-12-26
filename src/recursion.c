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

#include "cf3.defs.h"

#include "dir.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_operators.h"
#include "matching.h"
#include "cfstream.h"
#include "signals.h"
#include "logging.h"
#include "string_lib.h"


/*********************************************************************/
/* Depth searches                                                    */
/*********************************************************************/


/*******************************************************************/
/* Level                                                           */
/*******************************************************************/


/**********************************************************************/

int SkipDirLinks(char *path, const char *lastnode, Recursion r)
{
    CfDebug("SkipDirLinks(%s,%s)\n", path, lastnode);

    if (r.exclude_dirs)
    {
        if ((MatchRlistItem(r.exclude_dirs, path)) || (MatchRlistItem(r.exclude_dirs, lastnode)))
        {
            CfOut(cf_verbose, "", "Skipping matched excluded directory %s\n", path);
            return true;
        }
    }

    if (r.include_dirs)
    {
        if (!((MatchRlistItem(r.include_dirs, path)) || (MatchRlistItem(r.include_dirs, lastnode))))
        {
            CfOut(cf_verbose, "", "Skipping matched non-included directory %s\n", path);
            return true;
        }
    }

    return false;
}
