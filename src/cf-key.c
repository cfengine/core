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
/* File: exec.c                                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#include "dbm_api.h"
#include "lastseen.h"
#include "dir.h"

int SHOWHOSTS = false;
bool REMOVEKEYS = false;
const char *remove_keys_host;

static GenericAgentConfig CheckOpts(int argc, char **argv);

static void ShowLastSeenHosts(void);
static int RemoveKeys(const char *host);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *ID = "The cfengine's generator makes key pairs for remote authentication.\n";

static const struct option OPTIONS[17] =
{
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"output-file", required_argument, 0, 'f'},
    {"show-hosts", no_argument, 0, 's'},
    {"remove-keys", required_argument, 0, 'r'},
    {NULL, 0, 0, '\0'}
};

static const char *HINTS[17] =
{
    "Print the help message",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    "Specify an alternative output file than the default (localhost)",
    "Show lastseen hostnames and IP addresses",
    "Remove keys for specified hostname/IP",
    NULL
};

/*****************************************************************************/

int main(int argc, char *argv[])
{
    GenericAgentConfig config = CheckOpts(argc, argv);

    THIS_AGENT_TYPE = cf_keygen;

    GenericInitialize("keygenerator", config);

    if (SHOWHOSTS)
    {
        ShowLastSeenHosts();
        return 0;
    }

    if (REMOVEKEYS)
    {
        return RemoveKeys(remove_keys_host);
    }

    KeepKeyPromises();
    return 0;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static GenericAgentConfig CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    GenericAgentConfig config = GenericAgentDefaultConfig(cf_keygen);

    while ((c = getopt_long(argc, argv, "dvf:VMsr:", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'f':

            snprintf(CFPRIVKEYFILE, CF_BUFSIZE, "%s.priv", optarg);
            snprintf(CFPUBKEYFILE, CF_BUFSIZE, "%s.pub", optarg);
            break;

        case 'd':
            DEBUG = true;
            break;

        case 'V':
            PrintVersionBanner("cf-key");
            exit(0);

        case 'v':
            VERBOSE = true;
            break;
        case 's':
            SHOWHOSTS = true;
            break;

        case 'r':
            REMOVEKEYS = true;
            remove_keys_host = optarg;
            break;

        case 'h':
            Syntax("cf-key - cfengine's key generator", OPTIONS, HINTS, ID);
            exit(0);

        case 'M':
            ManPage("cf-key - cfengine's key generator", OPTIONS, HINTS, ID);
            exit(0);

        default:
            Syntax("cf-key - cfengine's key generator", OPTIONS, HINTS, ID);
            exit(1);

        }
    }

    return config;
}

/*****************************************************************************/

static bool ShowHost(const char *hostkey, const char *address, bool incoming,
                     const KeyHostSeen *quality, void *ctx)
{
    int *count = ctx;

    char hostname[CF_BUFSIZE];
    strlcpy(hostname, IPString2Hostname(address), CF_BUFSIZE);

    (*count)++;
    printf("%-9.9s %17.17s %-25.25s %s\n", incoming ? "Incoming" : "Outgoing",
           address, hostname, hostkey);

    return true;
}

static void ShowLastSeenHosts()
{
    int count = 0;

    printf("%9.9s %17.17s %-25.25s %15.15s\n", "Direction", "IP", "Name", "Key");

    if (!ScanLastSeenQuality(ShowHost, &count))
    {
        CfOut(cf_error, "", "Unable to show lastseen database");
        return;
    }

    printf("Total Entries: %d\n", count);
}

/*
 * Returns:
 *  amount of keys removed
 *  -1 if there was an error
 */
static int RemovePublicKey(const char *id)
{
    Dir *dirh = NULL;
    int removed = 0;
    char keysdir[CF_BUFSIZE];
    const struct dirent *dirp;
    char suffix[CF_BUFSIZE];

    snprintf(keysdir, CF_BUFSIZE, "%s/ppkeys", CFWORKDIR);
    MapName(keysdir);

    if ((dirh = OpenDirLocal(keysdir)) == NULL)
    {
        if (errno == ENOENT)
        {
            return 0;
        }
        else
        {
            CfOut(cf_error, "opendir", "Unable to open keys directory");
            return -1;
        }
    }

    snprintf(suffix, CF_BUFSIZE, "-%s.pub", id);

    while ((dirp = ReadDir(dirh)) != NULL)
    {
        char *c = strstr(dirp->d_name, suffix);

        if (c && c[strlen(suffix)] == '\0')     /* dirp->d_name ends with suffix */
        {
            char keyfilename[CF_BUFSIZE];

            snprintf(keyfilename, CF_BUFSIZE, "%s/%s", keysdir, dirp->d_name);
            MapName(keyfilename);

            if (unlink(keyfilename) < 0)
            {
                if (errno != ENOENT)
                {
                    CfOut(cf_error, "unlink", "Unable to remove key file %s", dirp->d_name);
                    CloseDir(dirh);
                    return -1;
                }
            }
            else
            {
                removed++;
            }
        }
    }

    if (errno)
    {
        CfOut(cf_error, "ReadDir", "Unable to enumerate files in keys directory");
        CloseDir(dirh);
        return -1;
    }

    CloseDir(dirh);
    return removed;
}

static int RemoveKeys(const char *host)
{
    char ip[CF_BUFSIZE];
    char digest[CF_BUFSIZE];

    strcpy(ip, Hostname2IPString(host));
    Address2Hostkey(ip, digest);

    RemoveHostFromLastSeen(digest);

    int removed_by_ip = RemovePublicKey(ip);
    int removed_by_digest = RemovePublicKey(digest);

    if (removed_by_ip == -1 || removed_by_digest == -1)
    {
        CfOut(cf_error, "", "Unable to remove keys for the host %s",
              remove_keys_host);
        return 255;
    }
    else if (removed_by_ip + removed_by_digest == 0)
    {
        CfOut(cf_error, "", "No keys for host %s were found", remove_keys_host);
        return 1;
    }
    else
    {
        CfOut(cf_inform, "", "Removed %d key(s) for host %s",
              removed_by_ip + removed_by_digest, remove_keys_host);
        return 0;
    }
}
