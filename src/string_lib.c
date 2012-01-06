#include "cf3.defs.h"
#include "cf3.extern.h"
#include "writer.h"

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

/*******************************************************************/

int StripListSep(char *strList, char *outBuf, int outBufSz)
{
  memset(outBuf,0,outBufSz);

  if(EMPTY(strList))
    {
    return false;
    }

  if(strList[0] != '{')
    {
    return false;
    }

  snprintf(outBuf,outBufSz,"%s",strList + 1);

  if(outBuf[strlen(outBuf) - 1] == '}')
    {
    outBuf[strlen(outBuf) - 1] = '\0';
    }

  return true;
}

/*******************************************************************/

int GetStringListElement(char *strList, int index, char *outBuf, int outBufSz)

/** Takes a string-parsed list "{'el1','el2','el3',..}" and writes
 ** "el1" or "el2" etc. based on index (starting on 0) in outBuf.
 ** returns true on success, false otherwise.
 **/

{ char *sp,*elStart = strList,*elEnd;
  int elNum = 0;
  int minBuf;

memset(outBuf,0,outBufSz);

if (EMPTY(strList))
   {
   return false;
   }

if(strList[0] != '{')
   {
   return false;
   }

for(sp = strList; *sp != '\0'; sp++)
   {
   if((sp[0] == '{' || sp[0] == ',') && sp[1] == '\'')
      {
      elStart = sp + 2;
      }

   else if((sp[0] == '\'') && (sp[1] == ',' || sp[1] == '}'))
      {
      elEnd = sp;

      if(elNum == index)
         {
         if(elEnd - elStart < outBufSz)
            {
            minBuf = elEnd - elStart;
            }
         else
            {
            minBuf = outBufSz - 1;
            }

         strncpy(outBuf,elStart,minBuf);

         break;
         }

      elNum++;
      }
   }

return true;
}

/*********************************************************************/

char* SearchAndReplace(const char *source, const char *search, const char *replace)

{
Writer *w = StringWriter();
const char *source_ptr = source;

if (source == NULL || search == NULL || replace == NULL)
   {
   FatalError("Programming error: NULL argument is passed to SearchAndReplace");
   }

if (strcmp(search, "") == 0)
   {
   return xstrdup(source);
   }

for (;;)
   {
   const char *found_ptr = strstr(source_ptr, search);
   if (found_ptr == NULL)
      {
      WriterWrite(w, source_ptr);
      return StringWriterClose(w);
      }

   WriterWriteLen(w, source_ptr, found_ptr - source_ptr);
   WriterWrite(w, replace);

   source_ptr += found_ptr - source_ptr + strlen(search);
   }
}
