#include "cf3.defs.h"
#include "cf3.extern.h"

char ToLower (char ch)

{
if (isupper((int)ch))
   {
   return(ch - 'A' + 'a');
   }
else
   {
   return(ch);
   }
}


/*********************************************************************/

char ToUpper (char ch)

{
if (isdigit((int)ch) || ispunct((int)ch))
   {
   return(ch);
   }

if (isupper((int)ch))
   {
   return(ch);
   }
else
   {
   return(ch - 'a' + 'A');
   }
}

/*********************************************************************/

void ToUpperStrInplace(char *str)
{
for (; *str != '\0'; str++)
   {
   *str = ToUpper(*str);
   }
}

/*********************************************************************/

char *ToUpperStr(const char *str)
{
static char buffer[CF_BUFSIZE];

if (strlen(str) >= CF_BUFSIZE)
   {
   FatalError("String too long in ToUpperStr: %s", str);
   }

strlcpy(buffer, str, CF_BUFSIZE);
ToUpperStrInplace(buffer);

return buffer;
}

/*********************************************************************/

void ToLowerStrInplace(char *str)
{
for (; *str != '\0'; str++)
   {
   *str = ToLower(*str);
   }
}

/*********************************************************************/

char *ToLowerStr(const char *str)

{
static char buffer[CF_BUFSIZE];

if (strlen(str) >= CF_BUFSIZE-1)
   {
   FatalError("String too long in ToLowerStr: %s", str);
   }

strlcpy(buffer, str, CF_BUFSIZE);

ToLowerStrInplace(buffer);

return buffer;
}
