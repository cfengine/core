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

#include "evalfunction.h"

#include "env_context.h"
#include "promises.h"
#include "dir.h"
#include "dbm_api.h"
#include "lastseen.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "vars.h"
#include "addr_lib.h"
#include "syntax.h"
#include "item_lib.h"
#include "conversion.h"
#include "reporting.h"
#include "expand.h"
#include "scope.h"
#include "keyring.h"
#include "matching.h"
#include "hashes.h"
#include "unix.h"
#include "cfstream.h"
#include "string_lib.h"
#include "args.h"
#include "client_code.h"
#include "communication.h"
#include "pipes.h"

#include <libgen.h>

static char *StripPatterns(char *file_buffer, char *pattern, char *filename);
static void CloseStringHole(char *s, int start, int end);
static int BuildLineArray(char *array_lval, char *file_buffer, char *split, int maxent, enum cfdatatype type, int intIndex);
static int ExecModule(char *command, const char*namespace);
static int CheckID(char *id);

static void *CfReadFile(char *filename, int maxsize);

/*******************************************************************/

int FnNumArgs(const FnCallType *call_type)
{
    for (int i = 0;; i++)
    {
        if (call_type->args[i].pattern == NULL)
        {
            return i;
        }
    }
}

/*******************************************************************/

/* assume args are all scalar literals by the time we get here
     and each handler allocates the memory it returns. There is
     a protocol to be followed here:
     Set args,
     Eval Content,
     Set rtype,
     ErrorFlags

     returnval = FnCallXXXResult(fp)

  */

/*******************************************************************/
/* End FnCall API                                                  */
/*******************************************************************/

static Rlist *GetHostsFromLastseenDB(Item *addresses, time_t horizon, bool return_address, bool return_recent)
{
    Rlist *recent = NULL, *aged = NULL;
    Item *ip;
    time_t now = time(NULL);
    double entrytime;
    char address[CF_MAXVARSIZE];

    for (ip = addresses; ip != NULL; ip = ip->next)
    {
        if (sscanf(ip->classes, "%lf", &entrytime) != 1)
        {
            CfOut(cf_error, "", "!! Could not get host entry age");
            continue;
        }

        if (return_address)
        {
            snprintf(address, sizeof(address), "%s", ip->name);
        }
        else
        {
            snprintf(address, sizeof(address), "%s", IPString2Hostname(ip->name));
        }

        if (entrytime < now - horizon)
        {
            CfDebug("Old entry.\n");

            if (KeyInRlist(recent, address))
            {
                CfDebug("There is recent entry for this address. Do nothing.\n");
            }
            else
            {
                CfDebug("Adding to list of aged hosts.\n");
                IdempPrependRScalar(&aged, address, CF_SCALAR);
            }
        }
        else
        {
            Rlist *r;

            CfDebug("Recent entry.\n");

            if ((r = KeyInRlist(aged, address)))
            {
                CfDebug("Purging from list of aged hosts.\n");
                DeleteRlistEntry(&aged, r);
            }

            CfDebug("Adding to list of recent hosts.\n");
            IdempPrependRScalar(&recent, address, CF_SCALAR);
        }
    }

    if (return_recent)
    {
        DeleteRlist(aged);
        if (recent == NULL)
        {
            IdempAppendRScalar(&recent, CF_NULL_VALUE, CF_SCALAR);
        }
        return recent;
    }
    else
    {
        DeleteRlist(recent);
        if (aged == NULL)
        {
            IdempAppendRScalar(&aged, CF_NULL_VALUE, CF_SCALAR);
        }
        return aged;
    }
}

/*********************************************************************/

static FnCallResult FnCallAnd(FnCall *fp, Rlist *finalargs)
{
    Rlist *arg;
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall and-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (arg = finalargs; arg; arg = arg->next)
    {
        CheckConstraintTypeMatch(id, (Rval) {arg->item, arg->type}, cf_str, "", 1);
    }

    for (arg = finalargs; arg; arg = arg->next)
    {
        if (!IsDefinedClass(ScalarValue(arg), fp->namespace))
        {
            return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), CF_SCALAR } };
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
}

/*******************************************************************/

static bool CallHostsSeenCallback(const char *hostkey, const char *address,
                                  bool incoming, const KeyHostSeen *quality,
                                  void *ctx)
{
    Item **addresses = ctx;

    if (HostKeyAddressUnknown(hostkey))
    {
        return true;
    }

    char buf[CF_BUFSIZE];
    snprintf(buf, sizeof(buf), "%ju", (uintmax_t)quality->lastseen);

    PrependItem(addresses, address, buf);

    return true;
}

/*******************************************************************/

static FnCallResult FnCallHostsSeen(FnCall *fp, Rlist *finalargs)
{
    Item *addresses = NULL;

    int horizon = Str2Int(ScalarValue(finalargs)) * 3600;
    char *policy = ScalarValue(finalargs->next);
    char *format = ScalarValue(finalargs->next->next);

    CfDebug("Calling hostsseen(%d,%s,%s)\n", horizon, policy, format);

    if (!ScanLastSeenQuality(&CallHostsSeenCallback, &addresses))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    Rlist *returnlist = GetHostsFromLastseenDB(addresses, horizon,
                                               strcmp(format, "address") == 0,
                                               strcmp(policy, "lastseen") == 0);

    DeleteItemList(addresses);

    CfDebug(" | Return value:\n");
    for (Rlist *rp = returnlist; rp; rp = rp->next)
    {
        CfDebug(" |  %s\n", (char *) rp->item);
    }

    if (returnlist == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { returnlist, CF_LIST } };
    }
}

/*********************************************************************/

static FnCallResult FnCallHostsWithClass(FnCall *fp, Rlist *finalargs)
{
    Rlist *returnlist = NULL;

    char *class_name = ScalarValue(finalargs);
    char *return_format = ScalarValue(finalargs->next);
    
    if(!CFDB_HostsWithClass(&returnlist, class_name, return_format))
    {
        return (FnCallResult){ FNCALL_FAILURE };
    }
    
    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, CF_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallRandomInt(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    int tmp, range, result;

    buffer[0] = '\0';

/* begin fn specific content */

    int from = Str2Int(ScalarValue(finalargs));
    int to = Str2Int(ScalarValue(finalargs->next));

    if (from == CF_NOINT || to == CF_NOINT)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (from > to)
    {
        tmp = to;
        to = from;
        from = tmp;
    }

    range = fabs(to - from);
    result = from + (int) (drand48() * (double) range);
    snprintf(buffer, CF_BUFSIZE - 1, "%d", result);

/* end fn specific content */

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallGetEnv(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE] = "", ctrlstr[CF_SMALLBUF];

/* begin fn specific content */

    char *name = ScalarValue(finalargs);
    int limit = Str2Int(ScalarValue(finalargs->next));

    snprintf(ctrlstr, CF_SMALLBUF, "%%.%ds", limit);    // -> %45s

    if (getenv(name))
    {
        snprintf(buffer, CF_BUFSIZE - 1, ctrlstr, getenv(name));
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

#if defined(HAVE_GETPWENT)

static FnCallResult FnCallGetUsers(FnCall *fp, Rlist *finalargs)
{
    Rlist *newlist = NULL, *except_names, *except_uids;
    struct passwd *pw;

/* begin fn specific content */

    char *except_name = ScalarValue(finalargs);
    char *except_uid = ScalarValue(finalargs->next);

    except_names = SplitStringAsRList(except_name, ',');
    except_uids = SplitStringAsRList(except_uid, ',');

    setpwent();

    while ((pw = getpwent()))
    {
        if (!IsStringIn(except_names, pw->pw_name) && !IsIntIn(except_uids, (int) pw->pw_uid))
        {
            IdempPrependRScalar(&newlist, pw->pw_name, CF_SCALAR);
        }
    }

    endpwent();

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, CF_LIST } };
}

#else

static FnCallResult FnCallGetUsers(FnCall *fp, Rlist *finalargs)
{
    CfOut(cf_error, "", " -> getusers is not implemented");
    return (FnCallResult) { FNCALL_FAILURE };
}

#endif

/*********************************************************************/

static FnCallResult FnCallEscape(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *name = ScalarValue(finalargs);

    EscapeSpecialChars(name, buffer, CF_BUFSIZE - 1, "", "");

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallHost2IP(FnCall *fp, Rlist *finalargs)
{
    char *name = ScalarValue(finalargs);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(Hostname2IPString(name)), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallIP2Host(FnCall *fp, Rlist *finalargs)
{
/* begin fn specific content */
    char *ip = ScalarValue(finalargs);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(IPString2Hostname(ip)), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallGetUid(FnCall *fp, Rlist *finalargs)
#ifndef MINGW
{
    struct passwd *pw;

/* begin fn specific content */

    if ((pw = getpwnam(ScalarValue(finalargs))) == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else
    {
        char buffer[CF_BUFSIZE];

        snprintf(buffer, CF_BUFSIZE - 1, "%ju", (uintmax_t)pw->pw_uid);
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
    }
}
#else                           /* MINGW */
{
    return (FnCallResult) { FNCALL_FAILURE };
}
#endif /* MINGW */

/*********************************************************************/

static FnCallResult FnCallGetGid(FnCall *fp, Rlist *finalargs)
#ifndef MINGW
{
    struct group *gr;

/* begin fn specific content */

    if ((gr = getgrnam(ScalarValue(finalargs))) == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else
    {
        char buffer[CF_BUFSIZE];

        snprintf(buffer, CF_BUFSIZE - 1, "%ju", (uintmax_t)gr->gr_gid);
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
    }
}
#else                           /* MINGW */
{
    return (FnCallResult) { FNCALL_FAILURE };
}
#endif /* MINGW */

/*********************************************************************/

static FnCallResult FnCallHash(FnCall *fp, Rlist *finalargs)
/* Hash(string,md5|sha1|crypt) */
{
    char buffer[CF_BUFSIZE];
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    enum cfhashes type;

    buffer[0] = '\0';

/* begin fn specific content */

    char *string = ScalarValue(finalargs);
    char *typestring = ScalarValue(finalargs->next);

    type = String2HashType(typestring);

    if (FIPS_MODE && type == cf_md5)
    {
        CfOut(cf_error, "", " !! FIPS mode is enabled, and md5 is not an approved algorithm in call to hash()");
    }

    HashString(string, strlen(string), digest, type);

    snprintf(buffer, CF_BUFSIZE - 1, "%s", HashPrint(type, digest));

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(SkipHashType(buffer)), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallHashMatch(FnCall *fp, Rlist *finalargs)
/* HashMatch(string,md5|sha1|crypt,"abdxy98edj") */
{
    char buffer[CF_BUFSIZE], ret[CF_BUFSIZE];
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    enum cfhashes type;

    buffer[0] = '\0';

/* begin fn specific content */

    char *string = ScalarValue(finalargs);
    char *typestring = ScalarValue(finalargs->next);
    char *compare = ScalarValue(finalargs->next->next);

    type = String2HashType(typestring);
    HashFile(string, digest, type);
    snprintf(buffer, CF_BUFSIZE - 1, "%s", HashPrint(type, digest));
    CfOut(cf_verbose, "", " -> File \"%s\" hashes to \"%s\", compare to \"%s\"\n", string, buffer, compare);

    if (strcmp(buffer + 4, compare) == 0)
    {
        strcpy(ret, "any");
    }
    else
    {
        strcpy(ret, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(ret), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallConcat(FnCall *fp, Rlist *finalargs)
{
    Rlist *arg = NULL;
    char id[CF_BUFSIZE];
    char result[CF_BUFSIZE] = "";

    snprintf(id, CF_BUFSIZE, "built-in FnCall concat-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (arg = finalargs; arg; arg = arg->next)
    {
        CheckConstraintTypeMatch(id, (Rval) {arg->item, arg->type}, cf_str, "", 1);
    }

    for (arg = finalargs; arg; arg = arg->next)
    {
        if (strlcat(result, ScalarValue(arg), CF_BUFSIZE) >= CF_BUFSIZE)
        {
            /* Complain */
            CfOut(cf_error, "", "!! Unable to evaluate concat() function, arguments are too long");
            return (FnCallResult) { FNCALL_FAILURE};
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(result), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallClassMatch(FnCall *fp, Rlist *finalargs)
{
    if (MatchInAlphaList(&VHARDHEAP, ScalarValue(finalargs)) || MatchInAlphaList(&VHEAP, ScalarValue(finalargs)) || MatchInAlphaList(&VADDCLASSES, ScalarValue(finalargs)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), CF_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallCountClassesMatching(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], *string = ScalarValue(finalargs);
    Item *ip;
    int count = 0;
    int i = (int) *string;

/* begin fn specific content */

    if (isalnum(i) || *string == '_')
    {
        for (ip = VHEAP.list[i]; ip != NULL; ip = ip->next)
        {
            if (FullTextMatch(string, ip->name))
            {
                count++;
            }
        }

        for (ip = VHARDHEAP.list[i]; ip != NULL; ip = ip->next)
        {
            if (FullTextMatch(string, ip->name))
            {
                count++;
            }
        }

        for (ip = VADDCLASSES.list[i]; ip != NULL; ip = ip->next)
        {
            if (FullTextMatch(string, ip->name))
            {
                count++;
            }
        }
    }
    else
    {
        for (i = 0; i < CF_ALPHABETSIZE; i++)
        {
            for (ip = VHEAP.list[i]; ip != NULL; ip = ip->next)
            {
                if (FullTextMatch(string, ip->name))
                {
                    count++;
                }
            }

            for (ip = VHARDHEAP.list[i]; ip != NULL; ip = ip->next)
            {
                if (FullTextMatch(string, ip->name))
                {
                    count++;
                }
            }

            for (ip = VADDCLASSES.list[i]; ip != NULL; ip = ip->next)
            {
                if (FullTextMatch(string, ip->name))
                {
                    count++;
                }
            }
        }
    }

    snprintf(buffer, CF_MAXVARSIZE, "%d", count);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallCanonify(FnCall *fp, Rlist *finalargs)
{
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(CanonifyName(ScalarValue(finalargs))), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallLastNode(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist;

/* begin fn specific content */

    char *name = ScalarValue(finalargs);
    char *split = ScalarValue(finalargs->next);

    newlist = SplitRegexAsRList(name, split, 100, true);

    for (rp = newlist; rp != NULL; rp = rp->next)
    {
        if (rp->next == NULL)
        {
            break;
        }
    }

    if (rp && rp->item)
    {
        char *res = xstrdup(rp->item);

        DeleteRlist(newlist);
        return (FnCallResult) { FNCALL_SUCCESS, { res, CF_SCALAR } };
    }
    else
    {
        DeleteRlist(newlist);
        return (FnCallResult) { FNCALL_FAILURE };
    }
}

/*******************************************************************/

static FnCallResult FnCallDirname(FnCall *fp, Rlist *finalargs)
{
    char *dir = xstrdup(ScalarValue(finalargs));

    DeleteSlash(dir);
    ChopLastNode(dir);

    return (FnCallResult) { FNCALL_SUCCESS, { dir, CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallClassify(FnCall *fp, Rlist *finalargs)
{
    bool is_defined = IsDefinedClass(CanonifyName(ScalarValue(finalargs)), fp->namespace);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(is_defined ? "any" : "!any"), CF_SCALAR } };
}

/*********************************************************************/
/* Executions                                                        */
/*********************************************************************/

static FnCallResult FnCallReturnsZero(FnCall *fp, Rlist *finalargs)
{
    if (!IsAbsoluteFileName(ScalarValue(finalargs)))
    {
        CfOut(cf_error, "", "execresult \"%s\" does not have an absolute path\n", ScalarValue(finalargs));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!IsExecutable(GetArg0(ScalarValue(finalargs))))
    {
        CfOut(cf_error, "", "execresult \"%s\" is assumed to be executable but isn't\n", ScalarValue(finalargs));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    struct stat statbuf;
    char comm[CF_BUFSIZE];
    int useshell = strcmp(ScalarValue(finalargs->next), "useshell") == 0;

    snprintf(comm, CF_BUFSIZE, "%s", ScalarValue(finalargs));

    if (cfstat(GetArg0(ScalarValue(finalargs)), &statbuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (ShellCommandReturnsZero(comm, useshell))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), CF_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallExecResult(FnCall *fp, Rlist *finalargs)
  /* execresult("/programpath",useshell|noshell) */
{
    if (!IsAbsoluteFileName(ScalarValue(finalargs)))
    {
        CfOut(cf_error, "", "execresult \"%s\" does not have an absolute path\n", ScalarValue(finalargs));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!IsExecutable(GetArg0(ScalarValue(finalargs))))
    {
        CfOut(cf_error, "", "execresult \"%s\" is assumed to be executable but isn't\n", ScalarValue(finalargs));
        return (FnCallResult) { FNCALL_FAILURE };
    }

    bool useshell = strcmp(ScalarValue(finalargs->next), "useshell") == 0;
    char buffer[CF_EXPANDSIZE];

    if (GetExecOutput(ScalarValue(finalargs), buffer, useshell))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
}

/*********************************************************************/

static FnCallResult FnCallUseModule(FnCall *fp, Rlist *finalargs)
  /* usemodule("/programpath",varargs) */
{
    char modulecmd[CF_BUFSIZE];
    struct stat statbuf;

/* begin fn specific content */

    char *command = ScalarValue(finalargs);
    char *args = ScalarValue(finalargs->next);

    snprintf(modulecmd, CF_BUFSIZE, "%s%cmodules%c%s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, command);

    if (cfstat(GetArg0(modulecmd), &statbuf) == -1)
    {
        CfOut(cf_error, "", "(Plug-in module %s not found)", modulecmd);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if ((statbuf.st_uid != 0) && (statbuf.st_uid != getuid()))
    {
        CfOut(cf_error, "", "Module %s was not owned by uid=%ju who is executing agent\n", modulecmd, (uintmax_t)getuid());
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (!JoinPath(modulecmd, args))
    {
        CfOut(cf_error, "", "Culprit: class list for module (shouldn't happen)\n");
        return (FnCallResult) { FNCALL_FAILURE };
    }

    snprintf(modulecmd, CF_BUFSIZE, "%s%cmodules%c%s %s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, command, args);
    CfOut(cf_verbose, "", "Executing and using module [%s]\n", modulecmd);

    if (!ExecModule(modulecmd, fp->namespace))
    {
        return (FnCallResult) { FNCALL_FAILURE};
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
}

/*********************************************************************/
/* Misc                                                              */
/*********************************************************************/

static FnCallResult FnCallSplayClass(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], class[CF_MAXVARSIZE];

    enum cfinterval policy = Str2Interval(ScalarValue(finalargs->next));

    if (policy == cfa_hourly)
    {
        /* 12 5-minute slots in hour */
        int slot = GetHash(ScalarValue(finalargs)) * 12 / CF_HASHTABLESIZE;
        snprintf(class, CF_MAXVARSIZE, "Min%02d_%02d", slot * 5, ((slot + 1) * 5) % 60);
    }
    else
    {
        /* 12*24 5-minute slots in day */
        int dayslot = GetHash(ScalarValue(finalargs)) * 12 * 24 / CF_HASHTABLESIZE;
        int hour = dayslot / 12;
        int slot = dayslot % 12;

        snprintf(class, CF_MAXVARSIZE, "Min%02d_%02d.Hr%02d", slot * 5, ((slot + 1) * 5) % 60, hour);
    }

    if (IsDefinedClass(class, fp->namespace))
    {
        strcpy(buffer, "any");
    }
    else
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallReadTcp(FnCall *fp, Rlist *finalargs)
 /* ReadTCP(localhost,80,'GET index.html',1000) */
{
    AgentConnection *conn = NULL;
    char buffer[CF_BUFSIZE];
    int val = 0, n_read = 0;
    short portnum;
    Attributes attr = { {0} };

    memset(buffer, 0, sizeof(buffer));

/* begin fn specific content */

    char *hostnameip = ScalarValue(finalargs);
    char *port = ScalarValue(finalargs->next);
    char *sendstring = ScalarValue(finalargs->next->next);
    char *maxbytes = ScalarValue(finalargs->next->next->next);

    val = Str2Int(maxbytes);
    portnum = (short) Str2Int(port);

    if (val < 0 || portnum < 0 || THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (val > CF_BUFSIZE - 1)
    {
        CfOut(cf_error, "", "Too many bytes to read from TCP port %s@%s", port, hostnameip);
        val = CF_BUFSIZE - CF_BUFFERMARGIN;
    }

    CfDebug("Want to read %d bytes from port %d at %s\n", val, portnum, hostnameip);

    conn = NewAgentConn();

    attr.copy.force_ipv4 = false;
    attr.copy.portnumber = portnum;

    if (!ServerConnect(conn, hostnameip, attr, NULL))
    {
        CfOut(cf_inform, "socket", "Couldn't open a tcp socket");
        DeleteAgentConn(conn);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (strlen(sendstring) > 0)
    {
        if (SendSocketStream(conn->sd, sendstring, strlen(sendstring), 0) == -1)
        {
            cf_closesocket(conn->sd);
            DeleteAgentConn(conn);
            return (FnCallResult) { FNCALL_FAILURE };
        }
    }

    if ((n_read = recv(conn->sd, buffer, val, 0)) == -1)
    {
    }

    if (n_read == -1)
    {
        cf_closesocket(conn->sd);
        DeleteAgentConn(conn);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    cf_closesocket(conn->sd);
    DeleteAgentConn(conn);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRegList(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *list;
    char buffer[CF_BUFSIZE], naked[CF_MAXVARSIZE];
    Rval retval;

    buffer[0] = '\0';

/* begin fn specific content */

    char *listvar = ScalarValue(finalargs);
    char *regex = ScalarValue(finalargs->next);

    if (*listvar == '@')
    {
        GetNaked(naked, listvar);
    }
    else
    {
        CfOut(cf_verbose, "", "Function reglist was promised a list called \"%s\" but this was not found\n", listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (GetVariable(CONTEXTID, naked, &retval) == cf_notype)
    {
        CfOut(cf_verbose, "", "Function REGLIST was promised a list called \"%s\" but this was not found\n", listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (retval.rtype != CF_LIST)
    {
        CfOut(cf_verbose, "", "Function reglist was promised a list called \"%s\" but this variable is not a list\n",
              listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    list = (Rlist *) retval.item;

    strcpy(buffer, "!any");

    for (rp = list; rp != NULL; rp = rp->next)
    {
        if (strcmp(rp->item, CF_NULL_VALUE) == 0)
        {
            continue;
        }

        if (FullTextMatch(regex, rp->item))
        {
            strcpy(buffer, "any");
            break;
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRegArray(FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], scopeid[CF_MAXVARSIZE];
    char match[CF_MAXVARSIZE], buffer[CF_BUFSIZE];
    Scope *ptr;
    HashIterator i;
    CfAssoc *assoc;

/* begin fn specific content */

    char *arrayname = ScalarValue(finalargs);
    char *regex = ScalarValue(finalargs->next);

/* Locate the array */

    if (strstr(arrayname, "."))
    {
        scopeid[0] = '\0';
        sscanf(arrayname, "%[^.].%s", scopeid, lval);
    }
    else
    {
        strcpy(lval, arrayname);
        strcpy(scopeid, CONTEXTID);
    }

    if ((ptr = GetScope(scopeid)) == NULL)
    {
        CfOut(cf_verbose, "", "Function regarray was promised an array called \"%s\" but this was not found\n",
              arrayname);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    strcpy(buffer, "!any");

    i = HashIteratorInit(ptr->hashtable);

    while ((assoc = HashIteratorNext(&i)))
    {
        snprintf(match, CF_MAXVARSIZE, "%s[", lval);
        if (strncmp(match, assoc->lval, strlen(match)) == 0)
        {
            if (FullTextMatch(regex, assoc->rval.item))
            {
                strcpy(buffer, "any");
                break;
            }
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallGetIndices(FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], scopeid[CF_MAXVARSIZE];
    char index[CF_MAXVARSIZE], match[CF_MAXVARSIZE];
    Scope *ptr;
    Rlist *returnlist = NULL;
    HashIterator i;
    CfAssoc *assoc;

/* begin fn specific content */

    char *arrayname = ScalarValue(finalargs);

/* Locate the array */

    if (strstr(arrayname, "."))
    {
        scopeid[0] = '\0';
        sscanf(arrayname, "%127[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, arrayname);
        strcpy(scopeid, CONTEXTID);
    }

    if ((ptr = GetScope(scopeid)) == NULL)
    {
        CfOut(cf_verbose, "",
              "Function getindices was promised an array called \"%s\" in scope \"%s\" but this was not found\n", lval,
              scopeid);
        IdempAppendRScalar(&returnlist, CF_NULL_VALUE, CF_SCALAR);
        return (FnCallResult) { FNCALL_SUCCESS, { returnlist, CF_LIST } };
    }

    i = HashIteratorInit(ptr->hashtable);

    while ((assoc = HashIteratorNext(&i)))
    {
        snprintf(match, CF_MAXVARSIZE - 1, "%.127s[", lval);

        if (strncmp(match, assoc->lval, strlen(match)) == 0)
        {
            char *sp;

            index[0] = '\0';
            sscanf(assoc->lval + strlen(match), "%127[^\n]", index);
            if ((sp = strchr(index, ']')))
            {
                *sp = '\0';
            }
            else
            {
                index[strlen(index) - 1] = '\0';
            }

            if (strlen(index) > 0)
            {
                IdempAppendRScalar(&returnlist, index, CF_SCALAR);
            }
        }
    }

    if (returnlist == NULL)
    {
        IdempAppendRScalar(&returnlist, CF_NULL_VALUE, CF_SCALAR);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, CF_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallGetValues(FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], scopeid[CF_MAXVARSIZE];
    char match[CF_MAXVARSIZE];
    Scope *ptr;
    Rlist *rp, *returnlist = NULL;
    HashIterator i;
    CfAssoc *assoc;

/* begin fn specific content */

    char *arrayname = ScalarValue(finalargs);

/* Locate the array */

    if (strstr(arrayname, "."))
    {
        scopeid[0] = '\0';
        sscanf(arrayname, "%127[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, arrayname);
        strcpy(scopeid, CONTEXTID);
    }

    if ((ptr = GetScope(scopeid)) == NULL)
    {
        CfOut(cf_verbose, "",
              "Function getvalues was promised an array called \"%s\" in scope \"%s\" but this was not found\n", lval,
              scopeid);
        IdempAppendRScalar(&returnlist, CF_NULL_VALUE, CF_SCALAR);
        return (FnCallResult) { FNCALL_SUCCESS, { returnlist, CF_LIST } };
    }

    i = HashIteratorInit(ptr->hashtable);

    while ((assoc = HashIteratorNext(&i)))
    {
        snprintf(match, CF_MAXVARSIZE - 1, "%.127s[", lval);

        if (strncmp(match, assoc->lval, strlen(match)) == 0)
        {
            switch (assoc->rval.rtype)
            {
            case CF_SCALAR:
                IdempAppendRScalar(&returnlist, assoc->rval.item, CF_SCALAR);
                break;

            case CF_LIST:
                for (rp = assoc->rval.item; rp != NULL; rp = rp->next)
                {
                    IdempAppendRScalar(&returnlist, rp->item, CF_SCALAR);
                }
                break;
            }
        }
    }

    if (returnlist == NULL)
    {
        IdempAppendRScalar(&returnlist, CF_NULL_VALUE, CF_SCALAR);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, CF_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallGrep(FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE];
    char scopeid[CF_MAXVARSIZE];
    Rval rval2;
    Rlist *rp, *returnlist = NULL;
    Scope *ptr;

/* begin fn specific content */

    char *regex = ScalarValue(finalargs);
    char *name = ScalarValue(finalargs->next);

/* Locate the array */

    if (strstr(name, "."))
    {
        scopeid[0] = '\0';
        sscanf(name, "%127[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, name);
        strcpy(scopeid, CONTEXTID);
    }

    if ((ptr = GetScope(scopeid)) == NULL)
    {
        CfOut(cf_verbose, "", "Function \"grep\" was promised an array in scope \"%s\" but this was not found\n",
              scopeid);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (GetVariable(scopeid, lval, &rval2) == cf_notype)
    {
        CfOut(cf_verbose, "", "Function \"grep\" was promised a list called \"%s\" but this was not found\n", name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (rval2.rtype != CF_LIST)
    {
        CfOut(cf_verbose, "", "Function grep was promised a list called \"%s\" but this was not found\n", name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    AppendRScalar(&returnlist, CF_NULL_VALUE, CF_SCALAR);

    for (rp = (Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        if (FullTextMatch(regex, rp->item))
        {
            AppendRScalar(&returnlist, rp->item, CF_SCALAR);
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { returnlist, CF_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallSum(FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], buffer[CF_MAXVARSIZE];
    char scopeid[CF_MAXVARSIZE];
    Rval rval2;
    Rlist *rp;
    Scope *ptr;
    double sum = 0;

/* begin fn specific content */

    char *name = ScalarValue(finalargs);

/* Locate the array */

    if (strstr(name, "."))
    {
        scopeid[0] = '\0';
        sscanf(name, "%127[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, name);
        strcpy(scopeid, CONTEXTID);
    }

    if ((ptr = GetScope(scopeid)) == NULL)
    {
        CfOut(cf_verbose, "", "Function \"sum\" was promised a list in scope \"%s\" but this was not found\n", scopeid);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (GetVariable(scopeid, lval, &rval2) == cf_notype)
    {
        CfOut(cf_verbose, "", "Function \"sum\" was promised a list called \"%s\" but this was not found\n", name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (rval2.rtype != CF_LIST)
    {
        CfOut(cf_verbose, "", "Function \"sum\" was promised a list called \"%s\" but this was not found\n", name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (rp = (Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        double x;

        if ((x = Str2Double(rp->item)) == CF_NODOUBLE)
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }
        else
        {
            sum += x;
        }
    }

    snprintf(buffer, CF_MAXVARSIZE, "%lf", sum);
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallProduct(FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], buffer[CF_MAXVARSIZE];
    char scopeid[CF_MAXVARSIZE];
    Rval rval2;
    Rlist *rp;
    Scope *ptr;
    double product = 1.0;

/* begin fn specific content */

    char *name = ScalarValue(finalargs);

/* Locate the array */

    if (strstr(name, "."))
    {
        scopeid[0] = '\0';
        sscanf(name, "%127[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, name);
        strcpy(scopeid, CONTEXTID);
    }

    if ((ptr = GetScope(scopeid)) == NULL)
    {
        CfOut(cf_verbose, "", "Function \"product\" was promised a list in scope \"%s\" but this was not found\n",
              scopeid);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (GetVariable(scopeid, lval, &rval2) == cf_notype)
    {
        CfOut(cf_verbose, "", "Function \"product\" was promised a list called \"%s\" but this was not found\n", name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (rval2.rtype != CF_LIST)
    {
        CfOut(cf_verbose, "", "Function \"product\" was promised a list called \"%s\" but this was not found\n", name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (rp = (Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        double x;

        if ((x = Str2Double(rp->item)) == CF_NODOUBLE)
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }
        else
        {
            product *= x;
        }
    }

    snprintf(buffer, CF_MAXVARSIZE, "%lf", product);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallJoin(FnCall *fp, Rlist *finalargs)
{
    char lval[CF_MAXVARSIZE], *joined;
    char scopeid[CF_MAXVARSIZE];
    Rval rval2;
    Rlist *rp;
    Scope *ptr;
    int size = 0;

/* begin fn specific content */

    char *join = ScalarValue(finalargs);
    char *name = ScalarValue(finalargs->next);

/* Locate the array */

    if (strstr(name, "."))
    {
        scopeid[0] = '\0';
        sscanf(name, "%[^.].%127s", scopeid, lval);
    }
    else
    {
        strcpy(lval, name);
        strcpy(scopeid, "this");
    }

    if ((ptr = GetScope(scopeid)) == NULL)
    {
        CfOut(cf_verbose, "", "Function \"join\" was promised an array in scope \"%s\" but this was not found\n",
              scopeid);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (GetVariable(scopeid, lval, &rval2) == cf_notype)
    {
        CfOut(cf_verbose, "", "Function \"join\" was promised a list called \"%s.%s\" but this was not (yet) found\n",
              scopeid, name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (rval2.rtype != CF_LIST)
    {
        CfOut(cf_verbose, "", "Function \"join\" was promised a list called \"%s\" but this was not (yet) found\n",
              name);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (rp = (Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        if (strcmp(rp->item, CF_NULL_VALUE) == 0)
        {
            continue;
        }

        size += strlen(rp->item) + strlen(join);
    }

    joined = xcalloc(1, size + 1);
    size = 0;

    for (rp = (Rlist *) rval2.item; rp != NULL; rp = rp->next)
    {
        if (strcmp(rp->item, CF_NULL_VALUE) == 0)
        {
            continue;
        }

        strcpy(joined + size, rp->item);

        if (rp->next != NULL)
        {
            strcpy(joined + size + strlen(rp->item), join);
            size += strlen(rp->item) + strlen(join);
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { joined, CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallGetFields(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist;
    char name[CF_MAXVARSIZE], line[CF_BUFSIZE], retval[CF_SMALLBUF];
    int lcount = 0, vcount = 0, nopurge = true;
    FILE *fin;

/* begin fn specific content */

    char *regex = ScalarValue(finalargs);
    char *filename = ScalarValue(finalargs->next);
    char *split = ScalarValue(finalargs->next->next);
    char *array_lval = ScalarValue(finalargs->next->next->next);

    if ((fin = fopen(filename, "r")) == NULL)
    {
        CfOut(cf_error, "fopen", " !! File \"%s\" could not be read in getfields()", filename);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    while (!feof(fin))
    {
        line[0] = '\0';
        fgets(line, CF_BUFSIZE - 1, fin);
        Chop(line);

        if (feof(fin))
        {
            break;
        }

        if (!FullTextMatch(regex, line))
        {
            continue;
        }

        if (lcount == 0)
        {
            newlist = SplitRegexAsRList(line, split, 31, nopurge);

            vcount = 1;

            for (rp = newlist; rp != NULL; rp = rp->next)
            {
                snprintf(name, CF_MAXVARSIZE - 1, "%s[%d]", array_lval, vcount);
                NewScalar(THIS_BUNDLE, name, ScalarValue(rp), cf_str);
                CfOut(cf_verbose, "", " -> getfields: defining %s = %s\n", name, ScalarValue(rp));
                vcount++;
            }
        }

        lcount++;
    }

    fclose(fin);

    snprintf(retval, CF_SMALLBUF - 1, "%d", lcount);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(retval), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallCountLinesMatching(FnCall *fp, Rlist *finalargs)
{
    char line[CF_BUFSIZE], retval[CF_SMALLBUF];
    int lcount = 0;
    FILE *fin;

/* begin fn specific content */

    char *regex = ScalarValue(finalargs);
    char *filename = ScalarValue(finalargs->next);

    if ((fin = fopen(filename, "r")) == NULL)
    {
        CfOut(cf_verbose, "fopen", " !! File \"%s\" could not be read in countlinesmatching()", filename);
        snprintf(retval, CF_SMALLBUF - 1, "0");
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(retval), CF_SCALAR } };
    }

    while (!feof(fin))
    {
        line[0] = '\0';
        fgets(line, CF_BUFSIZE - 1, fin);
        Chop(line);

        if (feof(fin))
        {
            break;
        }

        if (FullTextMatch(regex, line))
        {
            lcount++;
            CfOut(cf_verbose, "", " -> countlinesmatching: matched \"%s\"", line);
            continue;
        }
    }

    fclose(fin);

    snprintf(retval, CF_SMALLBUF - 1, "%d", lcount);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(retval), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallLsDir(FnCall *fp, Rlist *finalargs)
{
    char line[CF_BUFSIZE], retval[CF_SMALLBUF];
    Dir *dirh = NULL;
    const struct dirent *dirp;
    Rlist *newlist = NULL;

/* begin fn specific content */

    char *dirname = ScalarValue(finalargs);
    char *regex = ScalarValue(finalargs->next);
    int includepath = GetBoolean(ScalarValue(finalargs->next->next));

    dirh = OpenDirLocal(dirname);

    if (dirh == NULL)
    {
        CfOut(cf_verbose, "opendir", " !! Directory \"%s\" could not be accessed in lsdir()", dirname);
        snprintf(retval, CF_SMALLBUF - 1, "0");
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(retval), CF_SCALAR } };
    }

    for (dirp = ReadDir(dirh); dirp != NULL; dirp = ReadDir(dirh))
    {
        if (strlen(regex) == 0 || FullTextMatch(regex, dirp->d_name))
        {
            if (includepath)
            {
                snprintf(line, CF_BUFSIZE, "%s/%s", dirname, dirp->d_name);
                MapName(line);
                PrependRScalar(&newlist, line, CF_SCALAR);
            }
            else
            {
                PrependRScalar(&newlist, (char *) dirp->d_name, CF_SCALAR);
            }
        }
    }

    CloseDir(dirh);

    if (newlist == NULL)
    {
        PrependRScalar(&newlist, "cf_null", CF_SCALAR);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, CF_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallMapList(FnCall *fp, Rlist *finalargs)
{
    char expbuf[CF_EXPANDSIZE], lval[CF_MAXVARSIZE], scopeid[CF_MAXVARSIZE];
    Rlist *rp, *newlist = NULL;
    Rval rval;
    Scope *ptr;
    enum cfdatatype retype;

/* begin fn specific content */

    char *map = ScalarValue(finalargs);
    char *listvar = ScalarValue(finalargs->next);

/* Locate the array */

    if (*listvar == '@')        // Handle use of @(list) as well as raw name
    {
        listvar += 2;
    }

    if (strstr(listvar, "."))
    {
        scopeid[0] = '\0';
        sscanf(listvar, "%127[^.].%127[^)}]", scopeid, lval);
    }
    else
    {
        strcpy(lval, listvar);

        if (*(lval + strlen(lval) - 1) == ')' || *(lval + strlen(lval) - 1) == '}')
        {
            *(lval + strlen(lval) - 1) = '\0';
        }

        strcpy(scopeid, CONTEXTID);
    }

    if ((ptr = GetScope(scopeid)) == NULL)
    {
        CfOut(cf_verbose, "", "Function \"maplist\" was promised an list in scope \"%s\" but this was not found\n",
              scopeid);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    retype = GetVariable(scopeid, lval, &rval);

    if (retype != cf_slist && retype != cf_ilist && retype != cf_rlist)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
    {
        NewScalar("this", "this", (char *) rp->item, cf_str);

        ExpandScalar(map, expbuf);

        if (strstr(expbuf, "$(this)"))
        {
            DeleteRlist(newlist);
            return (FnCallResult) { FNCALL_FAILURE };
        }

        AppendRlist(&newlist, expbuf, CF_SCALAR);
        DeleteScalar("this", "this");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, CF_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallSelectServers(FnCall *fp, Rlist *finalargs)
 /* ReadTCP(localhost,80,'GET index.html',1000) */
{
    AgentConnection *conn = NULL;
    Rlist *rp, *hostnameip;
    char buffer[CF_BUFSIZE], naked[CF_MAXVARSIZE];
    int val = 0, n_read = 0, count = 0;
    short portnum;
    Attributes attr = { {0} };
    Rval retval;
    Promise *pp;

    buffer[0] = '\0';

/* begin fn specific content */

    char *listvar = ScalarValue(finalargs);
    char *port = ScalarValue(finalargs->next);
    char *sendstring = ScalarValue(finalargs->next->next);
    char *regex = ScalarValue(finalargs->next->next->next);
    char *maxbytes = ScalarValue(finalargs->next->next->next->next);
    char *array_lval = ScalarValue(finalargs->next->next->next->next->next);

    if (*listvar == '@')
    {
        GetNaked(naked, listvar);
    }
    else
    {
        CfOut(cf_verbose, "", "Function selectservers was promised a list called \"%s\" but this was not found\n",
              listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (GetVariable(CONTEXTID, naked, &retval) == cf_notype)
    {
        CfOut(cf_verbose, "",
              "Function selectservers was promised a list called \"%s\" but this was not found from context %s.%s\n",
              listvar, CONTEXTID, naked);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (retval.rtype != CF_LIST)
    {
        CfOut(cf_verbose, "",
              "Function selectservers was promised a list called \"%s\" but this variable is not a list\n", listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    hostnameip = ListRvalValue(retval);
    val = Str2Int(maxbytes);
    portnum = (short) Str2Int(port);

    if (val < 0 || portnum < 0)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (val > CF_BUFSIZE - 1)
    {
        CfOut(cf_error, "", "Too many bytes specificed in selectservers");
        val = CF_BUFSIZE - CF_BUFFERMARGIN;
    }

    if (THIS_AGENT_TYPE != AGENT_TYPE_AGENT)
    {
        snprintf(buffer, CF_MAXVARSIZE - 1, "%d", count);
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
    }

    pp = NewPromise("select_server", "function");

    for (rp = hostnameip; rp != NULL; rp = rp->next)
    {
        CfDebug("Want to read %d bytes from port %d at %s\n", val, portnum, (char *) rp->item);

        conn = NewAgentConn();

        attr.copy.force_ipv4 = false;
        attr.copy.portnumber = portnum;

        if (!ServerConnect(conn, rp->item, attr, pp))
        {
            CfOut(cf_inform, "socket", "Couldn't open a tcp socket");
            DeleteAgentConn(conn);
            continue;
        }

        if (strlen(sendstring) > 0)
        {
            if (SendSocketStream(conn->sd, sendstring, strlen(sendstring), 0) == -1)
            {
                cf_closesocket(conn->sd);
                DeleteAgentConn(conn);
                continue;
            }

            if ((n_read = recv(conn->sd, buffer, val, 0)) == -1)
            {
            }

            if (n_read == -1)
            {
                cf_closesocket(conn->sd);
                DeleteAgentConn(conn);
                continue;
            }

            if (strlen(regex) == 0 || FullTextMatch(regex, buffer))
            {
                CfOut(cf_verbose, "", "Host %s is alive and responding correctly\n", ScalarValue(rp));
                snprintf(buffer, CF_MAXVARSIZE - 1, "%s[%d]", array_lval, count);
                NewScalar(CONTEXTID, buffer, rp->item, cf_str);
                count++;
            }
        }
        else
        {
            CfOut(cf_verbose, "", "Host %s is alive\n", ScalarValue(rp));
            snprintf(buffer, CF_MAXVARSIZE - 1, "%s[%d]", array_lval, count);
            NewScalar(CONTEXTID, buffer, rp->item, cf_str);

            if (IsDefinedClass(CanonifyName(rp->item), fp->namespace))
            {
                CfOut(cf_verbose, "", "This host is in the list and has promised to join the class %s - joined\n",
                      array_lval);
                NewClass(array_lval, fp->namespace);
            }

            count++;
        }

        cf_closesocket(conn->sd);
        DeleteAgentConn(conn);
    }

    DeletePromise(pp);

/* Return the subset that is alive and responding correctly */

/* Return the number of lines in array */

    snprintf(buffer, CF_MAXVARSIZE - 1, "%d", count);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallIsNewerThan(FnCall *fp, Rlist *finalargs)
{
    struct stat frombuf, tobuf;

/* begin fn specific content */

    if (cfstat(ScalarValue(finalargs), &frombuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (cfstat(ScalarValue(finalargs->next), &tobuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE};
    }

    if (frombuf.st_mtime > tobuf.st_mtime)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), CF_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallIsAccessedBefore(FnCall *fp, Rlist *finalargs)
{
    struct stat frombuf, tobuf;

/* begin fn specific content */

    if (cfstat(ScalarValue(finalargs), &frombuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (cfstat(ScalarValue(finalargs->next), &tobuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE};
    }

    if (frombuf.st_atime < tobuf.st_atime)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), CF_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallIsChangedBefore(FnCall *fp, Rlist *finalargs)
{
    struct stat frombuf, tobuf;

/* begin fn specific content */

    if (cfstat(ScalarValue(finalargs), &frombuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else if (cfstat(ScalarValue(finalargs->next), &tobuf) == -1)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (frombuf.st_ctime > tobuf.st_ctime)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), CF_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallFileStat(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], *path = ScalarValue(finalargs);
    struct stat statbuf;

    buffer[0] = '\0';

/* begin fn specific content */

    if (lstat(path, &statbuf) == -1)
    {
        if (!strcmp(fp->name, "filesize"))
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }

        strcpy(buffer, "!any");
    }
    else
    {
        strcpy(buffer, "!any");

        if (!strcmp(fp->name, "isexecutable"))
        {
            if (IsExecutable(path))
            {
                strcpy(buffer, "any");
            }
        }
        else if (!strcmp(fp->name, "isdir"))
        {
            if (S_ISDIR(statbuf.st_mode))
            {
                strcpy(buffer, "any");
            }
        }
        else if (!strcmp(fp->name, "islink"))
        {
            if (S_ISLNK(statbuf.st_mode))
            {
                strcpy(buffer, "any");
            }
        }
        else if (!strcmp(fp->name, "isplain"))
        {
            if (S_ISREG(statbuf.st_mode))
            {
                strcpy(buffer, "any");
            }
        }
        else if (!strcmp(fp->name, "fileexists"))
        {
            strcpy(buffer, "any");
        }
        else if (!strcmp(fp->name, "filesize"))
        {
            snprintf(buffer, CF_MAXVARSIZE, "%jd", (uintmax_t) statbuf.st_size);
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallIPRange(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], *range = ScalarValue(finalargs);
    Item *ip;

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, "!any");

    if (!FuzzyMatchParse(range))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    for (ip = IPADDRESSES; ip != NULL; ip = ip->next)
    {
        CfDebug("Checking IP Range against RDNS %s\n", VIPADDRESS);

        if (FuzzySetMatch(range, VIPADDRESS) == 0)
        {
            CfDebug("IPRange Matched\n");
            strcpy(buffer, "any");
            break;
        }
        else
        {
            CfDebug("Checking IP Range against iface %s\n", ip->name);

            if (FuzzySetMatch(range, ip->name) == 0)
            {
                CfDebug("IPRange Matched\n");
                strcpy(buffer, "any");
                break;
            }
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallHostRange(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *prefix = ScalarValue(finalargs);
    char *range = ScalarValue(finalargs->next);

    strcpy(buffer, "!any");

    if (!FuzzyHostParse(prefix, range))
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (FuzzyHostMatch(prefix, range, VUQNAME) == 0)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), CF_SCALAR } };
    }
}

/*********************************************************************/

FnCallResult FnCallHostInNetgroup(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    char *host, *user, *domain;

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, "!any");

    setnetgrent(ScalarValue(finalargs));

    while (getnetgrent(&host, &user, &domain))
    {
        if (host == NULL)
        {
            CfOut(cf_verbose, "", "Matched %s in netgroup %s\n", VFQNAME, ScalarValue(finalargs));
            strcpy(buffer, "any");
            break;
        }

        if (strcmp(host, VFQNAME) == 0 || strcmp(host, VUQNAME) == 0)
        {
            CfOut(cf_verbose, "", "Matched %s in netgroup %s\n", host, ScalarValue(finalargs));
            strcpy(buffer, "any");
            break;
        }
    }

    endnetgrent();

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallIsVariable(FnCall *fp, Rlist *finalargs)
{
    if (DefinedVariable(ScalarValue(finalargs)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), CF_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallStrCmp(FnCall *fp, Rlist *finalargs)
{
    if (strcmp(ScalarValue(finalargs), ScalarValue(finalargs->next)) == 0)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), CF_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallTranslatePath(FnCall *fp, Rlist *finalargs)
{
    char buffer[MAX_FILENAME];

    buffer[0] = '\0';

/* begin fn specific content */

    snprintf(buffer, sizeof(buffer), "%s", ScalarValue(finalargs));
    MapName(buffer);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRegistryValue(FnCall *fp, Rlist *finalargs)
{
/* begin fn specific content */

#if defined(__MINGW32__)
    char buffer[CF_BUFSIZE] = "";

    if (GetRegistryValue(ScalarValue(finalargs), ScalarValue(finalargs->next), buffer, sizeof(buffer)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
#else
    return (FnCallResult) { FNCALL_FAILURE };
#endif
}

/*********************************************************************/

static FnCallResult FnCallRemoteScalar(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *handle = ScalarValue(finalargs);
    char *server = ScalarValue(finalargs->next);
    int encrypted = GetBoolean(ScalarValue(finalargs->next->next));

    if (strcmp(server, "localhost") == 0)
    {
        /* The only reason for this is testing... */
        server = "127.0.0.1";
    }

    if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("<remote scalar>"), CF_SCALAR } };
    }
    else
    {
        GetRemoteScalar("VAR", handle, server, encrypted, buffer);

        if (strncmp(buffer, "BAD:", 4) == 0)
        {
            if (!RetrieveUnreliableValue("remotescalar", handle, buffer))
            {
                // This function should never fail
                buffer[0] = '\0';
            }
        }
        else
        {
            CacheUnreliableValue("remotescalar", handle, buffer);
        }

        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallHubKnowledge(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *handle = ScalarValue(finalargs);

    if (THIS_AGENT_TYPE != AGENT_TYPE_AGENT)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("<inaccessible remote scalar>"), CF_SCALAR } };
    }
    else
    {
        CfOut(cf_verbose, "", " -> Accessing hub knowledge bank for \"%s\"", handle);
        GetRemoteScalar("VAR", handle, POLICY_SERVER, true, buffer);

        // This should always be successful - and this one doesn't cache

        if (strncmp(buffer, "BAD:", 4) == 0)
        {
            snprintf(buffer, CF_MAXVARSIZE, "0");
        }

        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallRemoteClassesMatching(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *classlist;
    char buffer[CF_BUFSIZE], class[CF_MAXVARSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    char *regex = ScalarValue(finalargs);
    char *server = ScalarValue(finalargs->next);
    int encrypted = GetBoolean(ScalarValue(finalargs->next->next));
    char *prefix = ScalarValue(finalargs->next->next->next);

    if (strcmp(server, "localhost") == 0)
    {
        /* The only reason for this is testing... */
        server = "127.0.0.1";
    }

    if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("remote_classes"), CF_SCALAR } };
    }
    else
    {
        GetRemoteScalar("CONTEXT", regex, server, encrypted, buffer);

        if (strncmp(buffer, "BAD:", 4) == 0)
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }

        if ((classlist = SplitStringAsRList(buffer, ',')))
        {
            for (rp = classlist; rp != NULL; rp = rp->next)
            {
                snprintf(class, CF_MAXVARSIZE - 1, "%s_%s", prefix, (char *) rp->item);
                NewBundleClass(class, THIS_BUNDLE, fp->namespace);
            }
            DeleteRlist(classlist);
        }

        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
    }
}

/*********************************************************************/

static FnCallResult FnCallPeers(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist, *pruned;
    char *split = "\n";
    char *file_buffer = NULL;
    int i, found, maxent = 100000, maxsize = 100000;

/* begin fn specific content */

    char *filename = ScalarValue(finalargs);
    char *comment = ScalarValue(finalargs->next);
    int groupsize = Str2Int(ScalarValue(finalargs->next->next));

    file_buffer = (char *) CfReadFile(filename, maxsize);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    file_buffer = StripPatterns(file_buffer, comment, filename);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { NULL, CF_LIST } };
    }
    else
    {
        newlist = SplitRegexAsRList(file_buffer, split, maxent, true);
    }

/* Slice up the list and discard everything except our slice */

    i = 0;
    found = false;
    pruned = NULL;

    for (rp = newlist; rp != NULL; rp = rp->next)
    {
        char s[CF_MAXVARSIZE];

        if (EmptyString(rp->item))
        {
            continue;
        }

        s[0] = '\0';
        sscanf(rp->item, "%s", s);

        if (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0)
        {
            found = true;
        }
        else
        {
            PrependRScalar(&pruned, s, CF_SCALAR);
        }

        if (i++ % groupsize == groupsize - 1)
        {
            if (found)
            {
                break;
            }
            else
            {
                DeleteRlist(pruned);
                pruned = NULL;
            }
        }
    }

    DeleteRlist(newlist);
    free(file_buffer);

    if (pruned)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { pruned, CF_LIST } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
}

/*********************************************************************/

static FnCallResult FnCallPeerLeader(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist;
    char *split = "\n";
    char *file_buffer = NULL, buffer[CF_MAXVARSIZE];
    int i, found, maxent = 100000, maxsize = 100000;

    buffer[0] = '\0';

/* begin fn specific content */

    char *filename = ScalarValue(finalargs);
    char *comment = ScalarValue(finalargs->next);
    int groupsize = Str2Int(ScalarValue(finalargs->next->next));

    file_buffer = (char *) CfReadFile(filename, maxsize);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else
    {
        file_buffer = StripPatterns(file_buffer, comment, filename);

        if (file_buffer == NULL)
        {
            return (FnCallResult) { FNCALL_SUCCESS, { NULL, CF_LIST } };
        }
        else
        {
            newlist = SplitRegexAsRList(file_buffer, split, maxent, true);
        }
    }

/* Slice up the list and discard everything except our slice */

    i = 0;
    found = false;
    buffer[0] = '\0';

    for (rp = newlist; rp != NULL; rp = rp->next)
    {
        char s[CF_MAXVARSIZE];

        if (EmptyString(rp->item))
        {
            continue;
        }

        s[0] = '\0';
        sscanf(rp->item, "%s", s);

        if (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0)
        {
            found = true;
        }

        if (i % groupsize == 0)
        {
            if (found)
            {
                if (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0)
                {
                    strncpy(buffer, "localhost", CF_MAXVARSIZE - 1);
                }
                else
                {
                    strncpy(buffer, s, CF_MAXVARSIZE - 1);
                }
                break;
            }
        }

        i++;
    }

    DeleteRlist(newlist);
    free(file_buffer);

    if (strlen(buffer) > 0)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

/*********************************************************************/

static FnCallResult FnCallPeerLeaders(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *newlist, *pruned;
    char *split = "\n";
    char *file_buffer = NULL;
    int i, maxent = 100000, maxsize = 100000;

/* begin fn specific content */

    char *filename = ScalarValue(finalargs);
    char *comment = ScalarValue(finalargs->next);
    int groupsize = Str2Int(ScalarValue(finalargs->next->next));

    file_buffer = (char *) CfReadFile(filename, maxsize);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    file_buffer = StripPatterns(file_buffer, comment, filename);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { NULL, CF_LIST } };
    }

    newlist = SplitRegexAsRList(file_buffer, split, maxent, true);

/* Slice up the list and discard everything except our slice */

    i = 0;
    pruned = NULL;

    for (rp = newlist; rp != NULL; rp = rp->next)
    {
        char s[CF_MAXVARSIZE];

        if (EmptyString(rp->item))
        {
            continue;
        }

        s[0] = '\0';
        sscanf(rp->item, "%s", s);

        if (i % groupsize == 0)
        {
            if (strcmp(s, VFQNAME) == 0 || strcmp(s, VUQNAME) == 0)
            {
                PrependRScalar(&pruned, "localhost", CF_SCALAR);
            }
            else
            {
                PrependRScalar(&pruned, s, CF_SCALAR);
            }
        }

        i++;
    }

    DeleteRlist(newlist);
    free(file_buffer);

    if (pruned)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { pruned, CF_LIST } };
    }
    else
    {
        free(file_buffer);
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

/*********************************************************************/

static FnCallResult FnCallRegCmp(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, CF_ANYCLASS);
    char *argv0 = ScalarValue(finalargs);
    char *argv1 = ScalarValue(finalargs->next);

    if (FullTextMatch(argv0, argv1))
    {
        strcpy(buffer, "any");
    }
    else
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRegExtract(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    Scope *ptr;

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, CF_ANYCLASS);
    char *regex = ScalarValue(finalargs);
    char *data = ScalarValue(finalargs->next);
    char *arrayname = ScalarValue(finalargs->next->next);

    if (FullTextMatch(regex, data))
    {
        strcpy(buffer, "any");
    }
    else
    {
        strcpy(buffer, "!any");
    }

    ptr = GetScope("match");

    if (ptr && ptr->hashtable)
    {
        HashIterator i = HashIteratorInit(ptr->hashtable);
        CfAssoc *assoc;

        while ((assoc = HashIteratorNext(&i)))
        {
            char var[CF_MAXVARSIZE];

            if (assoc->rval.rtype != CF_SCALAR)
            {
                CfOut(cf_error, "",
                      " !! Software error: pattern match was non-scalar in regextract (shouldn't happen)");
                return (FnCallResult) { FNCALL_FAILURE };
            }
            else
            {
                snprintf(var, CF_MAXVARSIZE - 1, "%s[%s]", arrayname, assoc->lval);
                NewScalar(THIS_BUNDLE, var, assoc->rval.item, cf_str);
            }
        }
    }
    else
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRegLine(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], line[CF_BUFSIZE];
    FILE *fin;

    buffer[0] = '\0';

/* begin fn specific content */

    char *argv0 = ScalarValue(finalargs);
    char *argv1 = ScalarValue(finalargs->next);

    strcpy(buffer, "!any");

    if ((fin = fopen(argv1, "r")) == NULL)
    {
        strcpy(buffer, "!any");
    }
    else
    {
        while (!feof(fin))
        {
            line[0] = '\0';
            fgets(line, CF_BUFSIZE - 1, fin);
            Chop(line);

            if (FullTextMatch(argv0, line))
            {
                strcpy(buffer, "any");
                break;
            }
        }

        fclose(fin);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallIsLessGreaterThan(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];

    buffer[0] = '\0';

    char *argv0 = ScalarValue(finalargs);
    char *argv1 = ScalarValue(finalargs->next);

    if (IsRealNumber(argv0) && IsRealNumber(argv1))
    {
        double a = Str2Double(argv0);
        double b = Str2Double(argv1);

        if (a == CF_NODOUBLE || b == CF_NODOUBLE)
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }

        CfDebug("%s and %s are numerical\n", argv0, argv1);

        if (!strcmp(fp->name, "isgreaterthan"))
        {
            if (a > b)
            {
                strcpy(buffer, "any");
            }
            else
            {
                strcpy(buffer, "!any");
            }
        }
        else
        {
            if (a < b)
            {
                strcpy(buffer, "any");
            }
            else
            {
                strcpy(buffer, "!any");
            }
        }
    }
    else if (strcmp(argv0, argv1) > 0)
    {
        CfDebug("%s and %s are NOT numerical\n", argv0, argv1);

        if (!strcmp(fp->name, "isgreaterthan"))
        {
            strcpy(buffer, "any");
        }
        else
        {
            strcpy(buffer, "!any");
        }
    }
    else
    {
        if (!strcmp(fp->name, "isgreaterthan"))
        {
            strcpy(buffer, "!any");
        }
        else
        {
            strcpy(buffer, "any");
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallIRange(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    long tmp;

    buffer[0] = '\0';

/* begin fn specific content */

    long from = Str2Int(ScalarValue(finalargs));
    long to = Str2Int(ScalarValue(finalargs->next));

    if (from == CF_NOINT || to == CF_NOINT)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (from == CF_NOINT || to == CF_NOINT)
    {
        snprintf(buffer, CF_BUFSIZE, "Error reading assumed int values %s=>%ld,%s=>%ld\n", (char *) (finalargs->item),
                 from, (char *) (finalargs->next->item), to);
        ReportError(buffer);
    }

    if (from > to)
    {
        tmp = to;
        to = from;
        from = tmp;
    }

    snprintf(buffer, CF_BUFSIZE - 1, "%ld,%ld", from, to);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallRRange(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    int tmp;

    buffer[0] = '\0';

/* begin fn specific content */

    double from = Str2Double(ScalarValue(finalargs));
    double to = Str2Double(ScalarValue(finalargs->next));

    if (from == CF_NODOUBLE || to == CF_NODOUBLE)
    {
        snprintf(buffer, CF_BUFSIZE, "Error reading assumed real values %s=>%lf,%s=>%lf\n", (char *) (finalargs->item),
                 from, (char *) (finalargs->next->item), to);
        ReportError(buffer);
    }

    if (from > to)
    {
        tmp = to;
        to = from;
        from = tmp;
    }

    snprintf(buffer, CF_BUFSIZE - 1, "%lf,%lf", from, to);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallOn(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp;
    char buffer[CF_BUFSIZE];
    long d[6];
    time_t cftime;
    struct tm tmv;
    enum cfdatetemplate i;

    buffer[0] = '\0';

/* begin fn specific content */

    rp = finalargs;

    for (i = 0; i < 6; i++)
    {
        if (rp != NULL)
        {
            d[i] = Str2Int(ScalarValue(rp));
            rp = rp->next;
        }
    }

/* (year,month,day,hour,minutes,seconds) */

    tmv.tm_year = d[cfa_year] - 1900;
    tmv.tm_mon = d[cfa_month] - 1;
    tmv.tm_mday = d[cfa_day];
    tmv.tm_hour = d[cfa_hour];
    tmv.tm_min = d[cfa_min];
    tmv.tm_sec = d[cfa_sec];
    tmv.tm_isdst = -1;

    if ((cftime = mktime(&tmv)) == -1)
    {
        CfOut(cf_inform, "", "Illegal time value");
    }

    CfDebug("Time computed from input was: %s\n", cf_ctime(&cftime));

    snprintf(buffer, CF_BUFSIZE - 1, "%ld", cftime);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallOr(FnCall *fp, Rlist *finalargs)
{
    Rlist *arg;
    char id[CF_BUFSIZE];

    snprintf(id, CF_BUFSIZE, "built-in FnCall or-arg");

/* We need to check all the arguments, ArgTemplate does not check varadic functions */
    for (arg = finalargs; arg; arg = arg->next)
    {
        CheckConstraintTypeMatch(id, (Rval) {arg->item, arg->type}, cf_str, "", 1);
    }

    for (arg = finalargs; arg; arg = arg->next)
    {
        if (IsDefinedClass(ScalarValue(arg), fp->namespace))
        {
            return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("any"), CF_SCALAR } };
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup("!any"), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallLaterThan(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp;
    char buffer[CF_BUFSIZE];
    long d[6];
    time_t cftime, now = time(NULL);
    struct tm tmv;
    enum cfdatetemplate i;

    buffer[0] = '\0';

/* begin fn specific content */

    rp = finalargs;

    for (i = 0; i < 6; i++)
    {
        if (rp != NULL)
        {
            d[i] = Str2Int(ScalarValue(rp));
            rp = rp->next;
        }
    }

/* (year,month,day,hour,minutes,seconds) */

    tmv.tm_year = d[cfa_year] - 1900;
    tmv.tm_mon = d[cfa_month] - 1;
    tmv.tm_mday = d[cfa_day];
    tmv.tm_hour = d[cfa_hour];
    tmv.tm_min = d[cfa_min];
    tmv.tm_sec = d[cfa_sec];
    tmv.tm_isdst = -1;

    if ((cftime = mktime(&tmv)) == -1)
    {
        CfOut(cf_inform, "", "Illegal time value");
    }

    CfDebug("Time computed from input was: %s\n", cf_ctime(&cftime));

    if (now > cftime)
    {
        strcpy(buffer, CF_ANYCLASS);
    }
    else
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallAgoDate(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp;
    char buffer[CF_BUFSIZE];
    time_t cftime;
    long d[6];
    enum cfdatetemplate i;

    buffer[0] = '\0';

/* begin fn specific content */

    rp = finalargs;

    for (i = 0; i < 6; i++)
    {
        if (rp != NULL)
        {
            d[i] = Str2Int(ScalarValue(rp));
            rp = rp->next;
        }
    }

/* (year,month,day,hour,minutes,seconds) */

    cftime = CFSTARTTIME;
    cftime -= d[cfa_sec];
    cftime -= d[cfa_min] * 60;
    cftime -= d[cfa_hour] * 3600;
    cftime -= d[cfa_day] * 24 * 3600;
    cftime -= Months2Seconds(d[cfa_month]);
    cftime -= d[cfa_year] * 365 * 24 * 3600;

    CfDebug("Total negative offset = %.1f minutes\n", (double) (CFSTARTTIME - cftime) / 60.0);
    CfDebug("Time computed from input was: %s\n", cf_ctime(&cftime));

    snprintf(buffer, CF_BUFSIZE - 1, "%ld", cftime);

    if (cftime < 0)
    {
        CfDebug("AGO overflowed, truncating at zero\n");
        strcpy(buffer, "0");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallAccumulatedDate(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp;
    char buffer[CF_BUFSIZE];
    long d[6], cftime;
    enum cfdatetemplate i;

    buffer[0] = '\0';

/* begin fn specific content */

    rp = finalargs;

    for (i = 0; i < 6; i++)
    {
        if (rp != NULL)
        {
            d[i] = Str2Int(ScalarValue(rp));
            rp = rp->next;
        }
    }

/* (year,month,day,hour,minutes,seconds) */

    cftime = 0;
    cftime += d[cfa_sec];
    cftime += d[cfa_min] * 60;
    cftime += d[cfa_hour] * 3600;
    cftime += d[cfa_day] * 24 * 3600;
    cftime += d[cfa_month] * 30 * 24 * 3600;
    cftime += d[cfa_year] * 365 * 24 * 3600;

    snprintf(buffer, CF_BUFSIZE - 1, "%ld", cftime);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallNot(FnCall *fp, Rlist *finalargs)
{
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(IsDefinedClass(ScalarValue(finalargs), fp->namespace) ? "!any" : "any"), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallNow(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    time_t cftime;

    buffer[0] = '\0';

/* begin fn specific content */

    cftime = CFSTARTTIME;

    CfDebug("Time computed from input was: %s\n", cf_ctime(&cftime));

    snprintf(buffer, CF_BUFSIZE - 1, "%ld", (long) cftime);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/
/* Read functions                                                    */
/*********************************************************************/

static FnCallResult FnCallReadFile(FnCall *fp, Rlist *finalargs)
{
    char *contents;

/* begin fn specific content */

    char *filename = ScalarValue(finalargs);
    int maxsize = Str2Int(ScalarValue(finalargs->next));

// Read once to validate structure of file in itemlist

    CfDebug("Read string data from file %s (up to %d)\n", filename, maxsize);

    contents = CfReadFile(filename, maxsize);

    if (contents)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { contents, CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
}

/*********************************************************************/

static FnCallResult ReadList(FnCall *fp, Rlist *finalargs, enum cfdatatype type)
{
    Rlist *rp, *newlist = NULL;
    char fnname[CF_MAXVARSIZE], *file_buffer = NULL;
    int noerrors = true, blanks = false;

/* begin fn specific content */

    /* 5args: filename,comment_regex,split_regex,max number of entries,maxfilesize  */

    char *filename = ScalarValue(finalargs);
    char *comment = ScalarValue(finalargs->next);
    char *split = ScalarValue(finalargs->next->next);
    int maxent = Str2Int(ScalarValue(finalargs->next->next->next));
    int maxsize = Str2Int(ScalarValue(finalargs->next->next->next->next));

// Read once to validate structure of file in itemlist

    CfDebug("Read string data from file %s\n", filename);
    snprintf(fnname, CF_MAXVARSIZE - 1, "read%slist", CF_DATATYPES[type]);

    file_buffer = (char *) CfReadFile(filename, maxsize);

    if (file_buffer == NULL)
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
    else
    {
        file_buffer = StripPatterns(file_buffer, comment, filename);

        if (file_buffer == NULL)
        {
            return (FnCallResult) { FNCALL_SUCCESS, { NULL, CF_LIST } };
        }
        else
        {
            newlist = SplitRegexAsRList(file_buffer, split, maxent, blanks);
        }
    }

    switch (type)
    {
    case cf_str:
        break;

    case cf_int:
        for (rp = newlist; rp != NULL; rp = rp->next)
        {
            if (Str2Int(ScalarValue(rp)) == CF_NOINT)
            {
                CfOut(cf_error, "", "Presumed int value \"%s\" read from file %s has no recognizable value",
                      ScalarValue(rp), filename);
                noerrors = false;
            }
        }
        break;

    case cf_real:
        for (rp = newlist; rp != NULL; rp = rp->next)
        {
            if (Str2Double(ScalarValue(rp)) == CF_NODOUBLE)
            {
                CfOut(cf_error, "", "Presumed real value \"%s\" read from file %s has no recognizable value",
                      ScalarValue(rp), filename);
                noerrors = false;
            }
        }
        break;

    default:
        FatalError("Software error readstringlist - abused type");
    }

    free(file_buffer);

    if (newlist && noerrors)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newlist, CF_LIST } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

static FnCallResult FnCallReadStringList(FnCall *fp, Rlist *args)
{
    return ReadList(fp, args, cf_str);
}

static FnCallResult FnCallReadIntList(FnCall *fp, Rlist *args)
{
    return ReadList(fp, args, cf_int);
}

static FnCallResult FnCallReadRealList(FnCall *fp, Rlist *args)
{
    return ReadList(fp, args, cf_real);
}

/*********************************************************************/

static FnCallResult ReadArray(FnCall *fp, Rlist *finalargs, enum cfdatatype type, int intIndex)
/* lval,filename,separator,comment,Max number of bytes  */
{
    char fnname[CF_MAXVARSIZE], *file_buffer = NULL;
    int entries = 0;

    /* Arg validation */

    if (intIndex)
    {
        snprintf(fnname, CF_MAXVARSIZE - 1, "read%sarrayidx", CF_DATATYPES[type]);
    }
    else
    {
        snprintf(fnname, CF_MAXVARSIZE - 1, "read%sarray", CF_DATATYPES[type]);
    }

/* begin fn specific content */

    /* 6 args: array_lval,filename,comment_regex,split_regex,max number of entries,maxfilesize  */

    char *array_lval = ScalarValue(finalargs);
    char *filename = ScalarValue(finalargs->next);
    char *comment = ScalarValue(finalargs->next->next);
    char *split = ScalarValue(finalargs->next->next->next);
    int maxent = Str2Int(ScalarValue(finalargs->next->next->next->next));
    int maxsize = Str2Int(ScalarValue(finalargs->next->next->next->next->next));

// Read once to validate structure of file in itemlist

    CfDebug("Read string data from file %s - , maxent %d, maxsize %d\n", filename, maxent, maxsize);

    file_buffer = (char *) CfReadFile(filename, maxsize);

    CfDebug("FILE: %s\n", file_buffer);

    if (file_buffer == NULL)
    {
        entries = 0;
    }
    else
    {
        file_buffer = StripPatterns(file_buffer, comment, filename);

        if (file_buffer == NULL)
        {
            entries = 0;
        }
        else
        {
            entries = BuildLineArray(array_lval, file_buffer, split, maxent, type, intIndex);
        }
    }

    switch (type)
    {
    case cf_str:
    case cf_int:
    case cf_real:
        break;

    default:
        FatalError("Software error readstringarray - abused type");
    }

    free(file_buffer);

/* Return the number of lines in array */

    snprintf(fnname, CF_MAXVARSIZE - 1, "%d", entries);
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(fnname), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallReadStringArray(FnCall *fp, Rlist *args)
{
    return ReadArray(fp, args, cf_str, false);
}

/*********************************************************************/

static FnCallResult FnCallReadStringArrayIndex(FnCall *fp, Rlist *args)
{
    return ReadArray(fp, args, cf_str, true);
}

/*********************************************************************/

static FnCallResult FnCallReadIntArray(FnCall *fp, Rlist *args)
{
    return ReadArray(fp, args, cf_int, false);
}

/*********************************************************************/

static FnCallResult FnCallReadRealArray(FnCall *fp, Rlist *args)
{
    return ReadArray(fp, args, cf_real, false);
}

/*********************************************************************/

static FnCallResult ParseArray(FnCall *fp, Rlist *finalargs, enum cfdatatype type, int intIndex)
/* lval,filename,separator,comment,Max number of bytes  */
{
    char fnname[CF_MAXVARSIZE];
    int entries = 0;

    /* Arg validation */

    if (intIndex)
    {
        snprintf(fnname, CF_MAXVARSIZE - 1, "read%sarrayidx", CF_DATATYPES[type]);
    }
    else
    {
        snprintf(fnname, CF_MAXVARSIZE - 1, "read%sarray", CF_DATATYPES[type]);
    }

/* begin fn specific content */

    /* 6 args: array_lval,instring,comment_regex,split_regex,max number of entries,maxfilesize  */

    char *array_lval = ScalarValue(finalargs);
    char *instring = xstrdup(ScalarValue(finalargs->next));
    char *comment = ScalarValue(finalargs->next->next);
    char *split = ScalarValue(finalargs->next->next->next);
    int maxent = Str2Int(ScalarValue(finalargs->next->next->next->next));
    int maxsize = Str2Int(ScalarValue(finalargs->next->next->next->next->next));

// Read once to validate structure of file in itemlist

    CfDebug("Parse string data from string %s - , maxent %d, maxsize %d\n", instring, maxent, maxsize);

    if (instring == NULL)
    {
        entries = 0;
    }
    else
    {
        instring = StripPatterns(instring, comment, "string argument 2");

        if (instring == NULL)
        {
            entries = 0;
        }
        else
        {
            entries = BuildLineArray(array_lval, instring, split, maxent, type, intIndex);
        }
    }

    switch (type)
    {
    case cf_str:
    case cf_int:
    case cf_real:
        break;

    default:
        FatalError("Software error parsestringarray - abused type");
    }

    free(instring);

/* Return the number of lines in array */

    snprintf(fnname, CF_MAXVARSIZE - 1, "%d", entries);
    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(fnname), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallParseStringArray(FnCall *fp, Rlist *args)
{
    return ParseArray(fp, args, cf_str, false);
}

/*********************************************************************/

static FnCallResult FnCallParseStringArrayIndex(FnCall *fp, Rlist *args)
{
    return ParseArray(fp, args, cf_str, true);
}

/*********************************************************************/

static FnCallResult FnCallParseIntArray(FnCall *fp, Rlist *args)
{
    return ParseArray(fp, args, cf_int, false);
}

/*********************************************************************/

static FnCallResult FnCallParseRealArray(FnCall *fp, Rlist *args)
{
    return ParseArray(fp, args, cf_real, false);
}

/*********************************************************************/

static FnCallResult FnCallSplitString(FnCall *fp, Rlist *finalargs)
{
    Rlist *newlist = NULL;

/* begin fn specific content */

    /* 2args: string,split_regex,max  */

    char *string = ScalarValue(finalargs);
    char *split = ScalarValue(finalargs->next);
    int max = Str2Int(ScalarValue(finalargs->next->next));

// Read once to validate structure of file in itemlist

    newlist = SplitRegexAsRList(string, split, max, true);

    if (newlist == NULL)
    {
        PrependRScalar(&newlist, "cf_null", CF_SCALAR);
    }

    return (FnCallResult) { FNCALL_SUCCESS, { newlist, CF_LIST } };
}

/*********************************************************************/

static FnCallResult FnCallFileSexist(FnCall *fp, Rlist *finalargs)
{
    Rlist *rp, *files;
    char buffer[CF_BUFSIZE], naked[CF_MAXVARSIZE];
    Rval retval;
    struct stat sb;

    buffer[0] = '\0';

/* begin fn specific content */

    char *listvar = ScalarValue(finalargs);

    if (*listvar == '@')
    {
        GetNaked(naked, listvar);
    }
    else
    {
        CfOut(cf_verbose, "", "Function filesexist was promised a list called \"%s\" but this was not found\n",
              listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (GetVariable(CONTEXTID, naked, &retval) == cf_notype)
    {
        CfOut(cf_verbose, "", "Function filesexist was promised a list called \"%s\" but this was not found\n",
              listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    if (retval.rtype != CF_LIST)
    {
        CfOut(cf_verbose, "", "Function filesexist was promised a list called \"%s\" but this variable is not a list\n",
              listvar);
        return (FnCallResult) { FNCALL_FAILURE };
    }

    files = (Rlist *) retval.item;

    strcpy(buffer, "any");

    for (rp = files; rp != NULL; rp = rp->next)
    {
        if (cfstat(rp->item, &sb) == -1)
        {
            strcpy(buffer, "!any");
            break;
        }
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/
/* LDAP Nova features                                                */
/*********************************************************************/

static FnCallResult FnCallLDAPValue(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE], handle[CF_BUFSIZE];
    void *newval = NULL;

/* begin fn specific content */

    char *uri = ScalarValue(finalargs);
    char *dn = ScalarValue(finalargs->next);
    char *filter = ScalarValue(finalargs->next->next);
    char *name = ScalarValue(finalargs->next->next->next);
    char *scope = ScalarValue(finalargs->next->next->next->next);
    char *sec = ScalarValue(finalargs->next->next->next->next->next);

    snprintf(handle, CF_BUFSIZE, "%s_%s_%s_%s", dn, filter, name, scope);

    if ((newval = CfLDAPValue(uri, dn, filter, name, scope, sec)))
    {
        CacheUnreliableValue("ldapvalue", handle, newval);
    }
    else
    {
        if (RetrieveUnreliableValue("ldapvalue", handle, buffer))
        {
            newval = xstrdup(buffer);
        }
    }

    if (newval)
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newval, CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }
}

/*********************************************************************/

static FnCallResult FnCallLDAPArray(FnCall *fp, Rlist *finalargs)
{
    void *newval;

/* begin fn specific content */

    char *array = ScalarValue(finalargs);
    char *uri = ScalarValue(finalargs->next);
    char *dn = ScalarValue(finalargs->next->next);
    char *filter = ScalarValue(finalargs->next->next->next);
    char *scope = ScalarValue(finalargs->next->next->next->next);
    char *sec = ScalarValue(finalargs->next->next->next->next->next);

    if ((newval = CfLDAPArray(array, uri, dn, filter, scope, sec)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newval, CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

/*********************************************************************/

static FnCallResult FnCallLDAPList(FnCall *fp, Rlist *finalargs)
{
    void *newval;

/* begin fn specific content */

    char *uri = ScalarValue(finalargs);
    char *dn = ScalarValue(finalargs->next);
    char *filter = ScalarValue(finalargs->next->next);
    char *name = ScalarValue(finalargs->next->next->next);
    char *scope = ScalarValue(finalargs->next->next->next->next);
    char *sec = ScalarValue(finalargs->next->next->next->next->next);

    if ((newval = CfLDAPList(uri, dn, filter, name, scope, sec)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newval, CF_LIST } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

/*********************************************************************/

static FnCallResult FnCallRegLDAP(FnCall *fp, Rlist *finalargs)
{
    void *newval;

/* begin fn specific content */

    char *uri = ScalarValue(finalargs);
    char *dn = ScalarValue(finalargs->next);
    char *filter = ScalarValue(finalargs->next->next);
    char *name = ScalarValue(finalargs->next->next->next);
    char *scope = ScalarValue(finalargs->next->next->next->next);
    char *regex = ScalarValue(finalargs->next->next->next->next->next);
    char *sec = ScalarValue(finalargs->next->next->next->next->next->next);

    if ((newval = CfRegLDAP(uri, dn, filter, name, scope, regex, sec)))
    {
        return (FnCallResult) { FNCALL_SUCCESS, { newval, CF_SCALAR } };
    }
    else
    {
        return (FnCallResult) { FNCALL_FAILURE };
    }

}

/*********************************************************************/

#define KILOBYTE 1024

static FnCallResult FnCallDiskFree(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    off_t df;

    buffer[0] = '\0';

    df = GetDiskUsage(ScalarValue(finalargs), cfabs);

    if (df == CF_INFINITY)
    {
        df = 0;
    }

    /* Result is in kilobytes */
    snprintf(buffer, CF_BUFSIZE - 1, "%jd", ((intmax_t) df) / KILOBYTE);

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

#if !defined(__MINGW32__)
static FnCallResult FnCallUserExists(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    struct passwd *pw;
    uid_t uid = CF_SAME_OWNER;
    char *arg = ScalarValue(finalargs);

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, CF_ANYCLASS);

    if (IsNumber(arg))
    {
        uid = Str2Uid(arg, NULL, NULL);

        if (uid == CF_SAME_OWNER || uid == CF_UNKNOWN_OWNER)
        {
            return (FnCallResult){ FNCALL_FAILURE };
        }

        if ((pw = getpwuid(uid)) == NULL)
        {
            strcpy(buffer, "!any");
        }
    }
    else if ((pw = getpwnam(arg)) == NULL)
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

/*********************************************************************/

static FnCallResult FnCallGroupExists(FnCall *fp, Rlist *finalargs)
{
    char buffer[CF_BUFSIZE];
    struct group *gr;
    gid_t gid = CF_SAME_GROUP;
    char *arg = ScalarValue(finalargs);

    buffer[0] = '\0';

/* begin fn specific content */

    strcpy(buffer, CF_ANYCLASS);

    if (isdigit((int) *arg))
    {
        gid = Str2Gid(arg, NULL, NULL);

        if (gid == CF_SAME_GROUP || gid == CF_UNKNOWN_GROUP)
        {
            return (FnCallResult) { FNCALL_FAILURE };
        }

        if ((gr = getgrgid(gid)) == NULL)
        {
            strcpy(buffer, "!any");
        }
    }
    else if ((gr = getgrnam(arg)) == NULL)
    {
        strcpy(buffer, "!any");
    }

    return (FnCallResult) { FNCALL_SUCCESS, { xstrdup(buffer), CF_SCALAR } };
}

#endif /* !defined(__MINGW32__) */

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static void *CfReadFile(char *filename, int maxsize)
{
    struct stat sb;
    char *result = NULL;
    FILE *fp;
    size_t size;
    int i, newlines = 0;

    if (cfstat(filename, &sb) == -1)
    {
        if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
        {
            CfDebug("Could not examine file %s in readfile on this system", filename);
        }
        else
        {
            if (IsCf3VarString(filename))
            {
                CfOut(cf_verbose, "", "Cannot converge/reduce variable \"%s\" yet .. assuming it will resolve later",
                      filename);
            }
            else
            {
                CfOut(cf_inform, "stat", " !! Could not examine file \"%s\" in readfile", filename);
            }
        }
        return NULL;
    }

    if (sb.st_size > maxsize)
    {
        CfOut(cf_inform, "", "Truncating long file %s in readfile to max limit %d", filename, maxsize);
        size = maxsize;
    }
    else
    {
        size = sb.st_size;
    }

    result = xmalloc(size + 1);

    if ((fp = fopen(filename, "r")) == NULL)
    {
        CfOut(cf_verbose, "fopen", "Could not open file \"%s\" in readfile", filename);
        free(result);
        return NULL;
    }

    result[size] = '\0';

    if (size > 0)
    {
        if (fread(result, size, 1, fp) != 1)
        {
            CfOut(cf_verbose, "fread", "Could not read expected amount from file %s in readfile", filename);
            fclose(fp);
            free(result);
            return NULL;
        }

        for (i = 0; i < size - 1; i++)
        {
            if (result[i] == '\n' || result[i] == '\r')
            {
                newlines++;
            }
        }

        if (newlines == 0 && (result[size - 1] == '\n' || result[size - 1] == '\r'))
        {
            result[size - 1] = '\0';
        }
    }

    fclose(fp);
    return (void *) result;
}

/*********************************************************************/

static char *StripPatterns(char *file_buffer, char *pattern, char *filename)
{
    int start, end;
    int count = 0;

    if (!NULL_OR_EMPTY(pattern))
    {
        while (BlockTextMatch(pattern, file_buffer, &start, &end))
        {
            CloseStringHole(file_buffer, start, end);

            if (count++ > strlen(file_buffer))
            {
                CfOut(cf_error, "",
                      " !! Comment regex \"%s\" was irreconcilable reading input \"%s\" probably because it legally matches nothing",
                      pattern, filename);
                return file_buffer;
            }
        }
    }

    return file_buffer;
}

/*********************************************************************/

static void CloseStringHole(char *s, int start, int end)
{
    int off = end - start;
    char *sp;

    if (off <= 0)
    {
        return;
    }

    for (sp = s + start; *(sp + off) != '\0'; sp++)
    {
        *sp = *(sp + off);
    }

    *sp = '\0';
}

/*********************************************************************/

static int BuildLineArray(char *array_lval, char *file_buffer, char *split, int maxent, enum cfdatatype type,
                          int intIndex)
{
    char *sp, linebuf[CF_BUFSIZE], name[CF_MAXVARSIZE], first_one[CF_MAXVARSIZE];
    Rlist *rp, *newlist = NULL;
    int allowblanks = true, vcount, hcount, lcount = 0;
    int lineLen;

    memset(linebuf, 0, CF_BUFSIZE);
    hcount = 0;

    for (sp = file_buffer; hcount < maxent && *sp != '\0'; sp++)
    {
        linebuf[0] = '\0';
        sscanf(sp, "%1023[^\n]", linebuf);

        lineLen = strlen(linebuf);

        if (lineLen == 0)
        {
            continue;
        }
        else if (lineLen == 1 && linebuf[0] == '\r')
        {
            continue;
        }

        if (linebuf[lineLen - 1] == '\r')
        {
            linebuf[lineLen - 1] = '\0';
        }

        if (lcount++ > CF_HASHTABLESIZE)
        {
            CfOut(cf_error, "", " !! Array is too big to be read into Cfengine (max 4000)");
            break;
        }

        newlist = SplitRegexAsRList(linebuf, split, maxent, allowblanks);

        vcount = 0;
        first_one[0] = '\0';

        for (rp = newlist; rp != NULL; rp = rp->next)
        {
            char this_rval[CF_MAXVARSIZE];
            long ival;

            switch (type)
            {
            case cf_str:
                strncpy(this_rval, rp->item, CF_MAXVARSIZE - 1);
                break;

            case cf_int:
                ival = Str2Int(rp->item);
                snprintf(this_rval, CF_MAXVARSIZE, "%d", (int) ival);
                break;

            case cf_real:
                Str2Double(rp->item);   /* Verify syntax */
                sscanf(rp->item, "%255s", this_rval);
                break;

            default:

                FatalError("Software error readstringarray - abused type");
            }

            if (strlen(first_one) == 0)
            {
                strncpy(first_one, this_rval, CF_MAXVARSIZE - 1);
            }

            if (intIndex)
            {
                snprintf(name, CF_MAXVARSIZE, "%s[%d][%d]", array_lval, hcount, vcount);
            }
            else
            {
                snprintf(name, CF_MAXVARSIZE, "%s[%s][%d]", array_lval, first_one, vcount);
            }

            NewScalar(THIS_BUNDLE, name, this_rval, type);
            vcount++;
        }

        DeleteRlist(newlist);

        hcount++;
        sp += lineLen;

        if (*sp == '\0')        // either \n or \0
        {
            break;
        }
    }

/* Don't free data - goes into vars */

    return hcount;
}

/*********************************************************************/

static int ExecModule(char *command, const char *namespace)
{
    FILE *pp;
    char *sp, line[CF_BUFSIZE];
    int print = false;

    if ((pp = cf_popen(command, "r")) == NULL)
    {
        CfOut(cf_error, "cf_popen", "Couldn't open pipe from %s\n", command);
        return false;
    }

    while (!feof(pp))
    {
        if (ferror(pp))         /* abortable */
        {
            CfOut(cf_error, "", "Shell command pipe %s\n", command);
            break;
        }

        CfReadLine(line, CF_BUFSIZE, pp);

        if (strlen(line) > CF_BUFSIZE - 80)
        {
            CfOut(cf_error, "", "Line from module %s is too long to be sensible\n", command);
            break;
        }

        if (ferror(pp))         /* abortable */
        {
            CfOut(cf_error, "", "Shell command pipe %s\n", command);
            break;
        }

        print = false;

        for (sp = line; *sp != '\0'; sp++)
        {
            if (!isspace((int) *sp))
            {
                print = true;
                break;
            }
        }

        ModuleProtocol(command, line, print, namespace);
    }

    cf_pclose(pp);
    return true;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void ModuleProtocol(char *command, char *line, int print, const char *namespace)
{
    char name[CF_BUFSIZE], content[CF_BUFSIZE], context[CF_BUFSIZE];
    char arg0[CF_BUFSIZE];
    char *filename;

/* Infer namespace from script name */

    snprintf(arg0, CF_BUFSIZE, "%s", GetArg0(command));
    filename = basename(arg0);

/* Canonicalize filename into acceptable namespace name*/

    CanonifyNameInPlace(filename);
    strcpy(context, filename);
    CfOut(cf_verbose, "", "Module context: %s\n", context);

    NewScope(context);
    name[0] = '\0';
    content[0] = '\0';

    switch (*line)
    {
    case '+':
        CfOut(cf_verbose, "", "Activated classes: %s\n", line + 1);
        if (CheckID(line + 1))
        {
             NewClass(line + 1, namespace);
        }
        break;
    case '-':
        CfOut(cf_verbose, "", "Deactivated classes: %s\n", line + 1);
        if (CheckID(line + 1))
        {
            NegateClassesFromString(line + 1);
        }
        break;
    case '=':
        content[0] = '\0';
        sscanf(line + 1, "%[^=]=%[^\n]", name, content);

        if (CheckID(name))
        {
            CfOut(cf_verbose, "", "Defined variable: %s in context %s with value: %s\n", name, context, content);
            NewScalar(context, name, content, cf_str);
        }
        break;

    case '@':
        content[0] = '\0';
        sscanf(line + 1, "%[^=]=%[^\n]", name, content);

        if (CheckID(name))
        {
            Rlist *list = NULL;

            CfOut(cf_verbose, "", "Defined variable: %s in context %s with value: %s\n", name, context, content);
            list = ParseShownRlist(content);
            NewList(context, name, list, cf_slist);
        }
        break;

    case '\0':
        break;

    default:
        if (print)
        {
            CfOut(cf_cmdout, "", "M \"%s\": %s\n", command, line);
        }
        break;
    }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static int CheckID(char *id)
{
    char *sp;

    for (sp = id; *sp != '\0'; sp++)
    {
        if (!isalnum((int) *sp) && (*sp != '_') && (*sp != '[') && (*sp != ']'))
        {
            CfOut(cf_error, "",
                  "Module protocol contained an illegal character \'%c\' in class/variable identifier \'%s\'.", *sp,
                  id);
            return false;
        }
    }

    return true;
}

/*********************************************************************/

FnCallResult CallFunction(const FnCallType *function, FnCall *fp, Rlist *expargs)
{
    ArgTemplate(fp, function->args, expargs);
    return (*function->impl) (fp, expargs);
}

/*********************************************************/
/* Function prototypes                                   */
/*********************************************************/

FnCallArg ACCESSEDBEFORE_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "Newer filename"},
    {CF_ABSPATHRANGE, cf_str, "Older filename"},
    {NULL, cf_notype, NULL}
};

FnCallArg ACCUM_ARGS[] =
{
    {"0,1000", cf_int, "Years"},
    {"0,1000", cf_int, "Months"},
    {"0,1000", cf_int, "Days"},
    {"0,1000", cf_int, "Hours"},
    {"0,1000", cf_int, "Minutes"},
    {"0,40000", cf_int, "Seconds"},
    {NULL, cf_notype, NULL}
};

FnCallArg AND_ARGS[] =
{
    {NULL, cf_notype, NULL}
};

FnCallArg AGO_ARGS[] =
{
    {"0,1000", cf_int, "Years"},
    {"0,1000", cf_int, "Months"},
    {"0,1000", cf_int, "Days"},
    {"0,1000", cf_int, "Hours"},
    {"0,1000", cf_int, "Minutes"},
    {"0,40000", cf_int, "Seconds"},
    {NULL, cf_notype, NULL}
};

FnCallArg LATERTHAN_ARGS[] =
{
    {"0,1000", cf_int, "Years"},
    {"0,1000", cf_int, "Months"},
    {"0,1000", cf_int, "Days"},
    {"0,1000", cf_int, "Hours"},
    {"0,1000", cf_int, "Minutes"},
    {"0,40000", cf_int, "Seconds"},
    {NULL, cf_notype, NULL}
};

FnCallArg CANONIFY_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "String containing non-identifier characters"},
    {NULL, cf_notype, NULL}
};

FnCallArg CHANGEDBEFORE_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "Newer filename"},
    {CF_ABSPATHRANGE, cf_str, "Older filename"},
    {NULL, cf_notype, NULL}
};

FnCallArg CLASSIFY_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Input string"},
    {NULL, cf_notype, NULL}
};

FnCallArg CLASSMATCH_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Regular expression"},
    {NULL, cf_notype, NULL}
};

FnCallArg CONCAT_ARGS[] =
{
    {NULL, cf_notype, NULL}
};

FnCallArg COUNTCLASSESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Regular expression"},
    {NULL, cf_notype, NULL}
};

FnCallArg COUNTLINESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Regular expression"},
    {CF_ABSPATHRANGE, cf_str, "Filename"},
    {NULL, cf_notype, NULL}
};

FnCallArg DIRNAME_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "File path"},
    {NULL, cf_notype, NULL},
};

FnCallArg DISKFREE_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "File system directory"},
    {NULL, cf_notype, NULL}
};

FnCallArg ESCAPE_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "IP address or string to escape"},
    {NULL, cf_notype, NULL}
};

FnCallArg EXECRESULT_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "Fully qualified command path"},
    {"useshell,noshell", cf_opts, "Shell encapsulation option"},
    {NULL, cf_notype, NULL}
};

// fileexists, isdir,isplain,islink

FnCallArg FILESTAT_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "File object name"},
    {NULL, cf_notype, NULL}
};

FnCallArg FILESEXIST_ARGS[] =
{
    {CF_NAKEDLRANGE, cf_str, "Array identifier containing list"},
    {NULL, cf_notype, NULL}
};

FnCallArg GETFIELDS_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Regular expression to match line"},
    {CF_ABSPATHRANGE, cf_str, "Filename to read"},
    {CF_ANYSTRING, cf_str, "Regular expression to split fields"},
    {CF_ANYSTRING, cf_str, "Return array name"},
    {NULL, cf_notype, NULL}
};

FnCallArg GETINDICES_ARGS[] =
{
    {CF_IDRANGE, cf_str, "Cfengine array identifier"},
    {NULL, cf_notype, NULL}
};

FnCallArg GETUSERS_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Comma separated list of User names"},
    {CF_ANYSTRING, cf_str, "Comma separated list of UserID numbers"},
    {NULL, cf_notype, NULL}
};

FnCallArg GETENV_ARGS[] =
{
    {CF_IDRANGE, cf_str, "Name of environment variable"},
    {CF_VALRANGE, cf_int, "Maximum number of characters to read "},
    {NULL, cf_notype, NULL}
};

FnCallArg GETGID_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Group name in text"},
    {NULL, cf_notype, NULL}
};

FnCallArg GETUID_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "User name in text"},
    {NULL, cf_notype, NULL}
};

FnCallArg GREP_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Regular expression"},
    {CF_IDRANGE, cf_str, "CFEngine list identifier"},
    {NULL, cf_notype, NULL}
};

FnCallArg GROUPEXISTS_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Group name or identifier"},
    {NULL, cf_notype, NULL}
};

FnCallArg HASH_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Input text"},
    {"md5,sha1,sha256,sha512,sha384,crypt", cf_opts, "Hash or digest algorithm"},
    {NULL, cf_notype, NULL}
};

FnCallArg HASHMATCH_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "Filename to hash"},
    {"md5,sha1,crypt,cf_sha224,cf_sha256,cf_sha384,cf_sha512", cf_opts, "Hash or digest algorithm"},
    {CF_IDRANGE, cf_str, "ASCII representation of hash for comparison"},
    {NULL, cf_notype, NULL}
};

FnCallArg HOST2IP_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Host name in ascii"},
    {NULL, cf_notype, NULL}
};

FnCallArg IP2HOST_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "IP address (IPv4 or IPv6)"},
    {NULL, cf_notype, NULL}
};

FnCallArg HOSTINNETGROUP_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Netgroup name"},
    {NULL, cf_notype, NULL}
};

FnCallArg HOSTRANGE_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Hostname prefix"},
    {CF_ANYSTRING, cf_str, "Enumerated range"},
    {NULL, cf_notype, NULL}
};

FnCallArg HOSTSSEEN_ARGS[] =
{
    {CF_VALRANGE, cf_int, "Horizon since last seen in hours"},
    {"lastseen,notseen", cf_opts, "Complements for selection policy"},
    {"name,address", cf_opts, "Type of return value desired"},
    {NULL, cf_notype, NULL}
};

FnCallArg HOSTSWITHCLASS_ARGS[] =
{
    {"[a-zA-Z0-9_]+", cf_str, "Class name to look for"},
    {"name,address", cf_opts, "Type of return value desired"},
    {NULL, cf_notype, NULL}
};

FnCallArg IPRANGE_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "IP address range syntax"},
    {NULL, cf_notype, NULL}
};

FnCallArg IRANGE_ARGS[] =
{
    {CF_INTRANGE, cf_int, "Integer"},
    {CF_INTRANGE, cf_int, "Integer"},
    {NULL, cf_notype, NULL}
};

FnCallArg ISGREATERTHAN_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Larger string or value"},
    {CF_ANYSTRING, cf_str, "Smaller string or value"},
    {NULL, cf_notype, NULL}
};

FnCallArg ISLESSTHAN_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Smaller string or value"},
    {CF_ANYSTRING, cf_str, "Larger string or value"},
    {NULL, cf_notype, NULL}
};

FnCallArg ISNEWERTHAN_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "Newer file name"},
    {CF_ABSPATHRANGE, cf_str, "Older file name"},
    {NULL, cf_notype, NULL}
};

FnCallArg ISVARIABLE_ARGS[] =
{
    {CF_IDRANGE, cf_str, "Variable identifier"},
    {NULL, cf_notype, NULL}
};

FnCallArg JOIN_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Join glue-string"},
    {CF_IDRANGE, cf_str, "CFEngine list identifier"},
    {NULL, cf_notype, NULL}
};

FnCallArg LASTNODE_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Input string"},
    {CF_ANYSTRING, cf_str, "Link separator, e.g. /,:"},
    {NULL, cf_notype, NULL}
};

FnCallArg LDAPARRAY_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Array name"},
    {CF_ANYSTRING, cf_str, "URI"},
    {CF_ANYSTRING, cf_str, "Distinguished name"},
    {CF_ANYSTRING, cf_str, "Filter"},
    {"subtree,onelevel,base", cf_opts, "Search scope policy"},
    {"none,ssl,sasl", cf_opts, "Security level"},
    {NULL, cf_notype, NULL}
};

FnCallArg LDAPLIST_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "URI"},
    {CF_ANYSTRING, cf_str, "Distinguished name"},
    {CF_ANYSTRING, cf_str, "Filter"},
    {CF_ANYSTRING, cf_str, "Record name"},
    {"subtree,onelevel,base", cf_opts, "Search scope policy"},
    {"none,ssl,sasl", cf_opts, "Security level"},
    {NULL, cf_notype, NULL}
};

FnCallArg LDAPVALUE_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "URI"},
    {CF_ANYSTRING, cf_str, "Distinguished name"},
    {CF_ANYSTRING, cf_str, "Filter"},
    {CF_ANYSTRING, cf_str, "Record name"},
    {"subtree,onelevel,base", cf_opts, "Search scope policy"},
    {"none,ssl,sasl", cf_opts, "Security level"},
    {NULL, cf_notype, NULL}
};

FnCallArg LSDIRLIST_ARGS[] =
{
    {CF_PATHRANGE, cf_str, "Path to base directory"},
    {CF_ANYSTRING, cf_str, "Regular expression to match files or blank"},
    {CF_BOOL, cf_opts, "Include the base path in the list"},
    {NULL, cf_notype, NULL}
};

FnCallArg MAPLIST_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Pattern based on $(this) as original text"},
    {CF_IDRANGE, cf_str, "The name of the list variable to map"},
    {NULL, cf_notype, NULL}
};

FnCallArg NOT_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Class value"},
    {NULL, cf_notype, NULL}
};

FnCallArg NOW_ARGS[] =
{
    {NULL, cf_notype, NULL}
};

FnCallArg OR_ARGS[] =
{
    {NULL, cf_notype, NULL}
};

FnCallArg SUM_ARGS[] =
{
    {CF_IDRANGE, cf_str, "A list of arbitrary real values"},
    {NULL, cf_notype, NULL}
};

FnCallArg PRODUCT_ARGS[] =
{
    {CF_IDRANGE, cf_str, "A list of arbitrary real values"},
    {NULL, cf_notype, NULL}
};

FnCallArg DATE_ARGS[] =
{
    {"1970,3000", cf_int, "Year"},
    {"1,12", cf_int, "Month"},
    {"1,31", cf_int, "Day"},
    {"0,23", cf_int, "Hour"},
    {"0,59", cf_int, "Minute"},
    {"0,59", cf_int, "Second"},
    {NULL, cf_notype, NULL}
};

FnCallArg PEERS_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "File name of host list"},
    {CF_ANYSTRING, cf_str, "Comment regex pattern"},
    {CF_VALRANGE, cf_int, "Peer group size"},
    {NULL, cf_notype, NULL}
};

FnCallArg PEERLEADER_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "File name of host list"},
    {CF_ANYSTRING, cf_str, "Comment regex pattern"},
    {CF_VALRANGE, cf_int, "Peer group size"},
    {NULL, cf_notype, NULL}
};

FnCallArg PEERLEADERS_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "File name of host list"},
    {CF_ANYSTRING, cf_str, "Comment regex pattern"},
    {CF_VALRANGE, cf_int, "Peer group size"},
    {NULL, cf_notype, NULL}
};

FnCallArg RANDOMINT_ARGS[] =
{
    {CF_INTRANGE, cf_int, "Lower inclusive bound"},
    {CF_INTRANGE, cf_int, "Upper inclusive bound"},
    {NULL, cf_notype, NULL}
};

FnCallArg READFILE_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "File name"},
    {CF_VALRANGE, cf_int, "Maximum number of bytes to read"},
    {NULL, cf_notype, NULL}
};

FnCallArg READSTRINGARRAY_ARGS[] =
{
    {CF_IDRANGE, cf_str, "Array identifier to populate"},
    {CF_ABSPATHRANGE, cf_str, "File name to read"},
    {CF_ANYSTRING, cf_str, "Regex matching comments"},
    {CF_ANYSTRING, cf_str, "Regex to split data"},
    {CF_VALRANGE, cf_int, "Maximum number of entries to read"},
    {CF_VALRANGE, cf_int, "Maximum bytes to read"},
    {NULL, cf_notype, NULL}
};

FnCallArg PARSESTRINGARRAY_ARGS[] =
{
    {CF_IDRANGE, cf_str, "Array identifier to populate"},
    {CF_ABSPATHRANGE, cf_str, "A string to parse for input data"},
    {CF_ANYSTRING, cf_str, "Regex matching comments"},
    {CF_ANYSTRING, cf_str, "Regex to split data"},
    {CF_VALRANGE, cf_int, "Maximum number of entries to read"},
    {CF_VALRANGE, cf_int, "Maximum bytes to read"},
    {NULL, cf_notype, NULL}
};

FnCallArg READSTRINGARRAYIDX_ARGS[] =
{
    {CF_IDRANGE, cf_str, "Array identifier to populate"},
    {CF_ABSPATHRANGE, cf_str, "A string to parse for input data"},
    {CF_ANYSTRING, cf_str, "Regex matching comments"},
    {CF_ANYSTRING, cf_str, "Regex to split data"},
    {CF_VALRANGE, cf_int, "Maximum number of entries to read"},
    {CF_VALRANGE, cf_int, "Maximum bytes to read"},
    {NULL, cf_notype, NULL}
};

FnCallArg PARSESTRINGARRAYIDX_ARGS[] =
{
    {CF_IDRANGE, cf_str, "Array identifier to populate"},
    {CF_ABSPATHRANGE, cf_str, "A string to parse for input data"},
    {CF_ANYSTRING, cf_str, "Regex matching comments"},
    {CF_ANYSTRING, cf_str, "Regex to split data"},
    {CF_VALRANGE, cf_int, "Maximum number of entries to read"},
    {CF_VALRANGE, cf_int, "Maximum bytes to read"},
    {NULL, cf_notype, NULL}
};

FnCallArg READSTRINGLIST_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "File name to read"},
    {CF_ANYSTRING, cf_str, "Regex matching comments"},
    {CF_ANYSTRING, cf_str, "Regex to split data"},
    {CF_VALRANGE, cf_int, "Maximum number of entries to read"},
    {CF_VALRANGE, cf_int, "Maximum bytes to read"},
    {NULL, cf_notype, NULL}
};

FnCallArg READTCP_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Host name or IP address of server socket"},
    {CF_VALRANGE, cf_int, "Port number"},
    {CF_ANYSTRING, cf_str, "Protocol query string"},
    {CF_VALRANGE, cf_int, "Maximum number of bytes to read"},
    {NULL, cf_notype, NULL}
};

FnCallArg REGARRAY_ARGS[] =
{
    {CF_IDRANGE, cf_str, "Cfengine array identifier"},
    {CF_ANYSTRING, cf_str, "Regular expression"},
    {NULL, cf_notype, NULL}
};

FnCallArg REGCMP_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Regular expression"},
    {CF_ANYSTRING, cf_str, "Match string"},
    {NULL, cf_notype, NULL}
};

FnCallArg REGEXTRACT_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Regular expression"},
    {CF_ANYSTRING, cf_str, "Match string"},
    {CF_IDRANGE, cf_str, "Identifier for back-references"},
    {NULL, cf_notype, NULL}
};

FnCallArg REGISTRYVALUE_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Windows registry key"},
    {CF_ANYSTRING, cf_str, "Windows registry value-id"},
    {NULL, cf_notype, NULL}
};

FnCallArg REGLINE_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Regular expression"},
    {CF_ANYSTRING, cf_str, "Filename to search"},
    {NULL, cf_notype, NULL}
};

FnCallArg REGLIST_ARGS[] =
{
    {CF_NAKEDLRANGE, cf_str, "Cfengine list identifier"},
    {CF_ANYSTRING, cf_str, "Regular expression"},
    {NULL, cf_notype, NULL}
};

FnCallArg REGLDAP_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "URI"},
    {CF_ANYSTRING, cf_str, "Distinguished name"},
    {CF_ANYSTRING, cf_str, "Filter"},
    {CF_ANYSTRING, cf_str, "Record name"},
    {"subtree,onelevel,base", cf_opts, "Search scope policy"},
    {CF_ANYSTRING, cf_str, "Regex to match results"},
    {"none,ssl,sasl", cf_opts, "Security level"},
    {NULL, cf_notype, NULL}
};

FnCallArg REMOTESCALAR_ARGS[] =
{
    {CF_IDRANGE, cf_str, "Variable identifier"},
    {CF_ANYSTRING, cf_str, "Hostname or IP address of server"},
    {CF_BOOL, cf_opts, "Use enryption"},
    {NULL, cf_notype, NULL}
};

FnCallArg HUB_KNOWLEDGE_ARGS[] =
{
    {CF_IDRANGE, cf_str, "Variable identifier"},
    {NULL, cf_notype, NULL}
};

FnCallArg REMOTECLASSESMATCHING_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Regular expression"},
    {CF_ANYSTRING, cf_str, "Server name or address"},
    {CF_BOOL, cf_opts, "Use encryption"},
    {CF_IDRANGE, cf_str, "Return class prefix"},
    {NULL, cf_notype, NULL}
};

FnCallArg RETURNSZERO_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "Fully qualified command path"},
    {"useshell,noshell", cf_opts, "Shell encapsulation option"},
    {NULL, cf_notype, NULL}
};

FnCallArg RRANGE_ARGS[] =
{
    {CF_REALRANGE, cf_real, "Real number"},
    {CF_REALRANGE, cf_real, "Real number"},
    {NULL, cf_notype, NULL}
};

FnCallArg SELECTSERVERS_ARGS[] =
{
    {CF_NAKEDLRANGE, cf_str, "The identifier of a cfengine list of hosts or addresses to contact"},
    {CF_VALRANGE, cf_int, "The port number"},
    {CF_ANYSTRING, cf_str, "A query string"},
    {CF_ANYSTRING, cf_str, "A regular expression to match success"},
    {CF_VALRANGE, cf_int, "Maximum number of bytes to read from server"},
    {CF_IDRANGE, cf_str, "Name for array of results"},
    {NULL, cf_notype, NULL}
};

FnCallArg SPLAYCLASS_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Input string for classification"},
    {"daily,hourly", cf_opts, "Splay time policy"},
    {NULL, cf_notype, NULL}
};

FnCallArg SPLITSTRING_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "A data string"},
    {CF_ANYSTRING, cf_str, "Regex to split on"},
    {CF_VALRANGE, cf_int, "Maximum number of pieces"},
    {NULL, cf_notype, NULL}
};

FnCallArg STRCMP_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "String"},
    {CF_ANYSTRING, cf_str, "String"},
    {NULL, cf_notype, NULL}
};

FnCallArg TRANSLATEPATH_ARGS[] =
{
    {CF_ABSPATHRANGE, cf_str, "Unix style path"},
    {NULL, cf_notype, NULL}
};

FnCallArg USEMODULE_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "Name of module command"},
    {CF_ANYSTRING, cf_str, "Argument string for the module"},
    {NULL, cf_notype, NULL}
};

FnCallArg USEREXISTS_ARGS[] =
{
    {CF_ANYSTRING, cf_str, "User name or identifier"},
    {NULL, cf_notype, NULL}
};

/*********************************************************/
/* FnCalls are rvalues in certain promise constraints    */
/*********************************************************/

/* see cf3.defs.h enum fncalltype */

const FnCallType CF_FNCALL_TYPES[] =
{
    {"accessedbefore", cf_class, ACCESSEDBEFORE_ARGS, &FnCallIsAccessedBefore,
     "True if arg1 was accessed before arg2 (atime)"},
    {"accumulated", cf_int, ACCUM_ARGS, &FnCallAccumulatedDate,
     "Convert an accumulated amount of time into a system representation"},
    {"ago", cf_int, AGO_ARGS, &FnCallAgoDate, "Convert a time relative to now to an integer system representation"},
    {"and", cf_str, AND_ARGS, &FnCallAnd, "Calculate whether all arguments evaluate to true", true},
    {"canonify", cf_str, CANONIFY_ARGS, &FnCallCanonify, "Convert an abitrary string into a legal class name"},
    {"concat", cf_str, CONCAT_ARGS, &FnCallConcat, "Concatenate all arguments into string", true},
    {"changedbefore", cf_class, CHANGEDBEFORE_ARGS, &FnCallIsChangedBefore,
     "True if arg1 was changed before arg2 (ctime)"},
    {"classify", cf_class, CLASSIFY_ARGS, &FnCallClassify,
     "True if the canonicalization of the argument is a currently defined class"},
    {"classmatch", cf_class, CLASSMATCH_ARGS, &FnCallClassMatch,
     "True if the regular expression matches any currently defined class"},
    {"countclassesmatching", cf_int, COUNTCLASSESMATCHING_ARGS, &FnCallCountClassesMatching,
     "Count the number of defined classes matching regex arg1"},
    {"countlinesmatching", cf_int, COUNTLINESMATCHING_ARGS, &FnCallCountLinesMatching,
     "Count the number of lines matching regex arg1 in file arg2"},
    {"dirname", cf_str, DIRNAME_ARGS, &FnCallDirname, "Return the parent directory name for given path"},
    {"diskfree", cf_int, DISKFREE_ARGS, &FnCallDiskFree,
     "Return the free space (in KB) available on the directory's current partition (0 if not found)"},
    {"escape", cf_str, ESCAPE_ARGS, &FnCallEscape, "Escape regular expression characters in a string"},
    {"execresult", cf_str, EXECRESULT_ARGS, &FnCallExecResult, "Execute named command and assign output to variable"},
    {"fileexists", cf_class, FILESTAT_ARGS, &FnCallFileStat, "True if the named file can be accessed"},
    {"filesexist", cf_class, FILESEXIST_ARGS, &FnCallFileSexist, "True if the named list of files can ALL be accessed"},
    {"filesize", cf_int, FILESTAT_ARGS, &FnCallFileStat, "Returns the size in bytes of the file"},
    {"getenv", cf_str, GETENV_ARGS, &FnCallGetEnv,
     "Return the environment variable named arg1, truncated at arg2 characters"},
    {"getfields", cf_int, GETFIELDS_ARGS, &FnCallGetFields,
     "Get an array of fields in the lines matching regex arg1 in file arg2, split on regex arg3 as array name arg4"},
    {"getgid", cf_int, GETGID_ARGS, &FnCallGetGid, "Return the integer group id of the named group on this host"},
    {"getindices", cf_slist, GETINDICES_ARGS, &FnCallGetIndices,
     "Get a list of keys to the array whose id is the argument and assign to variable"},
    {"getuid", cf_int, GETUID_ARGS, &FnCallGetUid, "Return the integer user id of the named user on this host"},
    {"getusers", cf_slist, GETUSERS_ARGS, &FnCallGetUsers,
     "Get a list of all system users defined, minus those names defined in arg1 and uids in arg2"},
    {"getvalues", cf_slist, GETINDICES_ARGS, &FnCallGetValues,
     "Get a list of values corresponding to the right hand sides in an array whose id is the argument and assign to variable"},
    {"grep", cf_slist, GREP_ARGS, &FnCallGrep,
     "Extract the sub-list if items matching the regular expression in arg1 of the list named in arg2"},
    {"groupexists", cf_class, GROUPEXISTS_ARGS, &FnCallGroupExists,
     "True if group or numerical id exists on this host"},
    {"hash", cf_str, HASH_ARGS, &FnCallHash, "Return the hash of arg1, type arg2 and assign to a variable"},
    {"hashmatch", cf_class, HASHMATCH_ARGS, &FnCallHashMatch,
     "Compute the hash of arg1, of type arg2 and test if it matches the value in arg3"},
    {"host2ip", cf_str, HOST2IP_ARGS, &FnCallHost2IP, "Returns the primary name-service IP address for the named host"},
    {"ip2host", cf_str, IP2HOST_ARGS, &FnCallIP2Host, "Returns the primary name-service host name for the IP address"},
    {"hostinnetgroup", cf_class, HOSTINNETGROUP_ARGS, &FnCallHostInNetgroup,
     "True if the current host is in the named netgroup"},
    {"hostrange", cf_class, HOSTRANGE_ARGS, &FnCallHostRange,
     "True if the current host lies in the range of enumerated hostnames specified"},
    {"hostsseen", cf_slist, HOSTSSEEN_ARGS, &FnCallHostsSeen,
     "Extract the list of hosts last seen/not seen within the last arg1 hours"},
    {"hostswithclass", cf_slist, HOSTSWITHCLASS_ARGS, &FnCallHostsWithClass,
     "Extract the list of hosts with the given class set from the hub database (commercial extension)"},
    {"hubknowledge", cf_str, HUB_KNOWLEDGE_ARGS, &FnCallHubKnowledge,
     "Read global knowledge from the hub host by id (commercial extension)"},
    {"iprange", cf_class, IPRANGE_ARGS, &FnCallIPRange,
     "True if the current host lies in the range of IP addresses specified"},
    {"irange", cf_irange, IRANGE_ARGS, &FnCallIRange, "Define a range of integer values for cfengine internal use"},
    {"isdir", cf_class, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a directory"},
    {"isexecutable", cf_class, FILESTAT_ARGS, &FnCallFileStat,
     "True if the named object has execution rights for the current user"},
    {"isgreaterthan", cf_class, ISGREATERTHAN_ARGS, &FnCallIsLessGreaterThan,
     "True if arg1 is numerically greater than arg2, else compare strings like strcmp"},
    {"islessthan", cf_class, ISLESSTHAN_ARGS, &FnCallIsLessGreaterThan,
     "True if arg1 is numerically less than arg2, else compare strings like NOT strcmp"},
    {"islink", cf_class, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a symbolic link"},
    {"isnewerthan", cf_class, ISNEWERTHAN_ARGS, &FnCallIsNewerThan,
     "True if arg1 is newer (modified later) than arg2 (mtime)"},
    {"isplain", cf_class, FILESTAT_ARGS, &FnCallFileStat, "True if the named object is a plain/regular file"},
    {"isvariable", cf_class, ISVARIABLE_ARGS, &FnCallIsVariable, "True if the named variable is defined"},
    {"join", cf_str, JOIN_ARGS, &FnCallJoin, "Join the items of arg2 into a string, using the conjunction in arg1"},
    {"lastnode", cf_str, LASTNODE_ARGS, &FnCallLastNode,
     "Extract the last of a separated string, e.g. filename from a path"},
    {"laterthan", cf_class, LATERTHAN_ARGS, &FnCallLaterThan, "True if the current time is later than the given date"},
    {"ldaparray", cf_class, LDAPARRAY_ARGS, &FnCallLDAPArray, "Extract all values from an ldap record"},
    {"ldaplist", cf_slist, LDAPLIST_ARGS, &FnCallLDAPList, "Extract all named values from multiple ldap records"},
    {"ldapvalue", cf_str, LDAPVALUE_ARGS, &FnCallLDAPValue, "Extract the first matching named value from ldap"},
    {"lsdir", cf_slist, LSDIRLIST_ARGS, &FnCallLsDir,
     "Return a list of files in a directory matching a regular expression"},
    {"maplist", cf_slist, MAPLIST_ARGS, &FnCallMapList,
     "Return a list with each element modified by a pattern based $(this)"},
    {"not", cf_str, NOT_ARGS, &FnCallNot, "Calculate whether argument is false"},
    {"now", cf_int, NOW_ARGS, &FnCallNow, "Convert the current time into system representation"},
    {"on", cf_int, DATE_ARGS, &FnCallOn, "Convert an exact date/time to an integer system representation"},
    {"or", cf_str, OR_ARGS, &FnCallOr, "Calculate whether any argument evaluates to true", true},
    {"parseintarray", cf_int, PARSESTRINGARRAY_ARGS, &FnCallParseIntArray,
     "Read an array of integers from a file and assign the dimension to a variable"},
    {"parserealarray", cf_int, PARSESTRINGARRAY_ARGS, &FnCallParseRealArray,
     "Read an array of real numbers from a file and assign the dimension to a variable"},
    {"parsestringarray", cf_int, PARSESTRINGARRAY_ARGS, &FnCallParseStringArray,
     "Read an array of strings from a file and assign the dimension to a variable"},
    {"parsestringarrayidx", cf_int, PARSESTRINGARRAYIDX_ARGS, &FnCallParseStringArrayIndex,
     "Read an array of strings from a file and assign the dimension to a variable with integer indeces"},
    {"peers", cf_slist, PEERS_ARGS, &FnCallPeers,
     "Get a list of peers (not including ourself) from the partition to which we belong"},
    {"peerleader", cf_str, PEERLEADER_ARGS, &FnCallPeerLeader,
     "Get the assigned peer-leader of the partition to which we belong"},
    {"peerleaders", cf_slist, PEERLEADERS_ARGS, &FnCallPeerLeaders,
     "Get a list of peer leaders from the named partitioning"},
    {"product", cf_real, PRODUCT_ARGS, &FnCallProduct, "Return the product of a list of reals"},
    {"randomint", cf_int, RANDOMINT_ARGS, &FnCallRandomInt, "Generate a random integer between the given limits"},
    {"readfile", cf_str, READFILE_ARGS, &FnCallReadFile,
     "Read max number of bytes from named file and assign to variable"},
    {"readintarray", cf_int, READSTRINGARRAY_ARGS, &FnCallReadIntArray,
     "Read an array of integers from a file and assign the dimension to a variable"},
    {"readintlist", cf_ilist, READSTRINGLIST_ARGS, &FnCallReadIntList,
     "Read and assign a list variable from a file of separated ints"},
    {"readrealarray", cf_int, READSTRINGARRAY_ARGS, &FnCallReadRealArray,
     "Read an array of real numbers from a file and assign the dimension to a variable"},
    {"readreallist", cf_rlist, READSTRINGLIST_ARGS, &FnCallReadRealList,
     "Read and assign a list variable from a file of separated real numbers"},
    {"readstringarray", cf_int, READSTRINGARRAY_ARGS, &FnCallReadStringArray,
     "Read an array of strings from a file and assign the dimension to a variable"},
    {"readstringarrayidx", cf_int, READSTRINGARRAYIDX_ARGS, &FnCallReadStringArrayIndex,
     "Read an array of strings from a file and assign the dimension to a variable with integer indeces"},
    {"readstringlist", cf_slist, READSTRINGLIST_ARGS, &FnCallReadStringList,
     "Read and assign a list variable from a file of separated strings"},
    {"readtcp", cf_str, READTCP_ARGS, &FnCallReadTcp, "Connect to tcp port, send string and assign result to variable"},
    {"regarray", cf_class, REGARRAY_ARGS, &FnCallRegArray,
     "True if arg1 matches any item in the associative array with id=arg2"},
    {"regcmp", cf_class, REGCMP_ARGS, &FnCallRegCmp,
     "True if arg1 is a regular expression matching that matches string arg2"},
    {"regextract", cf_class, REGEXTRACT_ARGS, &FnCallRegExtract,
     "True if the regular expression in arg 1 matches the string in arg2 and sets a non-empty array of backreferences named arg3"},
    {"registryvalue", cf_str, REGISTRYVALUE_ARGS, &FnCallRegistryValue,
     "Returns a value for an MS-Win registry key,value pair"},
    {"regline", cf_class, REGLINE_ARGS, &FnCallRegLine,
     "True if the regular expression in arg1 matches a line in file arg2"},
    {"reglist", cf_class, REGLIST_ARGS, &FnCallRegList,
     "True if the regular expression in arg2 matches any item in the list whose id is arg1"},
    {"regldap", cf_class, REGLDAP_ARGS, &FnCallRegLDAP,
     "True if the regular expression in arg6 matches a value item in an ldap search"},
    {"remotescalar", cf_str, REMOTESCALAR_ARGS, &FnCallRemoteScalar,
     "Read a scalar value from a remote cfengine server"},
    {"remoteclassesmatching", cf_class, REMOTECLASSESMATCHING_ARGS, &FnCallRemoteClassesMatching,
     "Read persistent classes matching a regular expression from a remote cfengine server and add them into local context with prefix"},
    {"returnszero", cf_class, RETURNSZERO_ARGS, &FnCallReturnsZero, "True if named shell command has exit status zero"},
    {"rrange", cf_rrange, RRANGE_ARGS, &FnCallRRange, "Define a range of real numbers for cfengine internal use"},
    {"selectservers", cf_int, SELECTSERVERS_ARGS, &FnCallSelectServers,
     "Select tcp servers which respond correctly to a query and return their number, set array of names"},
    {"splayclass", cf_class, SPLAYCLASS_ARGS, &FnCallSplayClass,
     "True if the first argument's time-slot has arrived, according to a policy in arg2"},
    {"splitstring", cf_slist, SPLITSTRING_ARGS, &FnCallSplitString,
     "Convert a string in arg1 into a list of max arg3 strings by splitting on a regular expression in arg2"},
    {"strcmp", cf_class, STRCMP_ARGS, &FnCallStrCmp, "True if the two strings match exactly"},
    {"sum", cf_real, SUM_ARGS, &FnCallSum, "Return the sum of a list of reals"},
    {"translatepath", cf_str, TRANSLATEPATH_ARGS, &FnCallTranslatePath,
     "Translate path separators from Unix style to the host's native"},
    {"usemodule", cf_class, USEMODULE_ARGS, &FnCallUseModule,
     "Execute cfengine module script and set class if successful"},
    {"userexists", cf_class, USEREXISTS_ARGS, &FnCallUserExists,
     "True if user name or numerical id exists on this host"},
    {NULL}
};
