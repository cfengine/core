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

#include "dbm_api.h"
#include "lastseen.h"
#include "dir.h"
#include "reporting.h"
#include "scope.h"
#include "files_interfaces.h"
#include "keyring.h"
#include "cfstream.h"
#include "communication.h"

#ifdef HAVE_NOVA
#include "license.h"
#endif


int SHOWHOSTS = false;
bool REMOVEKEYS = false;
bool LICENSE_INSTALL = false;
char LICENSE_SOURCE[MAX_FILENAME];
const char *remove_keys_host;

static GenericAgentConfig CheckOpts(int argc, char **argv);

static void ShowLastSeenHosts(void);
static int RemoveKeys(const char *host);
static void KeepKeyPromises(void);

#ifndef HAVE_NOVA
bool LicenseInstall(char *path_source);
#endif

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *ID = "The CFEngine key generator makes key pairs for remote authentication.\n";

static const struct option OPTIONS[17] =
{
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"output-file", required_argument, 0, 'f'},
    {"show-hosts", no_argument, 0, 's'},
    {"remove-keys", required_argument, 0, 'r'},
    {"install-license", required_argument, 0, 'l'},
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
    "Install license without boostrapping (CFEngine Enterprise only)",
    NULL
};

/*****************************************************************************/

int main(int argc, char *argv[])
{
    GenericAgentConfig config = CheckOpts(argc, argv);

    THIS_AGENT_TYPE = AGENT_TYPE_KEYGEN;

    ReportContext *report_context = OpenReports("keygenerator");
    GenericInitialize("keygenerator", config, report_context);

    if (SHOWHOSTS)
    {
        ShowLastSeenHosts();
        return 0;
    }

    if (REMOVEKEYS)
    {
        return RemoveKeys(remove_keys_host);
    }

    if(LICENSE_INSTALL)
    {
        bool success = LicenseInstall(LICENSE_SOURCE);
        return success ? 0 : 1;
    }

    KeepKeyPromises();

    ReportContextDestroy(report_context);
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
    GenericAgentConfig config = GenericAgentDefaultConfig(AGENT_TYPE_KEYGEN);

    while ((c = getopt_long(argc, argv, "dvf:VMsr:hl:", OPTIONS, &optindex)) != EOF)
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

        case 'l':
            LICENSE_INSTALL = true;
            strlcpy(LICENSE_SOURCE, optarg, sizeof(LICENSE_SOURCE));
            break;

        case 'h':
            Syntax("cf-key - CFEngine's key generator", OPTIONS, HINTS, ID);
            exit(0);

        case 'M':
            ManPage("cf-key - CFEngine's key generator", OPTIONS, HINTS, ID);
            exit(0);

        default:
            Syntax("cf-key - CFEngine's key generator", OPTIONS, HINTS, ID);
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
    char timebuf[26];

    char hostname[CF_BUFSIZE];
    strlcpy(hostname, IPString2Hostname(address), CF_BUFSIZE);

    (*count)++;
    printf("%-10.10s %-17.17s %-25.25s %-26.26s %-s\n", incoming ? "Incoming" : "Outgoing",
           address, hostname, cf_strtimestamp_local(quality->lastseen, timebuf), hostkey);

    return true;
}

static void ShowLastSeenHosts()
{
    int count = 0;

    printf("%-10.10s %-17.17s %-25.25s %-26.26s %-s\n", "Direction", "IP", "Name", "Last connection", "Key");

    if (!ScanLastSeenQuality(ShowHost, &count))
    {
        CfOut(cf_error, "", "Unable to show lastseen database");
        return;
    }

    printf("Total Entries: %d\n", count);
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

    if ((removed_by_ip == -1) || (removed_by_digest == -1))
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


static void KeepKeyPromises(void)
{
    unsigned long err;
    RSA *pair;
    FILE *fp;
    struct stat statbuf;
    int fd;
    static char *passphrase = "Cfengine passphrase";
    const EVP_CIPHER *cipher;
    char vbuff[CF_BUFSIZE];

    NewScope("common");

    cipher = EVP_des_ede3_cbc();

    if (cfstat(CFPUBKEYFILE, &statbuf) != -1)
    {
        CfOut(cf_cmdout, "", "A key file already exists at %s\n", CFPUBKEYFILE);
        return;
    }

    if (cfstat(CFPRIVKEYFILE, &statbuf) != -1)
    {
        CfOut(cf_cmdout, "", "A key file already exists at %s\n", CFPRIVKEYFILE);
        return;
    }

    printf("Making a key pair for cfengine, please wait, this could take a minute...\n");

    pair = RSA_generate_key(2048, 35, NULL, NULL);

    if (pair == NULL)
    {
        err = ERR_get_error();
        CfOut(cf_error, "", "Unable to generate key: %s\n", ERR_reason_error_string(err));
        return;
    }

    if (DEBUG)
    {
        RSA_print_fp(stdout, pair, 0);
    }

    fd = open(CFPRIVKEYFILE, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0)
    {
        CfOut(cf_error, "open", "Open %s failed: %s.", CFPRIVKEYFILE, strerror(errno));
        return;
    }

    if ((fp = fdopen(fd, "w")) == NULL)
    {
        CfOut(cf_error, "fdopen", "Couldn't open private key %s.", CFPRIVKEYFILE);
        close(fd);
        return;
    }

    CfOut(cf_verbose, "", "Writing private key to %s\n", CFPRIVKEYFILE);

    if (!PEM_write_RSAPrivateKey(fp, pair, cipher, passphrase, strlen(passphrase), NULL, NULL))
    {
        err = ERR_get_error();
        CfOut(cf_error, "", "Couldn't write private key: %s\n", ERR_reason_error_string(err));
        return;
    }

    fclose(fp);

    fd = open(CFPUBKEYFILE, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0)
    {
        CfOut(cf_error, "open", "Unable to open public key %s.", CFPUBKEYFILE);
        return;
    }

    if ((fp = fdopen(fd, "w")) == NULL)
    {
        CfOut(cf_error, "fdopen", "Open %s failed.", CFPUBKEYFILE);
        close(fd);
        return;
    }

    CfOut(cf_verbose, "", "Writing public key to %s\n", CFPUBKEYFILE);

    if (!PEM_write_RSAPublicKey(fp, pair))
    {
        err = ERR_get_error();
        CfOut(cf_error, "", "Unable to write public key: %s\n", ERR_reason_error_string(err));
        return;
    }

    fclose(fp);

    snprintf(vbuff, CF_BUFSIZE, "%s/randseed", CFWORKDIR);
    RAND_write_file(vbuff);
    cf_chmod(vbuff, 0644);
}


#ifndef HAVE_NOVA
bool LicenseInstall(char *path_source)
{
    CfOut(cf_error, "", "!! License installation only applies to CFEngine Enterprise");

    return false;
}
#endif  /* HAVE_NOVA */
