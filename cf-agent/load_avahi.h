/*
   Copyright 2017 Northern.tech AS

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

#ifndef CFENGINE_LOAD_AVAHI_H
#define CFENGINE_LOAD_AVAHI_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/address.h>

void (*avahi_simple_poll_quit_ptr)(AvahiSimplePoll *);
char* (*avahi_address_snprint_ptr)(char *, size_t , const AvahiAddress *);
int (*avahi_service_resolver_free_ptr)(AvahiServiceResolver *);
int (*avahi_client_errno_ptr)(AvahiClient *);
const char* (*avahi_strerror_ptr)(int);
AvahiServiceResolver* (*avahi_service_resolver_new_ptr)(AvahiClient *, AvahiIfIndex, AvahiProtocol, const char *, const char *, const char *, AvahiProtocol, AvahiLookupFlags, AvahiServiceResolverCallback, void *);
AvahiClient* (*avahi_service_browser_get_client_ptr)(AvahiServiceBrowser *);
AvahiClient* (*avahi_service_resolver_get_client_ptr)(AvahiServiceResolver *);
AvahiSimplePoll* (*avahi_simple_poll_new_ptr)();
const AvahiPoll* (*avahi_simple_poll_get_ptr)(AvahiSimplePoll *s);
AvahiClient* (*avahi_client_new_ptr)(const AvahiPoll *, AvahiClientFlags, AvahiClientCallback, void *, int *);
int (*avahi_simple_poll_loop_ptr)(AvahiSimplePoll *);
int (*avahi_service_browser_free_ptr)(AvahiServiceBrowser *);
void (*avahi_client_free_ptr)(AvahiClient *client);
void (*avahi_simple_poll_free_ptr)(AvahiSimplePoll *);
AvahiServiceBrowser* (*avahi_service_browser_new_ptr)(AvahiClient *, AvahiIfIndex, AvahiProtocol, const char *, const char *, AvahiLookupFlags, AvahiServiceBrowserCallback, void*);

void *avahi_handle;


int loadavahi();

#endif
