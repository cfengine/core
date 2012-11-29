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
#include "cf-execd-runner.h"

#include "files_names.h"
#include "files_interfaces.h"
#include "cfstream.h"
#include "string_lib.h"
#include "pipes.h"
#include "unix.h"

/*******************************************************************/

static const int INF_LINES = -2;

/*******************************************************************/

static int FileChecksum(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1]);
static int CompareResult(char *filename, char *prev_file);
static void MailResult(const ExecConfig *config, char *file);
static int Dialogue(int sd, char *s);

/******************************************************************************/

#if defined(HAVE_PTHREAD)
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
#else /* HAVE_PTHREAD */

static void *ThreadUniqueName(void)
{
    return NULL;
}

#endif /* HAVE_PTHREAD */

static const char *TwinFilename(void)
{
#if defined(__CYGWIN__) || defined(__MINGW32__)
    return "bin-twin/cf-agent.exe";
#else
    return "bin/cf-twin";
#endif
}

static const char *AgentFilename(void)
{
#if defined(__CYGWIN__) || defined(__MINGW32__)
    return "bin/cf-agent.exe";
#else
    return "bin/cf-agent";
#endif
}

static bool TwinExists(void)
{
    char twinfilename[CF_BUFSIZE];
    struct stat sb;

    snprintf(twinfilename, CF_BUFSIZE, "%s/%s", CFWORKDIR, TwinFilename());
    MapName(twinfilename);

    return (stat(twinfilename, &sb) == 0) && (IsExecutable(twinfilename));
}

/* Buffer has to be at least CF_BUFSIZE bytes long */
static void ConstructFailsafeCommand(bool scheduled_run, char *buffer)
{
    bool twin_exists = TwinExists();

    snprintf(buffer, CF_BUFSIZE,
             "\"%s/%s\" -f failsafe.cf "
             "&& \"%s/%s\" -Dfrom_cfexecd%s",
             CFWORKDIR, twin_exists ? TwinFilename() : AgentFilename(),
             CFWORKDIR, AgentFilename(), scheduled_run ? ":scheduled_run" : "");
}

void LocalExec(const ExecConfig *config)
{
    FILE *pp;
    char line[CF_BUFSIZE], line_escaped[sizeof(line) * 2], filename[CF_BUFSIZE], *sp;
    char cmd[CF_BUFSIZE], esc_command[CF_BUFSIZE];
    int print, count = 0;
    void *thread_name;
    time_t starttime = time(NULL);
    char starttime_str[64];
    FILE *fp;
    char canonified_fq_name[CF_BUFSIZE];

    thread_name = ThreadUniqueName();

    cf_strtimestamp_local(starttime, starttime_str);

    CfOut(cf_verbose, "", "------------------------------------------------------------------\n\n");
    CfOut(cf_verbose, "", "  LocalExec(%sscheduled) at %s\n", config->scheduled_run ? "" : "not ", starttime_str);
    CfOut(cf_verbose, "", "------------------------------------------------------------------\n");

/* Need to make sure we have LD_LIBRARY_PATH here or children will die  */

    if (strlen(config->exec_command) > 0)
    {
        strncpy(cmd, config->exec_command, CF_BUFSIZE - 1);

        if (!strstr(cmd, "-Dfrom_cfexecd"))
        {
            strcat(cmd, " -Dfrom_cfexecd");
        }
    }
    else
    {
        ConstructFailsafeCommand(config->scheduled_run, cmd);
    }

    strncpy(esc_command, MapName(cmd), CF_BUFSIZE - 1);

    snprintf(line, CF_BUFSIZE - 1, "_%jd_%s", (intmax_t) starttime, CanonifyName(cf_ctime(&starttime)));

    strlcpy(canonified_fq_name, config->fq_name, CF_BUFSIZE);
    CanonifyNameInPlace(canonified_fq_name);

    snprintf(filename, CF_BUFSIZE - 1, "%s/outputs/cf_%s_%s_%p", CFWORKDIR, canonified_fq_name, line, thread_name);
    MapName(filename);

/* What if no more processes? Could sacrifice and exec() - but we need a sentinel */

    if ((fp = fopen(filename, "w")) == NULL)
    {
        CfOut(cf_error, "fopen", "!! Couldn't open \"%s\" - aborting exec\n", filename);
        return;
    }

#if !defined(__MINGW32__)
/*
 * Don't inherit this file descriptor on fork/exec
 */

    if (fileno(fp) != -1)
    {
        fcntl(fileno(fp), F_SETFD, FD_CLOEXEC);
    }
#endif

    CfOut(cf_verbose, "", " -> Command => %s\n", cmd);

    if ((pp = cf_popen_sh(esc_command, "r")) == NULL)
    {
        CfOut(cf_error, "cf_popen", "!! Couldn't open pipe to command \"%s\"\n", cmd);
        fclose(fp);
        return;
    }

    CfOut(cf_verbose, "", " -> Command is executing...%s\n", esc_command);

    while (!feof(pp))
    {
        if(!IsReadReady(fileno(pp), (config->agent_expireafter * SECONDS_PER_MINUTE)))
        {
            char errmsg[CF_MAXVARSIZE];
            snprintf(errmsg, sizeof(errmsg), "cf-execd: !! Timeout waiting for output from agent (agent_expireafter=%d) - terminating it",
                     config->agent_expireafter);

            CfOut(cf_error, "", "%s", errmsg);
            fprintf(fp, "%s\n", errmsg);
            count++;

            pid_t pid_agent;

            if(PipeToPid(&pid_agent, pp))
            {
                ProcessSignalTerminate(pid_agent);
            }
            else
            {
                CfOut(cf_error, "", "!! Could not get PID of agent");
            }

            break;
        }

        if(!CfReadLine(line, CF_BUFSIZE, pp))
        {
            break;
        }

        if (ferror(pp))
        {
            fflush(pp);
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

        if (print)
        {
            // we must escape print format chars (%) from output

            ReplaceStr(line, line_escaped, sizeof(line_escaped), "%", "%%");

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

                CfOut(cf_inform, "", "%s", line_escaped);
            }

            line[0] = '\0';
            line_escaped[0] = '\0';
        }
    }

    cf_pclose(pp);
    CfDebug("Closing fp\n");
    fclose(fp);

    CfOut(cf_verbose, "", " -> Command is complete\n");

    if (count)
    {
        CfOut(cf_verbose, "", " -> Mailing result\n");
        MailResult(config, filename);
    }
    else
    {
        CfOut(cf_verbose, "", " -> No output\n");
        unlink(filename);
    }
}

static int FileChecksum(char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    FILE *file;
    EVP_MD_CTX context;
    int len;
    unsigned int md_len;
    unsigned char buffer[1024];
    const EVP_MD *md = NULL;

    CfDebug("FileChecksum(%s)\n", filename);

    if ((file = fopen(filename, "rb")) == NULL)
    {
        printf("%s can't be opened\n", filename);
    }
    else
    {
        md = EVP_get_digestbyname("md5");

        if (!md)
        {
            fclose(file);
            return 0;
        }

        EVP_DigestInit(&context, md);

        while ((len = fread(buffer, 1, 1024, file)))
        {
            EVP_DigestUpdate(&context, buffer, len);
        }

        EVP_DigestFinal(&context, digest, &md_len);
        fclose(file);
        return (md_len);
    }

    return 0;
}

static int CompareResult(char *filename, char *prev_file)
{
    int i;
    unsigned char digest1[EVP_MAX_MD_SIZE + 1];
    unsigned char digest2[EVP_MAX_MD_SIZE + 1];
    int md_len1, md_len2;
    FILE *fp;
    int rtn = 0;

    CfOut(cf_verbose, "", "Comparing files  %s with %s\n", prev_file, filename);

    if ((fp = fopen(prev_file, "r")) != NULL)
    {
        fclose(fp);

        md_len1 = FileChecksum(prev_file, digest1);
        md_len2 = FileChecksum(filename, digest2);

        if (md_len1 != md_len2)
        {
            rtn = 1;
        }
        else
        {
            for (i = 0; i < md_len1; i++)
            {
                if (digest1[i] != digest2[i])
                {
                    rtn = 1;
                    break;
                }
            }
        }
    }
    else
    {
        /* no previous file */
        rtn = 1;
    }

    if (!ThreadLock(cft_count))
    {
        CfOut(cf_error, "", "!! Severe lock error when mailing in exec");
        return 1;
    }

/* replace old file with new*/

    unlink(prev_file);

    if (!LinkOrCopy(filename, prev_file, true))
    {
        CfOut(cf_inform, "", "Could not symlink or copy %s to %s", filename, prev_file);
        rtn = 1;
    }

    ThreadUnlock(cft_count);
    return (rtn);
}

static void MailResult(const ExecConfig *config, char *file)
{
    int sd, count = 0, anomaly = false;
    char prev_file[CF_BUFSIZE], vbuff[CF_BUFSIZE];
    struct hostent *hp;
    struct sockaddr_in raddr;
    struct servent *server;
    struct stat statbuf;
#if defined LINUX || defined NETBSD || defined FREEBSD || defined OPENBSD
    time_t now = time(NULL);
#endif
    FILE *fp;

    CfOut(cf_verbose, "", "Mail result...\n");

    if (cfstat(file, &statbuf) == -1)
    {
        return;
    }

    snprintf(prev_file, CF_BUFSIZE - 1, "%s/outputs/previous", CFWORKDIR);
    MapName(prev_file);

    if (statbuf.st_size == 0)
    {
        unlink(file);
        CfDebug("Nothing to report in %s\n", file);
        return;
    }

    if (CompareResult(file, prev_file) == 0)
    {
        CfOut(cf_verbose, "", "Previous output is the same as current so do not mail it\n");
        return;
    }

    if ((strlen(config->mail_server) == 0) || (strlen(config->mail_to_address) == 0))
    {
        /* Syslog should have done this */
        CfOut(cf_verbose, "", "Empty mail server or address - skipping");
        return;
    }

    if (config->mail_max_lines == 0)
    {
        CfDebug("Not mailing: EmailMaxLines was zero\n");
        return;
    }

    CfDebug("Mailing results of (%s) to (%s)\n", file, config->mail_to_address);

/* Check first for anomalies - for subject header */

    if ((fp = fopen(file, "r")) == NULL)
    {
        CfOut(cf_inform, "fopen", "!! Couldn't open file %s", file);
        return;
    }

    while (!feof(fp))
    {
        vbuff[0] = '\0';
        if (fgets(vbuff, CF_BUFSIZE, fp) == NULL)
        {
            break;
        }

        if (strstr(vbuff, "entropy"))
        {
            anomaly = true;
            break;
        }
    }

    fclose(fp);

    if ((fp = fopen(file, "r")) == NULL)
    {
        CfOut(cf_inform, "fopen", "Couldn't open file %s", file);
        return;
    }

    CfDebug("Looking up hostname %s\n\n", config->mail_server);

    if ((hp = gethostbyname(config->mail_server)) == NULL)
    {
        printf("Unknown host: %s\n", config->mail_server);
        printf("Make sure that fully qualified names can be looked up at your site.\n");
        fclose(fp);
        return;
    }

    if ((server = getservbyname("smtp", "tcp")) == NULL)
    {
        CfOut(cf_inform, "getservbyname", "Unable to lookup smtp service");
        fclose(fp);
        return;
    }

    memset(&raddr, 0, sizeof(raddr));

    raddr.sin_port = (unsigned int) server->s_port;
    raddr.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
    raddr.sin_family = AF_INET;

    CfDebug("Connecting...\n");

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        CfOut(cf_inform, "socket", "Couldn't open a socket");
        fclose(fp);
        return;
    }

    if (connect(sd, (void *) &raddr, sizeof(raddr)) == -1)
    {
        CfOut(cf_inform, "connect", "Couldn't connect to host %s\n", config->mail_server);
        fclose(fp);
        cf_closesocket(sd);
        return;
    }

/* read greeting */

    if (!Dialogue(sd, NULL))
    {
        goto mail_err;
    }

    sprintf(vbuff, "HELO %s\r\n", config->fq_name);
    CfDebug("%s", vbuff);

    if (!Dialogue(sd, vbuff))
    {
        goto mail_err;
    }

    if (strlen(config->mail_from_address) == 0)
    {
        sprintf(vbuff, "MAIL FROM: <cfengine@%s>\r\n", config->fq_name);
        CfDebug("%s", vbuff);
    }
    else
    {
        sprintf(vbuff, "MAIL FROM: <%s>\r\n", config->mail_from_address);
        CfDebug("%s", vbuff);
    }

    if (!Dialogue(sd, vbuff))
    {
        goto mail_err;
    }

    sprintf(vbuff, "RCPT TO: <%s>\r\n", config->mail_to_address);
    CfDebug("%s", vbuff);

    if (!Dialogue(sd, vbuff))
    {
        goto mail_err;
    }

    if (!Dialogue(sd, "DATA\r\n"))
    {
        goto mail_err;
    }

    if (anomaly)
    {
        sprintf(vbuff, "Subject: %s **!! [%s/%s]\r\n", MailSubject(), config->fq_name, config->ip_address);
        CfDebug("%s", vbuff);
    }
    else
    {
        sprintf(vbuff, "Subject: %s [%s/%s]\r\n", MailSubject(), config->fq_name, config->ip_address);
        CfDebug("%s", vbuff);
    }

    send(sd, vbuff, strlen(vbuff), 0);

#if defined LINUX || defined NETBSD || defined FREEBSD || defined OPENBSD
    strftime(vbuff, CF_BUFSIZE, "Date: %a, %d %b %Y %H:%M:%S %z\r\n", localtime(&now));
    send(sd, vbuff, strlen(vbuff), 0);
#endif

    if (strlen(config->mail_from_address) == 0)
    {
        sprintf(vbuff, "From: cfengine@%s\r\n", config->fq_name);
        CfDebug("%s", vbuff);
    }
    else
    {
        sprintf(vbuff, "From: %s\r\n", config->mail_from_address);
        CfDebug("%s", vbuff);
    }

    send(sd, vbuff, strlen(vbuff), 0);

    sprintf(vbuff, "To: %s\r\n\r\n", config->mail_to_address);
    CfDebug("%s", vbuff);
    send(sd, vbuff, strlen(vbuff), 0);

    while (!feof(fp))
    {
        vbuff[0] = '\0';
        if (fgets(vbuff, CF_BUFSIZE, fp) == NULL)
        {
            break;
        }

        CfDebug("%s", vbuff);

        if (strlen(vbuff) > 0)
        {
            vbuff[strlen(vbuff) - 1] = '\r';
            strcat(vbuff, "\n");
            count++;
            send(sd, vbuff, strlen(vbuff), 0);
        }

        if ((config->mail_max_lines != INF_LINES) && (count > config->mail_max_lines))
        {
            sprintf(vbuff, "\r\n[Mail truncated by cfengine. File is at %s on %s]\r\n", file, config->fq_name);
            send(sd, vbuff, strlen(vbuff), 0);
            break;
        }
    }

    if (!Dialogue(sd, ".\r\n"))
    {
        CfDebug("mail_err\n");
        goto mail_err;
    }

    Dialogue(sd, "QUIT\r\n");
    CfDebug("Done sending mail\n");
    fclose(fp);
    cf_closesocket(sd);
    return;

  mail_err:

    fclose(fp);
    cf_closesocket(sd);
    CfOut(cf_log, "", "Cannot mail to %s.", config->mail_to_address);
}

static int Dialogue(int sd, char *s)
{
    int sent;
    char ch, f = '\0';
    int charpos, rfclinetype = ' ';

    if ((s != NULL) && (*s != '\0'))
    {
        sent = send(sd, s, strlen(s), 0);
        CfDebug("SENT(%d)->%s", sent, s);
    }
    else
    {
        CfDebug("Nothing to send .. waiting for opening\n");
    }

    charpos = 0;

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

        CfDebug("%c", ch);

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

