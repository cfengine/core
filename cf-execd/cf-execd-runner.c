/*
   Copyright 2018 Northern.tech AS

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

#include <cf3.defs.h>
#include <cf-execd-runner.h>

#include <files_names.h>
#include <files_interfaces.h>
#include <hashes.h>
#include <string_lib.h>
#include <pipes.h>
#include <unix.h>
#include <mutex.h>
#include <signals.h>
#include <exec_tools.h>
#include <misc_lib.h>
#include <file_lib.h>
#include <assert.h>
#include <crypto.h>
#include <known_dirs.h>
#include <bootstrap.h>
#include <policy_server.h>
#include <files_hashes.h>
#include <item_lib.h>

#include <cf-windows-functions.h>

/*******************************************************************/

static const int INF_LINES = -2;

/*******************************************************************/

static void MailResult(const ExecConfig *config, const char *file);
static int Dialogue(int sd, const char *s);

/******************************************************************************/

# if defined(__MINGW32__)

static void *ThreadUniqueName(void)
{
    return pthread_self().p;
}

# else /* __MINGW32__ */

static void *ThreadUniqueName(void)
{
    return (void *)pthread_self();
}

# endif /* __MINGW32__ */

static const char *TwinFilename(void)
{
#if defined(_WIN32)
    return "bin-twin/cf-agent.exe";
#else
    return "bin/cf-twin";
#endif
}

static const char *AgentFilename(void)
{
#if defined(_WIN32)
    return "bin/cf-agent.exe";
#else
    return "bin/cf-agent";
#endif
}

static bool TwinExists(void)
{
    char twinfilename[CF_BUFSIZE];
    struct stat sb;

    snprintf(twinfilename, CF_BUFSIZE, "%s/%s", GetWorkDir(), TwinFilename());

    MapName(twinfilename);

    return (stat(twinfilename, &sb) == 0) && (IsExecutable(twinfilename));
}

/* Buffer has to be at least CF_BUFSIZE bytes long */
static void ConstructFailsafeCommand(bool scheduled_run, char *buffer)
{
    bool twin_exists = TwinExists();

    const char* const workdir = GetWorkDir();

    snprintf(buffer, CF_BUFSIZE,
             "\"%s%c%s\" -f failsafe.cf "
             "&& \"%s%c%s\" -Dfrom_cfexecd%s",
             workdir, FILE_SEPARATOR, twin_exists ? TwinFilename() : AgentFilename(),
             workdir, FILE_SEPARATOR, AgentFilename(), scheduled_run ? ",scheduled_run" : "");
}

#ifndef __MINGW32__

#if defined(__hpux) && defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
// Avoid spurious HP-UX GCC type-pun warning on FD_SET() macro
#endif

static bool IsReadReady(int fd, int timeout_sec)
{
    fd_set  rset;
    FD_ZERO(&rset);
    FD_SET(fd, &rset);

    struct timeval tv = {
        .tv_sec = timeout_sec,
        .tv_usec = 0,
    };

    int ret = select(fd + 1, &rset, NULL, NULL, &tv);

    if(ret < 0)
    {
        Log(LOG_LEVEL_ERR, "IsReadReady: Failed checking for data. (select: %s)", GetErrorStr());
        return false;
    }

    if(FD_ISSET(fd, &rset))
    {
        return true;
    }

    if(ret == 0)  // timeout
    {
        return false;
    }

    // can we get here?
    Log(LOG_LEVEL_ERR, "IsReadReady: Unknown outcome (ret > 0 but our only fd is not set). (select: %s)", GetErrorStr());

    return false;
}
#if defined(__hpux) && defined(__GNUC__)
#pragma GCC diagnostic warning "-Wstrict-aliasing"
#endif

#endif  /* __MINGW32__ */

void LocalExec(const ExecConfig *config)
{
    time_t starttime = time(NULL);

    void *thread_name = ThreadUniqueName();

    {
        char starttime_str[64];
        cf_strtimestamp_local(starttime, starttime_str);

        Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");
        Log(LOG_LEVEL_VERBOSE, "  LocalExec(%sscheduled) at %s", config->scheduled_run ? "" : "not ", starttime_str);
        Log(LOG_LEVEL_VERBOSE, "----------------------------------------------------------------");
    }

/* Need to make sure we have LD_LIBRARY_PATH here or children will die  */

    char cmd[CF_BUFSIZE];
    if (strlen(config->exec_command) > 0)
    {
        strlcpy(cmd, config->exec_command, CF_BUFSIZE);
    }
    else
    {
        ConstructFailsafeCommand(config->scheduled_run, cmd);
    }

    char esc_command[CF_BUFSIZE];
    strlcpy(esc_command, MapName(cmd), CF_BUFSIZE);


    char filename[CF_BUFSIZE];
    {
        char line[CF_BUFSIZE];
        snprintf(line, CF_BUFSIZE, "_%jd_%s", (intmax_t) starttime, CanonifyName(ctime(&starttime)));
        {
            char canonified_fq_name[CF_BUFSIZE];

            strlcpy(canonified_fq_name, config->fq_name, CF_BUFSIZE);
            CanonifyNameInPlace(canonified_fq_name);

            snprintf(filename, CF_BUFSIZE, "%s/outputs/cf_%s_%s_%p",
                     GetWorkDir(), canonified_fq_name, line, thread_name);

            MapName(filename);
        }
    }


/* What if no more processes? Could sacrifice and exec() - but we need a sentinel */

    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open '%s' - aborting exec. (fopen: %s)", filename, GetErrorStr());
        return;
    }

/*
 * Don't inherit this file descriptor on fork/exec
 */

    if (fileno(fp) != -1)
    {
        SetCloseOnExec(fileno(fp), true);
    }

    Log(LOG_LEVEL_VERBOSE, "Command => %s", cmd);

    FILE *pp = cf_popen_sh(esc_command, "r");
    if (!pp)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open pipe to command '%s'. (cf_popen: %s)", cmd, GetErrorStr());
        fclose(fp);
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Command is executing...%s", esc_command);

    int count = 0;
    int complete = false;
    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);

    while (!IsPendingTermination())
    {
        if (!IsReadReady(fileno(pp),
                         config->agent_expireafter * SECONDS_PER_MINUTE))
        {
            char errmsg[] =
                "cf-execd: timeout waiting for output from agent"
                " (agent_expireafter=%d) - terminating it\n";

            fprintf(fp, errmsg, config->agent_expireafter);
            /* Trim '\n' before Log()ing. */
            errmsg[strlen(errmsg) - 1] = '\0';
            Log(LOG_LEVEL_NOTICE, errmsg, config->agent_expireafter);
            count++;

            pid_t pid_agent;

            if (PipeToPid(&pid_agent, pp))
            {
                ProcessSignalTerminate(pid_agent);
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Could not get PID of agent");
            }

            break;
        }

        ssize_t res = CfReadLine(&line, &line_size, pp);
        if (res == -1)
        {
            if (feof(pp))
            {
                complete = true;
            }
            else
            {
                Log(LOG_LEVEL_ERR,
                    "Unable to read output from command '%s'. (cfread: %s)",
                    cmd, GetErrorStr());
            }
            break;
        }

        const char *sp = line;
        while (*sp != '\0' && isspace(*sp))
        {
            sp++;
        }

        if (*sp != '\0') /* line isn't entirely blank */
        {
            char *line_escaped = xmalloc(2 * line_size);
            ReplaceStr(line, line_escaped, 2 * line_size, "%", "%%");

            fprintf(fp, "%s\n", line_escaped);
            count++;

            /* If we can't send mail, log to syslog */

            if (strlen(config->mail_to_address) == 0)
            {
                strncat(line_escaped, "\n", sizeof(line_escaped) - 1 - strlen(line_escaped));
                if ((strchr(line_escaped, '\n')) == NULL)
                {
                    line_escaped[sizeof(line_escaped) - 2] = '\n';
                }

                Log(LOG_LEVEL_INFO, "%s", line_escaped);
            }

            line[0] = '\0';
            free(line_escaped);
        }
    }

    free(line);
    cf_pclose(pp);
    Log(LOG_LEVEL_DEBUG, "Closing fp");
    fclose(fp);

    Log(LOG_LEVEL_VERBOSE,
        complete ? "Command is complete" : "Terminated command");

    if (count)
    {
        Log(LOG_LEVEL_VERBOSE, "Mailing result");
        MailResult(config, filename);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "No output");
        unlink(filename);
    }
}

// Returns true if line is filtered, IOW should not be included.
static bool LineIsFiltered(const ExecConfig *config,
                           const char *line)
{
    // Check whether the line matches mail filters
    int include_filter_len = SeqLength(config->mailfilter_include_regex);
    int exclude_filter_len = SeqLength(config->mailfilter_exclude_regex);
    // Count messages as matched in include set if there is no include set.
    bool included = (include_filter_len == 0);
    bool excluded = false;
    if (include_filter_len > 0)
    {
        for (int i = 0; i < include_filter_len; i++)
        {
            if (StringMatchFullWithPrecompiledRegex(SeqAt(config->mailfilter_include_regex, i),
                                                    line))
            {
                included = true;
            }
        }
    }
    if (exclude_filter_len > 0)
    {
        for (int i = 0; i < exclude_filter_len; i++)
        {
            if (StringMatchFullWithPrecompiledRegex(SeqAt(config->mailfilter_exclude_regex, i),
                                                    line))
            {
                excluded = true;
            }
        }
    }
    return !included || excluded;
}

static bool CompareResultEqualOrFiltered(const ExecConfig *config,
                                         const char *filename,
                                         const char *prev_file)
{
    Log(LOG_LEVEL_VERBOSE, "Comparing files  %s with %s", prev_file, filename);

    bool rtn = true;

    FILE *old_fp = safe_fopen(prev_file, "r");
    FILE *new_fp = safe_fopen(filename, "r");
    if (new_fp)
    {
        const char *errptr;
        int erroffset;
        pcre_extra *regex_extra = NULL;
        // Match timestamps and remove them. Not Y21K safe! :-)
        pcre *regex = pcre_compile(LOGGING_TIMESTAMP_REGEX, PCRE_MULTILINE, &errptr, &erroffset, NULL);
        if (!regex)
        {
            UnexpectedError("Compiling regular expression failed");
            rtn = false;
        }
        else
        {
            regex_extra = pcre_study(regex, 0, &errptr);
        }

        size_t old_line_size = CF_BUFSIZE;
        char *old_line = xmalloc(old_line_size);
        size_t new_line_size = CF_BUFSIZE;
        char *new_line = xmalloc(new_line_size);
        bool any_new_msg_present = false;
        while (regex)
        {
            char *old_msg = NULL;
            if (old_fp)
            {
                while (CfReadLine(&old_line, &old_line_size, old_fp) >= 0)
                {
                    if (!LineIsFiltered(config, old_line))
                    {
                        old_msg = old_line;
                        break;
                    }
                }
            }

            char *new_msg = NULL;
            while (CfReadLine(&new_line, &new_line_size, new_fp) >= 0)
            {
                if (!LineIsFiltered(config, new_line))
                {
                    any_new_msg_present = true;
                    new_msg = new_line;
                    break;
                }
            }

            if (!old_msg || !new_msg)
            {
                // Return difference in most cases, when there is a new
                // message line, but if there isn't one, return equal, even
                // if strictly speaking they aren't, since we don't want to
                // send an empty email.
                if (any_new_msg_present && old_msg != new_msg)
                {
                    rtn = false;
                }
                break;
            }

            // Remove timestamps from lines before comparison.
            char *index;
            if (pcre_exec(regex, regex_extra, old_msg, strlen(old_msg), 0, 0, NULL, 0) >= 0)
            {
                index = strstr(old_msg, ": ");
                if (index != NULL)
                {
                    old_msg = index + 2;
                }
            }
            if (pcre_exec(regex, regex_extra, new_msg, strlen(new_msg), 0, 0, NULL, 0) >= 0)
            {
                index = strstr(new_msg, ": ");
                if (index != NULL)
                {
                    new_msg = index + 2;
                }
            }

            if (strcmp(old_msg, new_msg) != 0)
            {
                rtn = false;
                break;
            }
        }

        free(old_line);
        free(new_line);

        if (regex_extra)
        {
            free(regex_extra);
        }
        pcre_free(regex);
    }
    else
    {
        /* no previous file */
        rtn = false;
    }

    if (old_fp)
    {
        fclose(old_fp);
    }
    if (new_fp)
    {
        fclose(new_fp);
    }

    if (!ThreadLock(cft_count))
    {
        Log(LOG_LEVEL_ERR, "Severe lock error when mailing in exec");
        return 1;
    }

/* replace old file with new*/

    unlink(prev_file);

    if (!LinkOrCopy(filename, prev_file, true))
    {
        Log(LOG_LEVEL_INFO, "Could not symlink or copy '%s' to '%s'", filename, prev_file);
        rtn = false;
    }

    ThreadUnlock(cft_count);
    return rtn;
}

#ifndef TEST_CF_EXECD
int ConnectToSmtpSocket(const ExecConfig *config)
{
    struct hostent *hp = gethostbyname(config->mail_server);
    if (!hp)
    {
        Log(LOG_LEVEL_ERR, "Mail report: unknown host '%s' ('smtpserver' in body executor control). Make sure that fully qualified names can be looked up at your site.",
                config->mail_server);
        return -1;
    }

    struct servent *server = getservbyname("smtp", "tcp");
    if (!server)
    {
        Log(LOG_LEVEL_ERR, "Mail report: unable to lookup smtp service. (getservbyname: %s)", GetErrorStr());
        return -1;
    }

    struct sockaddr_in raddr;
    memset(&raddr, 0, sizeof(raddr));

    raddr.sin_port = (unsigned int) server->s_port;
    raddr.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
    raddr.sin_family = AF_INET;

    Log(LOG_LEVEL_DEBUG, "Mail report: connecting...");

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1)
    {
        Log(LOG_LEVEL_ERR, "Mail report: couldn't open a socket. (socket: %s)", GetErrorStr());
        return -1;
    }

    if (connect(sd, (void *) &raddr, sizeof(raddr)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Mail report: couldn't connect to host '%s'. (connect: %s)",
            config->mail_server, GetErrorStr());
        cf_closesocket(sd);
        return -1;
    }

    return sd;
}
#endif // !TEST_CF_EXECD

static void MailResult(const ExecConfig *config, const char *file)
{
    /* Must be initialised to NULL, so that free(line) works
       when  goto mail_err. */
    char *line = NULL;

#if defined __linux__ || defined __NetBSD__ || defined __FreeBSD__ || defined __OpenBSD__
    time_t now = time(NULL);
#endif

    Log(LOG_LEVEL_VERBOSE, "Mail report: sending result...");

    {
        struct stat statbuf;
        if (stat(file, &statbuf) == -1)
        {
            Log(LOG_LEVEL_ERR, "Mail report: failed to stat file '%s' [errno: %d]", file, errno);
            return;
        }

        if (statbuf.st_size == 0)
        {
            unlink(file);
            Log(LOG_LEVEL_DEBUG, "Mail report: nothing to report in file '%s'", file);
            return;
        }
    }

    {
        char prev_file[CF_BUFSIZE];
        snprintf(prev_file, CF_BUFSIZE, "%s/outputs/previous", GetWorkDir());
        MapName(prev_file);

        if (CompareResultEqualOrFiltered(config, file, prev_file))
        {
            Log(LOG_LEVEL_VERBOSE, "Mail report: previous output is the same as current so do not mail it");
            return;
        }
    }

    if ((strlen(config->mail_server) == 0) || (strlen(config->mail_to_address) == 0))
    {
        /* Syslog should have done this */
        Log(LOG_LEVEL_VERBOSE, "Mail report: empty mail server or address - skipping");
        return;
    }

    if (config->mail_max_lines == 0)
    {
        Log(LOG_LEVEL_DEBUG, "Mail report: not mailing because EmailMaxLines was zero");
        return;
    }

    Log(LOG_LEVEL_DEBUG, "Mail report: mailing results of '%s' to '%s'", file, config->mail_to_address);

    FILE *fp = fopen(file, "r");
    if (fp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Mail report: couldn't open file '%s'. (fopen: %s)", file, GetErrorStr());
        return;
    }

    int sd = ConnectToSmtpSocket(config);
    if (sd < 0)
    {
        fclose(fp);
        return;
    }

/* read greeting */

    if (!Dialogue(sd, NULL))
    {
        goto mail_err;
    }

    char vbuff[CF_BUFSIZE];
    snprintf(vbuff, sizeof(vbuff), "HELO %s\r\n", config->fq_name);
    Log(LOG_LEVEL_DEBUG, "Mail report: %s", vbuff);

    if (!Dialogue(sd, vbuff))
    {
        goto mail_err;
    }

    if (strlen(config->mail_from_address) == 0)
    {
        snprintf(vbuff, sizeof(vbuff), "MAIL FROM: <cfengine@%s>\r\n",
                 config->fq_name);
        Log(LOG_LEVEL_DEBUG, "Mail report: %s", vbuff);
    }
    else
    {
        snprintf(vbuff, sizeof(vbuff), "MAIL FROM: <%s>\r\n",
                 config->mail_from_address);
        Log(LOG_LEVEL_DEBUG, "Mail report: %s", vbuff);
    }

    if (!Dialogue(sd, vbuff))
    {
        goto mail_err;
    }

    snprintf(vbuff, sizeof(vbuff), "RCPT TO: <%s>\r\n",
             config->mail_to_address);
    Log(LOG_LEVEL_DEBUG, "Mail report: %s", vbuff);

    if (!Dialogue(sd, vbuff))
    {
        goto mail_err;
    }

    if (!Dialogue(sd, "DATA\r\n"))
    {
        goto mail_err;
    }

    if (SafeStringLength(config->mail_subject) == 0)
    {
        snprintf(vbuff, sizeof(vbuff), "Subject: [%s/%s]\r\n", config->fq_name, config->ip_address);
        Log(LOG_LEVEL_DEBUG, "Mail report: %s", vbuff);
    }
    else
    {
        snprintf(vbuff, sizeof(vbuff), "Subject: %s\r\n", config->mail_subject);
        Log(LOG_LEVEL_DEBUG, "Mail report: %s", vbuff);
    }

    send(sd, vbuff, strlen(vbuff), 0);

    /* Send X-CFEngine SMTP header */
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    char buffer[CF_HOSTKEY_STRING_SIZE];

    char *existing_policy_server = PolicyServerReadFile(GetWorkDir());
    if (!existing_policy_server)
    {
        existing_policy_server = xstrdup("(none)");
    }

    HashPubKey(PUBKEY, digest, CF_DEFAULT_DIGEST);

    snprintf(vbuff, sizeof(vbuff),
             "X-CFEngine: vfqhost=\"%s\";ip-addresses=\"%s\";policyhub=\"%s\";pkhash=\"%s\"\r\n",
             VFQNAME, config->ip_addresses, existing_policy_server,
             HashPrintSafe(buffer, sizeof(buffer), digest, CF_DEFAULT_DIGEST, true));

    send(sd, vbuff, strlen(vbuff), 0);
    free(existing_policy_server);

#if defined __linux__ || defined __NetBSD__ || defined __FreeBSD__ || defined __OpenBSD__
    strftime(vbuff, CF_BUFSIZE, "Date: %a, %d %b %Y %H:%M:%S %z\r\n", localtime(&now));
    send(sd, vbuff, strlen(vbuff), 0);
#endif

    if (strlen(config->mail_from_address) == 0)
    {
        snprintf(vbuff, sizeof(vbuff), "From: cfengine@%s\r\n",
                 config->fq_name);
        Log(LOG_LEVEL_DEBUG, "Mail report: %s", vbuff);
    }
    else
    {
        snprintf(vbuff, sizeof(vbuff), "From: %s\r\n",
                 config->mail_from_address);
        Log(LOG_LEVEL_DEBUG, "Mail report: %s", vbuff);
    }

    send(sd, vbuff, strlen(vbuff), 0);

    snprintf(vbuff, sizeof(vbuff), "To: %s\r\n\r\n", config->mail_to_address);
    Log(LOG_LEVEL_DEBUG, "Mail report: %s", vbuff);
    send(sd, vbuff, strlen(vbuff), 0);

    size_t line_size = CF_BUFSIZE;
    line = xmalloc(line_size);
    ssize_t n_read;

    int count = 0;
    while ((n_read = CfReadLine(&line, &line_size, fp)) > 0)
    {
        if (LineIsFiltered(config, line))
        {
            continue;
        }
        if (send(sd, line, n_read, 0) == -1 ||
            send(sd, "\r\n", 2, 0) == -1)
        {
            Log(LOG_LEVEL_ERR, "Error while sending mail to mailserver "
                "'%s'. (send: '%s')", config->mail_server, GetErrorStr());
            goto mail_err;
        }

        count++;
        if ((config->mail_max_lines != INF_LINES) &&
            (count >= config->mail_max_lines))
        {
            snprintf(line, line_size,
                     "\r\n[Mail truncated by CFEngine. File is at %s on %s]\r\n",
                     file, config->fq_name);
            if (send(sd, line, strlen(line), 0) == -1)
            {
                Log(LOG_LEVEL_ERR, "Error while sending mail to mailserver "
                    "'%s'. (send: '%s')", config->mail_server, GetErrorStr());
                goto mail_err;
            }
            break;
        }
    }

    if (!Dialogue(sd, ".\r\n"))
    {
        Log(LOG_LEVEL_DEBUG, "Mail report: mail_err\n");
        goto mail_err;
    }

    Dialogue(sd, "QUIT\r\n");
    Log(LOG_LEVEL_DEBUG, "Mail report: done sending mail");
    free(line);
    fclose(fp);
    cf_closesocket(sd);
    return;

  mail_err:

    free(line);
    fclose(fp);
    cf_closesocket(sd);
    Log(LOG_LEVEL_ERR, "Mail report: cannot mail to %s.", config->mail_to_address);
}

static int Dialogue(int sd, const char *s)
{
    if ((s != NULL) && (*s != '\0'))
    {
        int sent = send(sd, s, strlen(s), 0);
        Log(LOG_LEVEL_DEBUG, "SENT(%d) -> '%s'", sent, s);
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "Nothing to send .. waiting for opening");
    }

    int charpos = 0;
    int rfclinetype = ' ';

    char ch, f = '\0';
    while (recv(sd, &ch, 1, 0))
    {
        charpos++;

        if (f == '\0')
        {
            f = ch;
        }

        if (charpos == 4)       /* Multiline RFC in form 222-Message with hyphen at pos 4 */
        {
            rfclinetype = ch;
        }

        if ((ch == '\n') || (ch == '\0'))
        {
            charpos = 0;

            if (rfclinetype == ' ')
            {
                break;
            }
        }
    }

    return ((f == '2') || (f == '3'));  /* return code 200 or 300 from smtp */
}
