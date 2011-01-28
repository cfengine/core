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

/*****************************************************************************/
/*                                                                           */
/* File: dbm_tokyocab.c                                                      */
/*                                                                           */
/*****************************************************************************/

/*
 * Implementation of the Cfengine DBM API using Tokyo Cabinet hash API.
 */

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifdef TCDB

/*****************************************************************************/

int TCDB_OpenDB(char *filename, CF_TCDB **hdbp)

{ int errCode;

ThreadLock(cft_system);
*hdbp = malloc(sizeof(CF_TCDB));
ThreadUnlock(cft_system);

if (*hdbp == NULL)
   {
   FatalError("Memory allocation in TCDB_OpenDB(");
   }

(*hdbp)->hdb = tchdbnew();

if (!tchdbopen((*hdbp)->hdb, filename, HDBOWRITER | HDBOCREAT))
   {
   errCode = tchdbecode((*hdbp)->hdb);
   CfOut(cf_error, "", "!! tchdbopen: Opening database \"%s\" failed: %s", filename, tchdberrmsg(errCode));

   ThreadLock(cft_system);
   free(*hdbp);
   *hdbp = NULL;
   ThreadUnlock(cft_system);
   return false;
   }

(*hdbp)->valmemp = NULL;
return true;
}

/*****************************************************************************/

int TCDB_CloseDB(CF_TCDB *hdbp)

{ int errCode;

char buf[CF_MAXVARSIZE];

snprintf(buf, sizeof(buf), "CloseDB(%s)\n", tchdbpath(hdbp->hdb));
Debug(buf);

if (!tchdbclose(hdbp->hdb))
   {
   errCode = tchdbecode(hdbp->hdb);
   CfOut(cf_error, "", "!! tchdbclose: Closing database failed: %s", tchdberrmsg(errCode));
   return false;
   }

tchdbdel(hdbp->hdb);

ThreadLock(cft_system);

if (hdbp->valmemp != NULL)
   {
   free(hdbp->valmemp);
   }

free(hdbp);
hdbp = NULL;

ThreadUnlock(cft_system);

return true;
}

/*****************************************************************************/

int TCDB_ValueSizeDB(CF_TCDB *hdbp, char *key)

{
return tchdbvsiz2(hdbp->hdb, key);
}

/*****************************************************************************/

int TCDB_ReadComplexKeyDB(CF_TCDB *hdbp, char *key, int keySz,void *dest, int destSz)

{ int errCode;

if (tchdbget3(hdbp->hdb, key, keySz, dest, destSz) == -1)
   {
   errCode = tchdbecode(hdbp->hdb);
   Debug("TCDB_ReadComplexKeyDB(%s): Could not read: %s\n", key, tchdberrmsg(errCode));
   return false;
   }

return true;
}

/*****************************************************************************/

int TCDB_RevealDB(CF_TCDB *hdbp, char *key, void **result, int *rsize)

{ int errCode;

ThreadLock(cft_system);

if (hdbp->valmemp != NULL)
   {
   free(hdbp->valmemp);
   hdbp->valmemp = NULL;
   }

ThreadUnlock(cft_system);

*result = tchdbget(hdbp->hdb, key, strlen(key), rsize);

if (*result == NULL)
   {
   errCode = tchdbecode(hdbp->hdb);
   Debug("TCDB_RevealDB(%s): Could not read: %s\n", key, tchdberrmsg(errCode));
   return false;
   }

hdbp->valmemp = *result;  // keep allocated address for later free

return true;
}

/*****************************************************************************/

int TCDB_WriteComplexKeyDB(CF_TCDB *hdbp, char *key, int keySz, void *src, int srcSz)

{ int errCode;
  int res;

res = tchdbput(hdbp->hdb, key, keySz, src, srcSz);

if (!res)
   {
   errCode = tchdbecode(hdbp->hdb);
   CfOut(cf_error, "", "!! tchdbput: Could not write key to DB \"%s\": %s",
	 tchdbpath(hdbp->hdb), tchdberrmsg(errCode));
   return false;
   }

return true;
}

/*****************************************************************************/

int TCDB_DeleteComplexKeyDB(CF_TCDB *hdbp, char *key, int size)

{ int errCode;

if (!tchdbout(hdbp->hdb, key, size))
   {
   errCode = tchdbecode(hdbp->hdb);
   Debug("TCDB_DeleteComplexKeyDB(%s): Could not delete key: %s\n", key, tchdberrmsg(errCode));
   return false;
   }

return true;
}

/*****************************************************************************/

int TCDB_NewDBCursor(CF_TCDB *hdbp,CF_TCDBC **hdbcp)

{ int errCode;

if (!tchdbiterinit(hdbp->hdb))
   {
   errCode = tchdbecode(hdbp->hdb);
   CfOut(cf_error, "", "!! tchdbiterinit: Could not initialize iterator: %s", tchdberrmsg(errCode));
   return false;
   }

ThreadLock(cft_system);

*hdbcp = malloc(sizeof(CF_TCDBC));

ThreadUnlock(cft_system);

if (*hdbcp == NULL)
   {
   FatalError("Memory allocation in TCDB_NewDBCursor()");
   }

(*hdbcp)->curkey = NULL;
(*hdbcp)->curval = NULL;

return true;  
}

/*****************************************************************************/

int TCDB_NextDB(CF_TCDB *hdbp,CF_TCDBC *hdbcp,char **key,int *ksize,void **value,int *vsize)

{ int errCode;

ThreadLock(cft_system);

if (hdbcp->curkey != NULL)
   {
   free(hdbcp->curkey);
   hdbcp->curkey = NULL;
   }

if(hdbcp->curval != NULL)
   {
   free(hdbcp->curval);
   hdbcp->curval = NULL;
   }

ThreadUnlock(cft_system);

*key = tchdbiternext(hdbp->hdb, ksize);

if (*key == NULL)
   {
   Debug("Got NULL-key in TCDB_NextDB()\n");
   return false;
   }

*value = tchdbget(hdbp->hdb, *key, *ksize, vsize);

if (*value == NULL)
   {
   ThreadLock(cft_system);
   free(*key);
   ThreadUnlock(cft_system);
   
   *key = NULL;
   errCode = tchdbecode(hdbp->hdb);
   CfOut(cf_error, "", "!! tchdbget: Could not get value corrsponding to key \"%s\": %s", key, tchdberrmsg(errCode));
   return false;
   }

// keep pointers for later free
hdbcp->curkey = *key;
hdbcp->curval = *value;

return true;
}

/*****************************************************************************/

int TCDB_DeleteDBCursor(CF_TCDB *hdbp,CF_TCDBC *hdbcp)

{
ThreadLock(cft_system);

if (hdbcp->curkey != NULL)
   {
   free(hdbcp->curkey);
   hdbcp->curkey = NULL;
   }

if (hdbcp->curval != NULL)
   {
   free(hdbcp->curval);
   hdbcp->curval = NULL;
   }

free(hdbcp);

ThreadUnlock(cft_system);

return true;
}

#endif
