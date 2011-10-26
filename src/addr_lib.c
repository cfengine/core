#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************************/

int FuzzySetMatch(char *s1,char *s2)

/* Match two IP strings - with : or . in hex or decimal
   s1 is the test string, and s2 is the reference e.g.
   FuzzySetMatch("128.39.74.10/23","128.39.75.56") == 0

   Returns 0 on match. */

{ short isCIDR = false, isrange = false, isv6 = false, isv4 = false;
  char address[CF_ADDRSIZE];
  int mask;
  unsigned long a1,a2;

if (strcmp(s1,s2) == 0)
   {
   return 0;
   }
  
if (strstr(s1,"/") != 0)
   {
   isCIDR = true;
   }

if (strstr(s1,"-") != 0)
   {
   isrange = true;
   }

if (strstr(s1,".") != 0)
   {
   isv4 = true;
   }

if (strstr(s1,":") != 0)
   {
   isv6 = true;
   }

if (strstr(s2,".") != 0)
   {
   isv4 = true;
   }

if (strstr(s2,":") != 0)
   {
   isv6 = true;
   }

if (isv4 && isv6)
   {
   /* This is just wrong */
   return -1;
   }

if (isCIDR && isrange)
   {
   CfOut(cf_error,"","Cannot mix CIDR notation with xxx-yyy range notation: %s",s1);
   return -1;
   }

if (!(isv6 || isv4))
   {
   CfOut(cf_error,"","Not a valid address range - or not a fully qualified name: %s",s1);
   return -1;
   }

if (!(isrange||isCIDR)) 
   {
   if (strlen(s2) > strlen(s1))
      {
      if (*(s2+strlen(s1)) != '.')
         {
         return -1; // Because xxx.1 should not match xxx.12 in the same octet
         }
      }
   
   return strncmp(s1,s2,strlen(s1)); /* do partial string match */
   }
 
if (isv4)
   {
   if (isCIDR)
      {
      struct sockaddr_in addr1,addr2;
      int shift;

      address[0] = '\0';
      mask = 0;
      sscanf(s1,"%16[^/]/%d",address,&mask);
      shift = 32 - mask;

      sockaddr_pton(AF_INET, address, &addr1);
      sockaddr_pton(AF_INET, s2, &addr2);

      a1 = htonl(addr1.sin_addr.s_addr);
      a2 = htonl(addr2.sin_addr.s_addr);
      
      a1 = a1 >> shift;
      a2 = a2 >> shift;
      
      if (a1 == a2)
         {
         return 0;
         }
      else
         {
         return -1;
         }
      }
   else
      {
      long i, from = -1, to = -1, cmp = -1;
      char *sp1,*sp2,buffer1[CF_MAX_IP_LEN],buffer2[CF_MAX_IP_LEN];

      sp1 = s1;
      sp2 = s2;
      
      for (i = 0; i < 4; i++)
         {
         buffer1[0] = '\0';
         sscanf(sp1,"%[^.]",buffer1);
         
         if (strlen(buffer1) == 0)
            {
            break;
            }

         sp1 += strlen(buffer1)+1;
         sscanf(sp2,"%[^.]",buffer2);
         sp2 += strlen(buffer2)+1;
         
         if (strstr(buffer1,"-"))
            {
            sscanf(buffer1,"%ld-%ld",&from,&to);
            sscanf(buffer2,"%ld",&cmp);

            if (from < 0 || to < 0)
               {
               CfDebug("Couldn't read range\n");
               return -1;
               }
            
            if ((from > cmp) || (cmp > to))
               {
               CfDebug("Out of range %ld > %ld > %ld (range %s)\n",from,cmp,to,buffer2);
               return -1;
               }
            }
         else
            {
            sscanf(buffer1,"%ld",&from);
            sscanf(buffer2,"%ld",&cmp);
            
            if (from != cmp)
               {
               CfDebug("Unequal\n");
               return -1;
               }
            }
         
         CfDebug("Matched octet %s with %s\n",buffer1,buffer2);
         }
      
      CfDebug("Matched IP range\n");
      return 0;
      }
   }

#if defined(HAVE_GETADDRINFO)
if (isv6)
   {
   int i;

   if (isCIDR)
      {
      int blocks;
      struct sockaddr_in6 addr1,addr2;

      address[0] = '\0';
      mask = 0;
      sscanf(s1,"%40[^/]/%d",address,&mask);
      blocks = mask/8;

      if (mask % 8 != 0)
         {
         CfOut(cf_error,"","Cannot handle ipv6 masks which are not 8 bit multiples (fix me)");
         return -1;
         }

      sockaddr_pton(AF_INET6, address, &addr1);
      sockaddr_pton(AF_INET6, s2, &addr2);

      for (i = 0; i < blocks; i++) /* blocks < 16 */
         {
         if (addr1.sin6_addr.s6_addr[i] != addr2.sin6_addr.s6_addr[i])
            {
            return -1;
            }
         }
      return 0;
      }
   else
      {
      long i, from = -1, to = -1, cmp = -1;
      char *sp1,*sp2,buffer1[CF_MAX_IP_LEN],buffer2[CF_MAX_IP_LEN];

      sp1 = s1;
      sp2 = s2;
      
      for (i = 0; i < 8; i++)
         {
         sscanf(sp1,"%[^:]",buffer1);
         sp1 += strlen(buffer1)+1;
         sscanf(sp2,"%[^:]",buffer2);
         sp2 += strlen(buffer2)+1;
         
         if (strstr(buffer1,"-"))
            {
            sscanf(buffer1,"%lx-%lx",&from,&to);
            sscanf(buffer2,"%lx",&cmp);
            
            if (from < 0 || to < 0)
               {
               return -1;
               }
            
            if ((from >= cmp) || (cmp > to))
               {
               CfDebug("%lx < %lx < %lx\n",from,cmp,to);
               return -1;
               }
            }
         else
            {
            sscanf(buffer1,"%ld",&from);
            sscanf(buffer2,"%ld",&cmp);
            
            if (from != cmp)
               {
               return -1;
               }
            }
         }
      
      return 0;
      }
   }
#endif 

return -1; 
}

/*********************************************************************/

int FuzzyHostParse(char *arg1,char *arg2)

{
  long start = -1, end = -1, where = -1;
  int n;

n = sscanf(arg2,"%ld-%ld%n",&start,&end,&where);

if (n != 2)
   {
   CfOut(cf_error,"","HostRange syntax error: second arg should have X-Y format where X and Y are decimal numbers");
   return false;
   } 

return true; 
}

/*********************************************************************/

int FuzzyHostMatch(char *arg0, char* arg1, char *refhost)

{
  char *sp, refbase[CF_MAXVARSIZE];
  long cmp = -1, start = -1, end = -1;
  char buf1[CF_BUFSIZE], buf2[CF_BUFSIZE];

strlcpy(refbase,refhost,CF_MAXVARSIZE);
sp = refbase + strlen(refbase) - 1;

while ( isdigit((int)*sp) )
   {
   sp--;
   }

sp++;
sscanf(sp,"%ld",&cmp);
*sp = '\0';

if (cmp < 0)
   {
   return 1;
   }

if (strlen(refbase) == 0)
   {
   return 1;
   }

sscanf(arg1,"%ld-%ld",&start,&end);

if ( cmp < start || cmp > end )
   {
   return 1;
   }

strncpy(buf1,ToLowerStr(refbase),CF_BUFSIZE-1);
strncpy(buf2,ToLowerStr(arg0),CF_BUFSIZE-1);

if (strcmp(buf1,buf2) != 0)
   {
   return 1;
   }

return 0;
}
