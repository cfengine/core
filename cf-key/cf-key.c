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

#include "generic_agent.h"

#include "dbm_api.h"
#include "lastseen.h"
#include "dir.h"
#include "scope.h"
#include "files_copy.h"
#include "files_interfaces.h"
#include "files_hashes.h"
#include "keyring.h"
#include "env_context.h"
#include "crypto.h"
#include "sysinfo.h"
#include "logging_old.h"

#include "cf-key-functions.h"

int SHOWHOSTS = false;
bool REMOVEKEYS = false;
bool LICENSE_INSTALL = false;
char LICENSE_SOURCE[MAX_FILENAME];
const char *remove_keys_host;
static char *print_digest_arg = NULL;
static char *trust_key_arg = NULL;
static char *KEY_PATH;

static GenericAgentConfig *CheckOpts(int argc, char **argv);

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
    {"print-digest", required_argument, 0, 'p'},
    {"trust-key", required_argument, 0, 't'},
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
    "Print digest of the specified public key",
    "Make cf-serverd/cf-agent trust the specified public key",
    NULL
};

/*****************************************************************************/

int main(int argc, char *argv[])
{
    EvalContext *ctx = EvalContextNew();

    GenericAgentConfig *config = CheckOpts(argc, argv);
    GenericAgentConfigApply(ctx, config);

    GenericAgentDiscoverContext(ctx, config);

    if (SHOWHOSTS)
    {
        ShowLastSeenHosts();
        return 0;
    }

    if (print_digest_arg)
    {
        return PrintDigest(print_digest_arg);
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

    if (trust_key_arg)
    {
        return TrustKey(trust_key_arg);
    }

    char *public_key_file, *private_key_file;

    if (KEY_PATH)
    {
        xasprintf(&public_key_file, "%s.pub", KEY_PATH);
        xasprintf(&private_key_file, "%s.priv", KEY_PATH);
    }
    else
    {
        public_key_file = xstrdup(PublicKeyFile(GetWorkDir()));
        private_key_file = xstrdup(PrivateKeyFile(GetWorkDir()));
    }

    KeepKeyPromises(public_key_file, private_key_file);

    free(public_key_file);
    free(private_key_file);

    GenericAgentConfigDestroy(config);
    EvalContextDestroy(ctx);
    return 0;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_KEYGEN);

    while ((c = getopt_long(argc, argv, "dvf:VMp:sr:t:hl:", OPTIONS, &optindex)) != EOF)
    {
        switch ((char) c)
        {
        case 'f':
            KEY_PATH = optarg;
            break;

        case 'd':
            DEBUG = true;
            break;

        case 'V':
            PrintVersion();
            exit(0);

        case 'v':
            VERBOSE = true;
            break;

        case 'p': /* print digest */
            print_digest_arg = optarg;
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

        case 't':
            trust_key_arg = optarg;
            break;

        case 'h':
            Syntax("cf-key", OPTIONS, HINTS, ID, false);
            exit(0);

        case 'M':
            ManPage("cf-key - CFEngine's key generator", OPTIONS, HINTS, ID);
            exit(0);

        default:
            Syntax("cf-key", OPTIONS, HINTS, ID, false);
            exit(1);

        }
    }

    return config;
}
