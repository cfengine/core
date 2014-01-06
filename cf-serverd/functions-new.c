/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
/*
 * XXX: need to check into copyright notice, since this is a significant
 * rewrite. -prj
 */

/* Standard Includes */
#include <sys/sockio.h>

#include <cf-serverd-functions.h>
#include <cf-serverd-enterprise-stubs.h>
#include <client_code.h>
#include <server_transform.h>
#include <bootstrap.h>
#include <scope.h>
#include <signals.h>
#include <mutex.h>
#include <locks.h>
#include <exec_tools.h>
#include <unix.h>
#include <man.h>
#include <tls_server.h>                              /* ServerTLSInitialize */
#include <timeout.h>
#include <unix_iface.h>
#include <known_dirs.h>
#include <sysinfo.h>
#include <time_classes.h>
#include <connection_info.h>
#include <file_lib.h>

#include <functions-new.h>		/* XXX: New location for defines for cleanliness. */

int FORK = true;		/* WARNING: Logic reversed from old way. */
/* Queue Size relocated to header. */

/* XXX: These should be changed to something saner. */
static const char *CF_SERVERD_SHORT_DESCRIPTION = "CFEngine file server daemon";

static const char *CF_SERVERD_MANPAGE_LONG_DESCRIPTION =
        "cf-serverd is a socket listening daemon providing two services: it acts as a file server for remote file copying "
        "and it allows an authorized cf-runagent to start a cf-agent run. cf-agent typically connects to a "
        "cf-serverd instance to request updated policy code, but may also request additional files for download. "
        "cf-serverd employs role based access control (defined in policy code) to authorize requests.";

static const struct option OPTIONS[] =
{
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {"file", required_argument, 0, 'f'},
    {"define", required_argument, 0, 'D'},
    {"negate", required_argument, 0, 'N'},
    {"no-lock", no_argument, 0, 'K'},
    {"inform", no_argument, 0, 'I'},
    {"diagnostic", no_argument, 0, 'x'},
    {"no-fork", no_argument, 0, 'F'},
    {"ld-library-path", required_argument, 0, 'L'},
    {"generate-avahi-conf", no_argument, 0, 'A'},
    {"legacy-output", no_argument, 0, 'l'},
    {"color", optional_argument, 0, 'C'},
    {NULL, 0, 0, '\0'}
};
static const char *HINTS[] =
{
    "Print the help message",
    "Enable debugging output",
    "Output verbose information about the behaviour of the agent",
    "Output the version of the software",
    "Specify an alternative input file than the default",
    "Define a list of comma separated classes to be defined at the start of execution",
    "Define a list of comma separated classes to be undefined at the start of execution",
    "Ignore locking constraints during execution (ifelapsed/expireafter) if \"too soon\" to run",
    "Print basic information about changes made to the system, i.e. promises repaired",
    "Activate internal diagnostics (developers only)",
    "Run as a foreground processes (do not fork)",
    "Set the internal value of LD_LIBRARY_PATH for child processes",
    "Generates avahi configuration file to enable policy server to be discovered in the network",
    "Use legacy output format",
    "Enable colorized output. Possible values: 'always', 'auto', 'never'. If option is used, the default value is 'auto'",
    NULL
};
/* XXX: ^^^ So very bad, so very antiquated. */

static void KeepHardClass(EvalContext *ctx)
{
	char name[CF_BUFSIZE];
	if (name != NULL)
	{
		char *existing_policy_server = ReadPolicyServerFile(CFWORKDIR);
		if (existing_policy_server)
		{
			if (GetAmPolicyHub(CFWORKDIR))
			{
				EvalContextClassPutHard(ctx, "am_policy_hub", "source=bootstrap");
			}
			free(existing_policy_server);
		}
	}
	/* FIXME: should be in generic_agent. */
	GenericAgentAddEditionClasses(ctx);
}

/*
 * XXX: PLACEHOLDER FOR GenericAgentConfig()
 */

/*
 * XXX: void ThisAgentInit(void)
 * Does nothing more than set umask. Why?? This should just be tossed.
 */

int GetAvailableListeners(void)
{
	/* Walk through all our interfaces to see listener candidates, using strict enforcement.
	 * Then, return an indexed list of listeners. (Prepping for #2922)
	 */
	char buffer[CF_BUFSIZE+1];

	int listenfd, new_fd;
	struct addrinfo hints, *servinfo, *p;
	socklen_t sin_size;
	struct sigaction sa;
	int pass=1;			/* Presume we can listen excepting all other issues. */
	int dnspass=0;		/* Presume DNS fail to force test. */
	char s[INET6_ADDRSTRLEN];
	int rv;

/* DNR BEGIN: NONSTRICT IS DEBUG FLAG. */
#ifndef NONSTRICT
/* DNR END: NONSTRICT IS DEBUG FLAG. */

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		/* Must walk AF_UNSPEC into AF_INET && AF_INET6 */
	hints.ai_socktype = SOCK_STREAM;	/* Same Same */
	hints.ai_flags = AI_PASSIVE;		/* Use any available address. */

	if ((rv = getaddrinfo(NULL, CFENGINE_PORT, &hints, &servinfo)) != 0) {
		Log(LOG_LEVEL_CRIT, "Unable to get any port or address: %s", gai_strerror(rv));
		break; 	/* Bail out immediate. */
	}

	/*
	 * Loop through all results, but don't bind yet.
	 * XXX: Don't even TEST binding; the free almost never returns fast enough.
	 * That results in a false-positive error condition where port is in use as it's
	 * still being freed. Ugh.
	 */
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			Log(LOG_LEVEL_NOTICE, "Unable to use %s %s", p->ai_addr, CFENGINE_PORT);
		}
		if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ) {
			Log(LOG_LEVEL_WARN, "Failed to set socket options %s %s", p->ai_addr, CFENGINE_PORT);
			/* Still not totally hosed. */
			continue;
		}
		if (bind(listenfd), p->ai_addr, p->ai_addrlen) {
			close(listenfd);
			Log(LOG_LEVEL_NOTICE, "Failed to bind %s %s", p->ai_addr, CFENGINE_PORT);
			continue;
		}

		break;
	}

	if (p == NULL) {
		Log(LOG_LEVEL_CRIT, "Failed to bind to address %s", p->ai_addr);
		return -1;
	}

	freeaddrinfo(servinfo); 	/* Clean up, and now we sleep just a bit. */
	sleep(1);					/* Prevent socket collision during free. Sorry. */

	if (listen(listenfd, CF_QUEUE_SIZE) == -1) {
		Log(LOG_LEVEL_WARNING, "Error: %s", perror(listen));
		return -1;
	}

	sa.sa_handler = sigchild_handler;	/* Reap what we sow. */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHILD, &sa, NULL) == -1) {
		Log(LOG_LEVEL_WARNING, "Error %s attempting to bind to %s %s", perror(sigaction), p->ai_addr, CFENGINE_PORT);
		exit(1);
	}

	/* XXX: Test for debug? */
	Log(LOG_LEVEL_NOTICE, "Policy Hub waiting connections at %s %s", p->ai_addr, CFENGINE_PORT);

	while(1) {
		sin_size = sizeof their_addr;
		new_fd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size)
			if (new_fd == -1) {
				perror("accept");
				continue;
			}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&theiraddr), s, sizeof s);
		Log(LOG_LEVEL_NOTICE, "Connection from %s", s);

		if(!fork()) {
			close(listenfd);	/* Wasted listener. */
			if (send(new_fd, "COMM_CHECK_360", 13, 0) == -1)
				perror("send");
			close(new_fd);
			exit(0);
		}
		close (new_fd);		/* Parent doesn't need it. */
	}
	return 0;
}

/*

//	while (BINDINTERFACE > 0) {
//		ptr = BINDINTERFACE;
//		/* Throw error if no DNS. */
//		if ((hptr = gethostbyname(BINDINTERFACE)) == NULL) {
//			Log(LOG_LEVEL_NOTICE, "Address %s does not have associated DNS record.", GetErrorStr());
//		}
//		if (hptr(ai_family) = AF_INET && !AF_LOOPBACK) {
//			/* Check DNS Strict. */
//			host_candidate = BINDINTERFACE;
//			switch (BINDINTERFACE->h_addrtype) {
//			case AF_INET:
//				hptr = hptr->h_addr_list;
//				for ( ; *pptr != NULL; pptr++)
//					gethostbyname(pptr, CFSERVER_PORT, )
//
//				}
//					gethostbyname(pptr)
//			}
//		}
//
//	};
//	ret = gethostbyname(BINDINTERFACE);
//}
// */
