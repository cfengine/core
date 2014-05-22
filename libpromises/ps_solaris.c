#include <ps_solaris.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <sys/ttold.h>
#include <libelf.h>
#include <gelf.h>
#include <locale.h>
#include <wctype.h>
#include <stdarg.h>
#include <sys/proc.h>
#include <zone.h>
#include "../cf-ps/priv_utils.h"
#include "../cf-ps/unistd.h"
#include "../cf-ps/procfs.h"
#include "../cf-ps/param.h"



#define	NARG	100
#define	_SYSCALL32

struct psent {
    psinfo_t *psinfo;
    char *psargs;
    int found;
};
int GetPsArgs(const char *process, char *args)
{
    psinfo_t info;		/* process information structure from /proc */
    char *psargs = NULL;	/* pointer to buffer for -w and -ww options */
    char *svpsargs = NULL;
    struct psent *psent;
    int entsize;
    int nent;
    int	    eflg = 0;	/* Display environment as well as arguments */
    char   *procdir = "/proc";	/* standard /proc directory */
    pid_t	pid;		/* pid: process id */
    pid_t	ppid;		/* ppid: parent process id */
    int	   found;
    char	psname[100];
    char	asname[100];
    int	pdlen;

    (void) setlocale(LC_ALL, "");

    /*
     * This program needs the proc_owner privilege
     */
    (void) __init_suid_priv(PU_CLEARLIMITSET, PRIV_PROC_OWNER,
        (char *)NULL);

    twidth = NCARGS;

    /* allocate an initial guess for the number of processes */
    entsize = 1024;
    psent = malloc(entsize * sizeof (struct psent));
    if (psent == NULL) {
        (void) fprintf(stderr, "ps: no memory\n");
        return (-1);
    }
    nent = 0;	/* no active entries yet */


    if (twidth > PRARGSZ && (psargs = malloc(twidth)) == NULL) {
        (void) fprintf(stderr, "ps: no memory\n");
        return (-1);
    }
    svpsargs = psargs;


    (void) strcpy(psname, procdir);
    pdlen = strlen(psname);
    psname[pdlen++] = '/';

    int finished = 0;
    /* for each active process --- */
    while (!finished) {
        int	psfd;	/* file descriptor for /proc/nnnnn/psinfo */
        int	asfd;	/* file descriptor for /proc/nnnnn/as */

        if (strlen(process) > 0)
            (void) strcpy(psname + pdlen, process);
        else
            return -1;
        (void) strcpy(asname, psname);
        (void) strcat(psname, "/psinfo");
        (void) strcat(asname, "/as");
retry:
        if ((psfd = open(psname, O_RDONLY)) == -1)
            continue;
        asfd = -1;
        if (psargs != NULL) {

            /* now we need the proc_owner privilege */
            (void) __priv_bracket(PRIV_ON);

            asfd = open(asname, O_RDONLY);

            /* drop proc_owner privilege after open */
            (void) __priv_bracket(PRIV_OFF);
        }

        /*
         * Get the info structure for the process
         */
        if (read(psfd, &info, sizeof (info)) != sizeof (info)) {
            int	saverr = errno;

            (void) close(psfd);
            if (asfd > 0)
                (void) close(asfd);
            if (saverr == EAGAIN)
                goto retry;
            if (saverr != ENOENT)
                Log(LOG_LEVEL_ERR, "rps.c read() failed on %s.", psname);
            continue;
        }
        (void) close(psfd);

        found = 0;
        if (info.pr_lwp.pr_state == 0)		/* can't happen? */
            goto closeit;
        pid = info.pr_pid;
        ppid = info.pr_ppid;


        /*
         * Read the args for the -w and -ww cases
         */
        if (asfd > 0) {
            if ((psargs != NULL &&
                preadargs(asfd, &info, psargs, twidth) == -1) ||
                (eflg && preadenvs(asfd, &info, psargs, twidth) == -1)) {
                int	saverr = errno;

                (void) close(asfd);
                if (saverr == EAGAIN)
                    goto retry;
                if (saverr != ENOENT)
                     Log(LOG_LEVEL_ERR, "rps.c read() failed on %s.", asname);
                continue;
            }
        } else {
            psargs = info.pr_psargs;
        }

        if (nent >= entsize) {
            entsize *= 2;
            psent = (struct psent *)realloc((char *)psent,
                entsize * sizeof (struct psent));
            if (psent == NULL) {
                (void) fprintf(stderr, "ps: no memory\n");
                return (-1);
            }
        }
        if ((psent[nent].psinfo = malloc(sizeof (psinfo_t)))
            == NULL) {
            (void) fprintf(stderr, "ps: no memory\n");
            return (-1);
        }
        *psent[nent].psinfo = info;
        if (psargs == NULL)
            psent[nent].psargs = NULL;
        else {
            if ((psent[nent].psargs = malloc(strlen(psargs)+1))
                == NULL) {
                (void) fprintf(stderr, "ps: no memory\n");
                return (-1);
            }
            (void) strcpy(args, psargs);
        }
        psent[nent].found = found;
        nent++;
closeit:
        if (asfd > 0)
            (void) close(asfd);
        psargs = svpsargs;
        finished = 1;
    }

    /* revert to non-privileged user */
    (void) __priv_relinquish();

    struct psent *pp = &psent[0];

    return (0);
}



/*
 * Read the process arguments from the process.
 * This allows >PRARGSZ characters of arguments to be displayed but,
 * unlike pr_psargs[], the process may have changed them.
 */
static int preadargs(int pfd, psinfo_t *psinfo, char *psargs, int twidth)
{
    off_t argvoff = (off_t)psinfo->pr_argv;
    size_t len;
    char *psa = psargs;
    int bsize = twidth;
    int narg = NARG;
    off_t argv[NARG];
    off_t argoff;
    off_t nextargoff;
    int i;
#ifdef _LP64
    caddr32_t argv32[NARG];
    int is32 = (psinfo->pr_dmodel != PR_MODEL_LP64);
#endif

    if (psinfo->pr_nlwp == 0 ||
        strcmp(psinfo->pr_lwp.pr_clname, "SYS") == 0)
        goto out;

    (void) memset(psa, 0, bsize--);
    nextargoff = 0;
    errno = EIO;
    while (bsize > 0) {
        if (narg == NARG) {
            (void) memset(argv, 0, sizeof (argv));
#ifdef _LP64
            if (is32) {
                if ((i = pread(pfd, argv32, sizeof (argv32),
                    argvoff)) <= 0) {
                    if (i == 0 || errno == EIO)
                        break;
                    return (-1);
                }
                for (i = 0; i < NARG; i++)
                    argv[i] = argv32[i];
            } else
#endif
                if ((i = pread(pfd, argv, sizeof (argv),
                    argvoff)) <= 0) {
                    if (i == 0 || errno == EIO)
                        break;
                    return (-1);
                }
            narg = 0;
        }
        if ((argoff = argv[narg++]) == 0)
            break;
        if (argoff != nextargoff &&
            (i = pread(pfd, psa, bsize, argoff)) <= 0) {
            if (i == 0 || errno == EIO)
                break;
            return (-1);
        }
        len = strlen(psa);
        psa += len;
        *psa++ = ' ';
        bsize -= len + 1;
        nextargoff = argoff + len + 1;
#ifdef _LP64
        argvoff += is32? sizeof (caddr32_t) : sizeof (caddr_t);
#else
        argvoff += sizeof (caddr_t);
#endif
    }
    while (psa > psargs && isspace(*(psa-1)))
        psa--;

out:
    *psa = '\0';
    if (strlen(psinfo->pr_psargs) > strlen(psargs))
        (void) strcpy(psargs, psinfo->pr_psargs);

    return (0);
}

/*
 * Read environment variables from the process.
 * Append them to psargs if there is room.
 */
static int
preadenvs(int pfd, psinfo_t *psinfo, char *psargs, int twidth)
{
    off_t envpoff = (off_t)psinfo->pr_envp;
    int len;
    char *psa;
    char *psainit;
    int bsize;
    int nenv = NARG;
    off_t envp[NARG];
    off_t envoff;
    off_t nextenvoff;
    int i;
#ifdef _LP64
    caddr32_t envp32[NARG];
    int is32 = (psinfo->pr_dmodel != PR_MODEL_LP64);
#endif

    psainit = psa = (psargs != NULL)? psargs : psinfo->pr_psargs;
    len = strlen(psa);
    psa += len;
    bsize = twidth - len - 1;

    if (bsize <= 0 || psinfo->pr_nlwp == 0 ||
        strcmp(psinfo->pr_lwp.pr_clname, "SYS") == 0)
        return (0);

    nextenvoff = 0;
    errno = EIO;
    while (bsize > 0) {
        if (nenv == NARG) {
            (void) memset(envp, 0, sizeof (envp));
#ifdef _LP64
            if (is32) {
                if ((i = pread(pfd, envp32, sizeof (envp32),
                    envpoff)) <= 0) {
                    if (i == 0 || errno == EIO)
                        break;
                    return (-1);
                }
                for (i = 0; i < NARG; i++)
                    envp[i] = envp32[i];
            } else
#endif
                if ((i = pread(pfd, envp, sizeof (envp),
                    envpoff)) <= 0) {
                    if (i == 0 || errno == EIO)
                        break;
                    return (-1);
                }
            nenv = 0;
        }
        if ((envoff = envp[nenv++]) == 0)
            break;
        if (envoff != nextenvoff &&
            (i = pread(pfd, psa+1, bsize, envoff)) <= 0) {
            if (i == 0 || errno == EIO)
                break;
            return (-1);
        }
        *psa++ = ' ';
        len = strlen(psa);
        psa += len;
        bsize -= len + 1;
        nextenvoff = envoff + len + 1;
#ifdef _LP64
        envpoff += is32? sizeof (caddr32_t) : sizeof (caddr_t);
#else
        envpoff += sizeof (caddr_t);
#endif
    }
    while (psa > psainit && isspace(*(psa-1)))
        psa--;
    *psa = '\0';

    return (0);
}
