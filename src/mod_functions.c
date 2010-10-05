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
/* File: mod_functions.c                                                     */
/*                                                                           */
/*****************************************************************************/

#define CF3_MOD_FUNCTIONS

#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************/
/* Function prototypes                                   */
/*********************************************************/

struct FnCallArg ACCESSEDBEFORE_ARGS[] =
   {
   {CF_PATHRANGE,cf_str,"Newer filename"},
   {CF_PATHRANGE,cf_str,"Older filename"},
   {NULL,cf_notype,NULL}
   };

struct FnCallArg ACCUM_ARGS[] =
   {
   {"0,1000",cf_int,"Years"},
   {"0,1000",cf_int,"Months"},    
   {"0,1000",cf_int,"Days"},      
   {"0,1000",cf_int,"Hours"},     
   {"0,1000",cf_int,"Minutes"},   
   {"0,40000",cf_int,"Seconds"},   
   {NULL,cf_notype,NULL}
   };

struct FnCallArg AGO_ARGS[] =
    {
    {"0,1000",cf_int,"Years"},
    {"0,1000",cf_int,"Months"},    
    {"0,1000",cf_int,"Days"},      
    {"0,1000",cf_int,"Hours"},     
    {"0,1000",cf_int,"Minutes"},   
    {"0,40000",cf_int,"Seconds"},   
    {NULL,cf_notype,NULL}
    };

struct FnCallArg LATERTHAN_ARGS[] =
    {
    {"0,1000",cf_int,"Years"},
    {"0,1000",cf_int,"Months"},    
    {"0,1000",cf_int,"Days"},      
    {"0,1000",cf_int,"Hours"},     
    {"0,1000",cf_int,"Minutes"},   
    {"0,40000",cf_int,"Seconds"},   
    {NULL,cf_notype,NULL}
    };

struct FnCallArg CANONIFY_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"String containing non-identifer characters"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg CHANGEDBEFORE_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"Newer filename"},
    {CF_PATHRANGE,cf_str,"Older filename"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg CLASSIFY_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Input string"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg CLASSMATCH_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg COUNTCLASSESMATCHING_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg COUNTLINESMATCHING_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_PATHRANGE,cf_str,"Filename"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg DISKFREE_ARGS[] =
   {
   {CF_PATHRANGE,cf_str,"File system directory"},
   {NULL,cf_notype,NULL}
   };

struct FnCallArg ESCAPE_ARGS[] =
   {
   {CF_ANYSTRING,cf_str,"IP address or string to escape"},
   {NULL,cf_notype,NULL}
   };

struct FnCallArg EXECRESULT_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"Fully qualified command path"},
    {"useshell,noshell",cf_opts,"Shell encapsulation option"},
    {NULL,cf_notype,NULL}
    };

// fileexists, isdir,isplain,islink

struct FnCallArg FILESTAT_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"File object name"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg FILESEXIST_ARGS[] =
    {
    {CF_NAKEDLRANGE,cf_str,"Array identifier containing list"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GETFIELDS_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression to match line"},
    {CF_PATHRANGE,cf_str,"Filename to read"},
    {CF_ANYSTRING,cf_str,"Regular expression to split fields"},
    {CF_ANYSTRING,cf_str,"Return array name"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GETINDICES_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Cfengine array identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GETUSERS_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Comma separated list of User names"},
    {CF_ANYSTRING,cf_str,"Comma separated list of UserID numbers"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GETENV_ARGS[] =
   {
   {CF_IDRANGE,cf_str,"Name of environment variable"},
   {CF_VALRANGE,cf_int,"Maximum number of characters to read "},
   {NULL,cf_notype,NULL}
   };

struct FnCallArg GETGID_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Group name in text"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GETUID_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"User name in text"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GREP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_IDRANGE,cf_str,"Cfengine array identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg GROUPEXISTS_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Group name or identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HASH_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Input text"},
    {"md5,sha1,sha256,sha512,sha384,crypt",cf_opts,"Hash or digest algorithm"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HASHMATCH_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"Filename to hash"},
    {"md5,sha1,crypt,cf_sha224,cf_sha256,cf_sha384,cf_sha512",cf_opts,"Hash or digest algorithm"},
    {CF_IDRANGE,cf_str,"ASCII representation of hash for comparison"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HOST2IP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Host name in ascii"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HOSTINNETGROUP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Host name"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HOSTRANGE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Hostname prefix"},
    {CF_ANYSTRING,cf_str,"Enumerated range"},
    {NULL,cf_notype,NULL}
    };


struct FnCallArg HOSTSSEEN_ARGS[] =
    {
    {CF_VALRANGE,cf_int,"Horizon since last seen in hours"},
    {"lastseen,notseen",cf_opts,"Complements for selection policy"},
    {"name,address",cf_opts,"Type of return value desired"},
    {NULL,cf_notype,NULL}
    };
    
struct FnCallArg IPRANGE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"IP address range syntax"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg IRANGE_ARGS[] =
    {
    {CF_INTRANGE,cf_int,"Integer"},
    {CF_INTRANGE,cf_int,"Integer"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg ISGREATERTHAN_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Larger string or value"},
    {CF_ANYSTRING,cf_str,"Smaller string or value"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg ISLESSTHAN_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Smaller string or value"},
    {CF_ANYSTRING,cf_str,"Larger string or value"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg ISNEWERTHAN_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"Newer file name"},
    {CF_PATHRANGE,cf_str,"Older file name"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg ISVARIABLE_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Variable identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg JOIN_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Join glue-string"},
    {CF_IDRANGE,cf_str,"Cfengine array identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg LASTNODE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Input string"},
    {CF_ANYSTRING,cf_str,"Link separator, e.g. /,:"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg LDAPARRAY_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"URI"},
    {CF_ANYSTRING,cf_str,"Distinguished name"},
    {CF_ANYSTRING,cf_str,"Filter"},
    {CF_ANYSTRING,cf_str,"Record name"},
    {"subtree,onelevel,base",cf_opts,"Search scope policy"},
    {"none,ssl,sasl",cf_opts,"Security level"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg LDAPLIST_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"URI"},
    {CF_ANYSTRING,cf_str,"Distinguished name"},
    {CF_ANYSTRING,cf_str,"Filter"},
    {CF_ANYSTRING,cf_str,"Record name"},
    {"subtree,onelevel,base",cf_opts,"Search scope policy"},
    {"none,ssl,sasl",cf_opts,"Security level"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg LDAPVALUE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"URI"},
    {CF_ANYSTRING,cf_str,"Distinguished name"},
    {CF_ANYSTRING,cf_str,"Filter"},
    {CF_ANYSTRING,cf_str,"Record name"},
    {"subtree,onelevel,base",cf_opts,"Search scope policy"},
    {"none,ssl,sasl",cf_opts,"Security level"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg NOW_ARGS[] =
    {
    {NULL,cf_notype,NULL}
    };

struct FnCallArg SUM_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"A list of arbitrary real values"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg PRODUCT_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"A list of arbitrary real values"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg DATE_ARGS[] =
    {
    {"1970,3000",cf_int,"Year"},
    {"1,12",cf_int,"Month"},    
    {"1,31",cf_int,"Day"},      
    {"0,23",cf_int,"Hour"},     
    {"0,59",cf_int,"Minute"},   
    {"0,59",cf_int,"Second"},   
    {NULL,cf_notype,NULL}
    };

struct FnCallArg PEERS_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"File name of host list"},
    {CF_ANYSTRING,cf_str,"Comment regex pattern"},
    {CF_VALRANGE,cf_int,"Peer group size"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg PEERLEADER_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"File name of host list"},
    {CF_ANYSTRING,cf_str,"Comment regex pattern"},
    {CF_VALRANGE,cf_int,"Peer group size"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg PEERLEADERS_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"File name of host list"},
    {CF_ANYSTRING,cf_str,"Comment regex pattern"},
    {CF_VALRANGE,cf_int,"Peer group size"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg RANDOMINT_ARGS[] =
    {
    {CF_INTRANGE,cf_int,"Lower inclusive bound"},
    {CF_INTRANGE,cf_int,"Upper inclusive bound"},
    {NULL,cf_notype,NULL}
    };
 
struct FnCallArg READFILE_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"File name"},
    {CF_VALRANGE,cf_int,"Maximum number of bytes to read"},
    {NULL,cf_notype,NULL}
    };


struct FnCallArg READSTRINGARRAY_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Array identifer to populate"},
    {CF_PATHRANGE,cf_str,"File name to read"},
    {CF_ANYSTRING,cf_str,"Regex matching comments"},
    {CF_ANYSTRING,cf_str,"Regex to split data"},
    {CF_VALRANGE,cf_int,"Maximum number of entries to read"},
    {CF_VALRANGE,cf_int,"Maximum bytes to read"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg READSTRINGARRAYIDX_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Array identifer to populate"},
    {CF_PATHRANGE,cf_str,"File name to read"},
    {CF_ANYSTRING,cf_str,"Regex matching comments"},
    {CF_ANYSTRING,cf_str,"Regex to split data"},
    {CF_VALRANGE,cf_int,"Maximum number of entries to read"},
    {CF_VALRANGE,cf_int,"Maximum bytes to read"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg READSTRINGLIST_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"File name to read"},
    {CF_ANYSTRING,cf_str,"Regex matching comments"},
    {CF_ANYSTRING,cf_str,"Regex to split data"},
    {CF_VALRANGE,cf_int,"Maximum number of entries to read"},
    {CF_VALRANGE,cf_int,"Maximum bytes to read"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg READTCP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Host name or IP address of server socket"},
    {CF_VALRANGE,cf_int,"Port number"},
    {CF_ANYSTRING,cf_str,"Protocol query string"},
    {CF_VALRANGE,cf_int,"Maximum number of bytes to read"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGARRAY_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Cfengine array identifier"},
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGCMP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_ANYSTRING,cf_str,"Match string"},
    {NULL,cf_notype,NULL}        
    };

struct FnCallArg REGEXTRACT_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_ANYSTRING,cf_str,"Match string"},
    {CF_IDRANGE,cf_str,"Identifier for back-references"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGISTRYVALUE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Windows registry key"},
    {CF_ANYSTRING,cf_str,"Windows registry value-id"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGLINE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_ANYSTRING,cf_str,"Filename to search"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGLIST_ARGS[] =
    {
    {CF_NAKEDLRANGE,cf_str,"Cfengine list identifier"},
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REGLDAP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"URI"},
    {CF_ANYSTRING,cf_str,"Distinguished name"},
    {CF_ANYSTRING,cf_str,"Filter"},
    {CF_ANYSTRING,cf_str,"Record name"},
    {"subtree,onelevel,base",cf_opts,"Search scope policy"},
    {CF_ANYSTRING,cf_str,"Regex to match results"},
    {"none,ssl,sasl",cf_opts,"Security level"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REMOTESCALAR_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Variable identifier"},
    {CF_ANYSTRING,cf_str,"Hostname or IP address of server"},
    {CF_BOOL,cf_opts,"Use enryption"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg HUB_KNOWLEDGE_ARGS[] =
    {
    {CF_IDRANGE,cf_str,"Variable identifier"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg REMOTECLASSESMATCHING_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Regular expression"},
    {CF_ANYSTRING,cf_str,"Server name or address"},
    {CF_BOOL,cf_opts,"Use encryption"},
    {CF_IDRANGE,cf_str,"Return class prefix"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg RETURNSZERO_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"Fully qualified command path"},
    {"useshell,noshell",cf_opts,"Shell encapsulation option"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg RRANGE_ARGS[] =
    {
    {CF_REALRANGE,cf_real,"Real number"},
    {CF_REALRANGE,cf_real,"Real number"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg SELECTSERVERS_ARGS[] =
    {
    {CF_NAKEDLRANGE,cf_str,"The identifier of a cfengine list of hosts or addresses to contact"},
    {CF_VALRANGE,cf_int,"The port number"},
    {CF_ANYSTRING,cf_str,"A query string"},
    {CF_ANYSTRING,cf_str,"A regular expression to match success"},     
    {CF_VALRANGE,cf_int,"Maximum number of bytes to read from server"},
    {CF_IDRANGE,cf_str,"Name for array of results"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg SPLAYCLASS_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Input string for classification"},
    {"daily,hourly",cf_opts,"Splay time policy"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg SPLITSTRING_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"A data string"},
    {CF_ANYSTRING,cf_str,"Regex to split on"},     
    {CF_VALRANGE,cf_int,"Maximum number of pieces"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg STRCMP_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"String"},
    {CF_ANYSTRING,cf_str,"String"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg TRANSLATEPATH_ARGS[] =
    {
    {CF_PATHRANGE,cf_str,"Unix style path"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg USEMODULE_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"Name of module command"},
    {CF_ANYSTRING,cf_str,"Argument string for the module"},
    {NULL,cf_notype,NULL}
    };

struct FnCallArg USEREXISTS_ARGS[] =
    {
    {CF_ANYSTRING,cf_str,"User name or identifier"},
    {NULL,cf_notype,NULL}
    };


/*********************************************************/
/* FnCalls are rvalues in certain promise constraints    */
/*********************************************************/

/* see cf3.defs.h enum fncalltype */

struct FnCallType CF_FNCALL_TYPES[] = 
   {
   {"accessedbefore",cf_class,2,ACCESSEDBEFORE_ARGS,"True if arg1 was accessed before arg2 (atime)"},
   {"accumulated",cf_int,6,ACCUM_ARGS,"Convert an accumulated amount of time into a system representation"},
   {"ago",cf_int,6,AGO_ARGS,"Convert a time relative to now to an integer system representation"},
   {"canonify",cf_str,1,CANONIFY_ARGS,"Convert an abitrary string into a legal class name"},
   {"changedbefore",cf_class,2,CHANGEDBEFORE_ARGS,"True if arg1 was changed before arg2 (ctime)"},
   {"classify",cf_class,1,CLASSIFY_ARGS,"True if the canonicalization of the argument is a currently defined class"},
   {"classmatch",cf_class,1,CLASSMATCH_ARGS,"True if the regular expression matches any currently defined class"},
   {"countclassesmatching",cf_int,1,COUNTCLASSESMATCHING_ARGS,"Count the number of defined classes matching regex arg1"},
   {"countlinesmatching",cf_int,2,COUNTLINESMATCHING_ARGS,"Count the number of lines matching regex arg1 in file arg2"},
   {"diskfree",cf_int,1,DISKFREE_ARGS,"Return the free space (in KB) available on the directory's current partition (0 if not found)"},
   {"escape",cf_str,1,ESCAPE_ARGS,"Escape regular expression characters in a string"},
   {"execresult",cf_str,2,EXECRESULT_ARGS,"Execute named command and assign output to variable"},
   {"fileexists",cf_class,1,FILESTAT_ARGS,"True if the named file can be accessed"},
   {"filesexist",cf_class,1,FILESEXIST_ARGS,"True if the named list of files can ALL be accessed"},
   {"getenv",cf_str,2,GETENV_ARGS,"Return the environment variable named arg1, truncated at arg2 characters"},
   {"getfields",cf_int,4,GETFIELDS_ARGS,"Get an array of fields in the lines matching regex arg1 in file arg2, split on regex arg3 as array name arg4"},
   {"getgid",cf_int,1,GETGID_ARGS,"Return the integer group id of the named group on this host"},
   {"getindices",cf_slist,1,GETINDICES_ARGS,"Get a list of keys to the array whose id is the argument and assign to variable"},
   {"getuid",cf_int,1,GETUID_ARGS,"Return the integer user id of the named user on this host"},
   {"getusers",cf_slist,2,GETUSERS_ARGS,"Get a list of all system users defined, minus those names defined in args 1 and uids in args"},
   {"grep",cf_str,2,GREP_ARGS,"Extract the sub-list if items matching the regular expression in arg1 of the list named in arg2"},
   {"groupexists",cf_class,1,GROUPEXISTS_ARGS,"True if group or numerical id exists on this host"},
   {"hash",cf_str,2,HASH_ARGS,"Return the hash of arg1, type arg2 and assign to a variable"},
   {"hashmatch",cf_class,3,HASHMATCH_ARGS,"Compute the hash of arg1, of type arg2 and test if it matches the value in arg 3"},
   {"host2ip",cf_str,1,HOST2IP_ARGS,"Returns the primary name-service IP address for the named host"},
   {"hostinnetgroup",cf_class,1,HOSTINNETGROUP_ARGS,"True if the current host is in the named netgroup"},
   {"hostrange",cf_class,2,HOSTRANGE_ARGS,"True if the current host lies in the range of enumerated hostnames specified"},
   {"hostsseen",cf_str,3,HOSTSSEEN_ARGS,"Extract the list of hosts last seen/not seen within the last arg1 hours"},
   {"hubknowledge",cf_str,1,HUB_KNOWLEDGE_ARGS,"Read global knowledge from the hub host by id (commercial extension)"},
   {"iprange",cf_class,1,IPRANGE_ARGS,"True if the current host lies in the range of IP addresses specified"},
   {"irange",cf_irange,2,IRANGE_ARGS,"Define a range of integer values for cfengine internal use"},
   {"isdir",cf_class,1,FILESTAT_ARGS,"True if the named object is a directory"},
   {"isexecutable",cf_class,1,FILESTAT_ARGS,"True if the named object has execution rights for the current user"},
   {"isgreaterthan",cf_class,2,ISGREATERTHAN_ARGS,"True if arg1 is numerically greater than arg2, else compare strings like strcmp"},
   {"islessthan",cf_class,2,ISLESSTHAN_ARGS,"True if arg1 is numerically less than arg2, else compare strings like NOT strcmp"},
   {"islink",cf_class,1,FILESTAT_ARGS,"True if the named object is a symbolic link"},
   {"isnewerthan",cf_class,2,ISNEWERTHAN_ARGS,"True if arg1 is newer (modified later) than arg2 (mtime)"},
   {"isplain",cf_class,1,FILESTAT_ARGS,"True if the named object is a plain/regular file"},
   {"isvariable",cf_class,1,ISVARIABLE_ARGS,"True if the named variable is defined"},
   {"join",cf_str,2,JOIN_ARGS,"Join the items of arg2 into a string, using the conjunction in arg1"},
   {"lastnode",cf_str,2,LASTNODE_ARGS,"Extract the last of a separated string, e.g. filename from a path"},
   {"laterthan",cf_class,6,LATERTHAN_ARGS,"True if the current time is later than the given date"},
   {"ldaparray",cf_class,6,LDAPARRAY_ARGS,"Extract all values from an ldap record"},
   {"ldaplist",cf_slist,6,LDAPLIST_ARGS,"Extract all named values from multiple ldap records"},
   {"ldapvalue",cf_str,6,LDAPVALUE_ARGS,"Extract the first matching named value from ldap"},
   {"now",cf_int,0,NOW_ARGS,"Convert the current time into system representation"},
   {"on",cf_int,6,DATE_ARGS,"Convert an exact date/time to an integer system representation"},
   {"peers",cf_slist,3,PEERS_ARGS,"Get a list of peers (not including ourself) from the partition to which we belong"},
   {"peerleader",cf_str,3,PEERLEADER_ARGS,"Get the assigned peer-leader of the partition to which we belong"},
   {"peerleaders",cf_slist,3,PEERLEADERS_ARGS,"Get a list of peer leaders from the named partitioning"},
   {"product",cf_real,1,PRODUCT_ARGS,"Return the product of a list of reals"},
   {"randomint",cf_int,2,RANDOMINT_ARGS,"Generate a random integer between the given limits"},
   {"readfile",cf_str,2,READFILE_ARGS,"Read max number of bytes from named file and assign to variable"},
   {"readintarray",cf_int,6,READSTRINGARRAY_ARGS,"Read an array of integers from a file and assign the dimension to a variable"},
   {"readintlist",cf_ilist,5,READSTRINGLIST_ARGS,"Read and assign a list variable from a file of separated ints"},
   {"readrealarray",cf_int,6,READSTRINGARRAY_ARGS,"Read an array of real numbers from a file and assign the dimension to a variable"},
   {"readreallist",cf_rlist,5,READSTRINGLIST_ARGS,"Read and assign a list variable from a file of separated real numbers"},
   {"readstringarray",cf_int,6,READSTRINGARRAY_ARGS,"Read an array of strings from a file and assign the dimension to a variable"},
   {"readstringarrayidx",cf_int,6,READSTRINGARRAYIDX_ARGS,"Read an array of strings from a file and assign the dimension to a variable with integer indeces"},
   {"readstringlist",cf_slist,5,READSTRINGLIST_ARGS,"Read and assign a list variable from a file of separated strings"},
   {"readtcp",cf_str,4,READTCP_ARGS,"Connect to tcp port, send string and assign result to variable"},
   {"regarray",cf_class,2,REGARRAY_ARGS,"True if arg1 matches any item in the associative array with id=arg2"},
   {"regcmp",cf_class,2,REGCMP_ARGS,"True if arg1 is a regular expression matching that matches string arg2"},
   {"regextract",cf_class,3,REGEXTRACT_ARGS,"True if the regular expression in arg 1 matches the string in arg2 and sets a non-empty array of backreferences named arg3"},
   {"registryvalue",cf_str,2,REGISTRYVALUE_ARGS,"Returns a value for an MS-Win registry key,value pair"},
   {"regline",cf_class,2,REGLINE_ARGS,"True if the regular expression in arg1 matches a line in file arg2"},
   {"reglist",cf_class,2,REGLIST_ARGS,"True if the regular expression in arg2 matches any item in the list whose id is arg1"},
   {"regldap",cf_class,7,REGLDAP_ARGS,"True if the regular expression in arg6 matches a value item in an ldap search"},
   {"remotescalar",cf_str,3,REMOTESCALAR_ARGS,"Read a scalar value from a remote cfengine server"},
   {"remoteclassesmatching",cf_class,4,REMOTECLASSESMATCHING_ARGS,"Read persistent classes matching a regular expression from a remote cfengine server and add them into local context with prefix"},
   {"returnszero",cf_class,2,RETURNSZERO_ARGS,"True if named shell command has exit status zero"},
   {"rrange",cf_rrange,2,RRANGE_ARGS,"Define a range of real numbers for cfengine internal use"},
   {"selectservers",cf_int,6,SELECTSERVERS_ARGS,"Select tcp servers which respond correctly to a query and return their number, set array of names"},
   {"splayclass",cf_class,2,SPLAYCLASS_ARGS,"True if the first argument's time-slot has arrived, according to a policy in arg2"},
   {"splitstring",cf_slist,3,SPLITSTRING_ARGS,"Convert a string in arg1 into a list of max arg3 strings by splitting on a regular expression in arg2"},
   {"strcmp",cf_class,2,STRCMP_ARGS,"True if the two strings match exactly"},
   {"sum",cf_real,1,SUM_ARGS,"Return the sum of a list of reals"},
   {"translatepath",cf_str,1,TRANSLATEPATH_ARGS,"Translate path separators from Unix style to the host's native"},
   {"usemodule",cf_class,2,USEMODULE_ARGS,"Execute cfengine module script and set class if successful"},
   {"userexists",cf_class,1,USEREXISTS_ARGS,"True if user name or numerical id exists on this host"},
   {NULL,cf_notype,0,NULL,NULL}
   };
