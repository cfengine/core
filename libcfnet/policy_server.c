/*
  Copyright 2019 Northern.tech AS

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

#include <policy_server.h>

#include <eval_context.h>
#include <addr_lib.h>
#include <communication.h>

#include <assert.h>
#include <logging.h>
#include <string_lib.h>
#include <file_lib.h>

//*******************************************************************
// POLICY SERVER VARIABLES:
//*******************************************************************

static char *POLICY_SERVER      = NULL; // full bootstrap argument
static char *POLICY_SERVER_HOST = NULL; // only host part, if present
static char  POLICY_SERVER_PORT[CF_MAX_PORT_LEN]; // only port part
static char  POLICY_SERVER_IP[CF_MAX_IP_LEN];     // resolved IP


//*******************************************************************
// POLICY SERVER SET FUNCTION:
//*******************************************************************

static bool is_whitespace_empty(const char *str) {
    if(NULL_OR_EMPTY(str))
    {
        return true;
    }
    while (str[0] != '\0')
    {
        if (!isspace(str[0]))
        {
            return false;
        }
        ++str;
    }
    return true;
}

/**
 * @brief Sets both internal C variables as well as policy sys variables.
 *
 * Called at bootstrap and after reading policy_server.dat.
 * Changes sys.policy_hub and sys.policy_hub_port.
 * NULL is a valid input for new_policy_server, everything will be freed and
 * set to NULL.
 *
 * @param ctx EvalContext is used to set related variables
 * @param new_policy_server can be 'host:port', same as policy_server.dat
 */
void PolicyServerSet(const char *new_policy_server)
{
    // Clean up static variables:
    free(POLICY_SERVER);
    free(POLICY_SERVER_HOST);
    POLICY_SERVER         = NULL;
    POLICY_SERVER_HOST    = NULL;

    POLICY_SERVER_IP[0]   = '\0';
    POLICY_SERVER_PORT[0] = '\0';

    if (is_whitespace_empty(new_policy_server))
    {
        return;
    }
    else
    {// Set POLICY_SERVER to be bootstrap argument/policy_server.dat contents
        POLICY_SERVER = xstrdup(new_policy_server);
    }

    // Parse policy server in a separate buffer:
    char *host_or_ip, *port;
    char *buffer = xstrdup(new_policy_server);

    AddressType address_type = ParseHostPort(buffer, &host_or_ip, &port);

    if (address_type == ADDRESS_TYPE_OTHER)
    {
            POLICY_SERVER_HOST = xstrdup(host_or_ip);
    }
    else // ADDRESS_TYPE_IPV4 or ADDRESS_TYPE_IPV6
    {
        assert(strlen(host_or_ip) < CF_MAX_IP_LEN);
        strcpy(POLICY_SERVER_IP, host_or_ip);
    }

    if ( ! NULL_OR_EMPTY(port) )
    {
        if(strlen(port) < CF_MAX_PORT_LEN)
        {
            strcpy(POLICY_SERVER_PORT, port);
        }
        else
        {
            Log(LOG_LEVEL_WARNING,
                "Too long port number in PolicyServerSet: '%s'",
                port);
        }
    }

    free(buffer);
}


//*******************************************************************
// POLICY SERVER GET FUNCTIONS:
//*******************************************************************

static char *CheckEmptyReturn(char *s)
{
    return (NULL_OR_EMPTY(s)) ? NULL : s;
}

/**
 * @brief   Used to access the internal POLICY_SERVER variable.
 * @return  Read-only string, can be 'host:port', same as policy_server.dat
 *          NULL if not bootstrapped ( not set ).
 */
const char *PolicyServerGet()
{
    return POLICY_SERVER; // Don't use CheckedReturn, var should be NULL!
}

/**
 * @brief   Gets the IP address of policy server, does lookup if necessary.
 * @return  Read-only string, can be IPv4 or IPv6.
 *          NULL if not bootstrapped or lookup failed.
 */
const char *PolicyServerGetIP()
{
    if (POLICY_SERVER_HOST == NULL)
    {
        return CheckEmptyReturn(POLICY_SERVER_IP);
    }
    assert(POLICY_SERVER_HOST[0] != '\0');
    int ret = Hostname2IPString(POLICY_SERVER_IP, POLICY_SERVER_HOST,
                                CF_MAX_IP_LEN);
    if (ret != 0) // Lookup failed
    {
        return NULL;
    }
    return CheckEmptyReturn(POLICY_SERVER_IP);
}

/**
 * @brief   Gets the host part of what was bootstrapped to (without port).
 * @return  Read-only string, hostname part of bootstrap argument.
 *          NULL if not bootstrapped or bootstrapped to IP.
 */
const char *PolicyServerGetHost()
{
    return POLICY_SERVER_HOST; // Don't use CheckedReturn, var should be NULL!
}

/**
 * @brief   Gets the port part of the policy server.
 * @return  Read-only null terminated string of port number.
 *          NULL if port not specified, or not bootstrapped at all.
 */
const char *PolicyServerGetPort()
{
    return CheckEmptyReturn(POLICY_SERVER_PORT);
}

//*******************************************************************
// POLICY SERVER FILE FUNCTIONS:
//*******************************************************************

static char *PolicyServerFilename(const char *workdir)
{
   return StringFormat("%s%cpolicy_server.dat", workdir, FILE_SEPARATOR);
}

/**
 * @brief     Reads the policy_server.dat file.
 * @param[in] workdir the directory of policy_server.dat usually GetWorkDir()
 * @return    Trimmed contents of policy_server.dat file. Null terminated.
 */
char *PolicyServerReadFile(const char *workdir)
{
    char contents[CF_MAX_SERVER_LEN] = "";

    char *filename = PolicyServerFilename(workdir);
    FILE *fp = safe_fopen(filename, "r");
    if (fp == NULL)
    {
        Log( LOG_LEVEL_VERBOSE, "Could not open file '%s' (fopen: %s)",
             filename, GetErrorStr() );
        free(filename);
        return NULL;
    }

    if (fgets(contents, CF_MAX_SERVER_LEN, fp) == NULL)
    {
        Log( LOG_LEVEL_VERBOSE, "Could not read file '%s' (fgets: %s)",
             filename, GetErrorStr() );
        free(filename);
        fclose(fp);
        return NULL;
    }

    free(filename);
    fclose(fp);
    char *start = TrimWhitespace(contents);
    return xstrdup(start);
}


/**
 * @brief      Reads and parses the policy_server.dat file.
 *
 * @code{.c}
 * //Typical usage:
 * char *host, *port;
 * bool file_read = PolicyServerParseFile(GetWorkDir(), &host, &port);
 * printf( "host is %s", file_read ? host : "unavailable" );
 * free(host); free(port);
 * @endcode
 *
 * @param[in]  workdir The directory of policy_server.dat usually GetWorkDir()
 * @param[out] host pointer at this address will be hostname string (strdup)
 * @param[out] port pointer at this address will be port string (strdup)
 * @attention  host* and port* must be freed.
 * @return     Boolean indicating success.
 */
bool PolicyServerParseFile(const char *workdir, char **host, char **port)
{
    char *contents = PolicyServerReadFile(workdir);
    if (contents == NULL)
    {
        return false;
    }
    (*host) = NULL;
    (*port) = NULL;

    ParseHostPort(contents, host, port);

    // The file did not contain a host
    if (*host == NULL)
    {
        return false;
    }

    (*host) = xstrdup(*host);
    if (*port != NULL)
    {
        (*port) = xstrdup(*port);
    }
    free(contents);
    return true;
}

/**
 * @brief      Reads and parses the policy_server.dat file.
 *
 * @param[in]  workdir The directory of policy_server.dat usually GetWorkDir()
 * @param[out] ipaddr pointer at this address will be hostname string (strdup)
 * @param[out] port pointer at this address will be port string (strdup)
 * @attention  ipaddr* and port* must be freed.
 * @return     Boolean indicating success.
 * @see        PolicyServerParseFile
 */
bool PolicyServerLookUpFile(const char *workdir, char **ipaddr, char **port)
{
    char *host;
    bool file_read = PolicyServerParseFile(workdir, &host, port);
    if (file_read == false)
    {
        return false;
    }
    char tmp_ipaddr[CF_MAX_IP_LEN];
    if (Hostname2IPString(tmp_ipaddr, host, sizeof(tmp_ipaddr)) == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Unable to resolve policy server host: %s", host);
        free(host);
        free(*port);
        (*port) = NULL;
        return false;
    }
    (*ipaddr) = xstrdup(tmp_ipaddr);
    free(host);
    return true;
}

/**
 * @brief     Write new_policy_server to the policy_server.dat file.
 * @param[in] workdir The directory of policy_server.dat, usually GetWorkDir()
 * @param[in] new_policy_server The host:port string defining the server
 * @return    True if successful
 */
bool PolicyServerWriteFile(const char *workdir, const char *new_policy_server)
{
    char *filename = PolicyServerFilename(workdir);

    FILE *file = safe_fopen(filename, "w");
    if (file == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to write policy server file '%s' (fopen: %s)", filename, GetErrorStr());
        free(filename);
        return false;
    }

    fprintf(file, "%s\n", new_policy_server);
    fclose(file);

    free(filename);
    return true;
}

/**
 * @brief     Remove the policy_server.dat file
 * @param[in] workdir The directory of policy_server.dat, usually GetWorkDir()
 * @return    True if successful
 */
bool PolicyServerRemoveFile(const char *workdir)
{
    char *filename = PolicyServerFilename(workdir);

    if (unlink(filename) != 0)
    {
        Log(LOG_LEVEL_ERR, "Unable to remove file '%s'. (unlink: %s)", filename, GetErrorStr());
        free(filename);
        return false;
    }

    free(filename);
    return true;
}
