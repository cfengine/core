/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <generic_agent.h>

#include <dbm_api.h>
#include <lastseen.h>
#include <dir.h>
#include <scope.h>
#include <files_copy.h>
#include <files_interfaces.h>
#include <files_hashes.h>
#include <keyring.h>
#include <eval_context.h>
#include <crypto.h>
#include <known_dirs.h>
#include <man.h>
#include <signals.h>

#include <cf-key-functions.h>

int SHOWHOSTS = false;                                          /* GLOBAL_A */
bool FORCEREMOVAL = false;                                      /* GLOBAL_A */
bool REMOVEKEYS = false;                                        /* GLOBAL_A */
bool LICENSE_INSTALL = false;                                   /* GLOBAL_A */
char LICENSE_SOURCE[MAX_FILENAME] = "";                         /* GLOBAL_A */
const char *remove_keys_host = NULL;                            /* GLOBAL_A */
static char *print_digest_arg = NULL;                           /* GLOBAL_A */
static char *trust_key_arg = NULL;                              /* GLOBAL_A */
static char *KEY_PATH = NULL;                                   /* GLOBAL_A */
bool LOOKUP_HOSTS = true;                                       /* GLOBAL_A */

static GenericAgentConfig *CheckOpts(int argc, char **argv);

/*******************************************************************/
/* Command line options                                            */
/*******************************************************************/

static const char *const CF_KEY_SHORT_DESCRIPTION =
    "make private/public key-pairs for CFEngine authentication";

static const char *const CF_KEY_MANPAGE_LONG_DESCRIPTION =
    "The CFEngine key generator makes key pairs for remote authentication.\n";

#define TIMESTAMP_VAL 1234 // Anything outside ASCII range.
static const struct option OPTIONS[] =
{
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"output-file", required_argument, 0, 'f'},
    {"show-hosts", no_argument, 0, 's'},
    {"remove-keys", required_argument, 0, 'r'},
    {"force-removal", no_argument, 0, 'x'},
    {"install-license", required_argument, 0, 'l'},
    {"print-digest", required_argument, 0, 'p'},
    {"trust-key", required_argument, 0, 't'},
    {"color", optional_argument, 0, 'C'},
    {"timestamp", no_argument, 0, TIMESTAMP_VAL},
    {"numeric", no_argument, 0, 'n'},
    {NULL, 0, 0, '\0'}
};

static const char *const HINTS[] =
{
    "Print the help message",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    "Specify an alternative output file than the default (localhost)",
    "Show lastseen hostnames and IP addresses",
    "Remove keys for specified hostname/IP",
    "Force removal of keys (USE AT YOUR OWN RISK)",
    "Install license file on Enterprise server (CFEngine Enterprise Only)",
    "Print digest of the specified public key",
    "Make cf-serverd/cf-agent trust the specified public key. Argument value is of the form [[USER@]IPADDR:]FILENAME where FILENAME is the local path of the public key for client at IPADDR address.",
    "Enable colorized output. Possible values: 'always', 'auto', 'never'. If option is used, the default value is 'auto'",
    "Log timestamps on each line of log output",
    "Do not lookup host names",
    NULL
};

/*****************************************************************************/

typedef void (*CfKeySigHandler)(int signum);
bool cf_key_interrupted = false;

static void handleShowKeysSignal(int signum)
{
    cf_key_interrupted = true;

    signal(signum, handleShowKeysSignal);
}

static void SetupSignalsForCfKey(CfKeySigHandler sighandler)
{
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, HandleSignalsForAgent);
    signal(SIGUSR2, HandleSignalsForAgent);
}

int main(int argc, char *argv[])
{
    SetupSignalsForCfKey(HandleSignalsForAgent);

    GenericAgentConfig *config = CheckOpts(argc, argv);
    EvalContext *ctx = EvalContextNew();
    GenericAgentConfigApply(ctx, config);

    GenericAgentDiscoverContext(ctx, config);

    if (SHOWHOSTS)
    {
        SetupSignalsForCfKey(handleShowKeysSignal);
        ShowLastSeenHosts();
        return 0;
    }

    if (print_digest_arg)
    {
        return PrintDigest(print_digest_arg);
    }

    GenericAgentPostLoadInit(ctx);

    if (REMOVEKEYS)
    {
        int status;
        if (FORCEREMOVAL)
        {
            if (!strncmp(remove_keys_host, "SHA=", 3) ||
                !strncmp(remove_keys_host, "MD5=", 3))
            {
                status = ForceKeyRemoval(remove_keys_host);
            }
            else
            {
                status = ForceIpAddressRemoval(remove_keys_host);
            }
        }
        else
        {
            status = RemoveKeys(remove_keys_host, true);
            if (status == 0 || status == 1)
            {
                Log (LOG_LEVEL_VERBOSE,
                    "Forced removal of entry '%s' was successful",
                    remove_keys_host);
                return 0;
            }
        }
        return status;
    }

    if(LICENSE_INSTALL)
    {
        bool success = LicenseInstall(LICENSE_SOURCE);
        return success ? 0 : 1;
    }

    if (trust_key_arg != NULL)
    {
        char *filename, *ipaddr, *username;
        /* We will modify the argument to --trust-key. */
        char *arg = xstrdup(trust_key_arg);

        ParseKeyArg(arg, &filename, &ipaddr, &username);

        /* Server IP address required to trust key on the client side. */
        if (ipaddr == NULL)
        {
            Log(LOG_LEVEL_NOTICE, "Establishing trust might be incomplete. "
                "For completeness, use --trust-key IPADDR:filename");
        }

        bool ret = TrustKey(filename, ipaddr, username);

        free(arg);
        return ret ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    char *public_key_file, *private_key_file;

    if (KEY_PATH)
    {
        xasprintf(&public_key_file, "%s.pub", KEY_PATH);
        xasprintf(&private_key_file, "%s.priv", KEY_PATH);
    }
    else
    {
        public_key_file = PublicKeyFile(GetWorkDir());
        private_key_file = PrivateKeyFile(GetWorkDir());
    }

    KeepKeyPromises(public_key_file, private_key_file);

    free(public_key_file);
    free(private_key_file);

    GenericAgentFinalize(ctx, config);
    return 0;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static GenericAgentConfig *CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    int c;
    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_KEYGEN, GetTTYInteractive());

    while ((c = getopt_long(argc, argv, "dvf:VMp:sr:xt:hl:C::n",
                            OPTIONS, NULL))
           != -1)
    {
        switch (c)
        {
        case 'f':
            KEY_PATH = optarg;
            break;

        case 'd':
            LogSetGlobalLevel(LOG_LEVEL_DEBUG);
            break;

        case 'V':
            {
                Writer *w = FileWriter(stdout);
                GenericAgentWriteVersion(w);
                FileWriterDetach(w);
            }
            exit(EXIT_SUCCESS);

        case 'v':
            LogSetGlobalLevel(LOG_LEVEL_VERBOSE);
            break;

        case 'p': /* print digest */
            print_digest_arg = optarg;
            break;

        case 's':
            SHOWHOSTS = true;
            break;

        case 'x':
            FORCEREMOVAL = true;
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
            {
                Writer *w = FileWriter(stdout);
                WriterWriteHelp(w, "cf-key", OPTIONS, HINTS, false, NULL);
                FileWriterDetach(w);
            }
            exit(EXIT_SUCCESS);

        case 'M':
            {
                Writer *out = FileWriter(stdout);
                ManPageWrite(out, "cf-key", time(NULL),
                             CF_KEY_SHORT_DESCRIPTION,
                             CF_KEY_MANPAGE_LONG_DESCRIPTION,
                             OPTIONS, HINTS,
                             false);
                FileWriterDetach(out);
                exit(EXIT_SUCCESS);
            }

        case 'C':
            if (!GenericAgentConfigParseColor(config, optarg))
            {
                exit(EXIT_FAILURE);
            }
            break;

        case TIMESTAMP_VAL:
            LoggingEnableTimestamps(true);
            break;

        case 'n':
            LOOKUP_HOSTS = false;
            break;

        default:
            {
                Writer *w = FileWriter(stdout);
                WriterWriteHelp(w, "cf-key", OPTIONS, HINTS, false, NULL);
                FileWriterDetach(w);
            }
            exit(EXIT_FAILURE);

        }
    }

    return config;
}
