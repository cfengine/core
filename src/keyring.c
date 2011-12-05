/*****************************************************************************/
/*                                                                           */
/* File: keyring.c                                                           */
/*                                                                           */
/* Created: Sun May 29 13:35:34 2011                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/***************************************************************/

int HostKeyAddressUnknown(char *value)

{
if (strcmp(value,CF_UNKNOWN_IP) == 0)
   {
   return true;
   }

// Is there some other non-ip string left over?

if (!(strchr(value,'.') || strchr(value,':')))
   {
   return false;
   }

return false;
}
