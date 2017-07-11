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

#include <load_avahi.h>
#include <files_interfaces.h>

#include <stdlib.h>

static const char *paths[] = {
    "/usr/lib/x86_64-linux-gnu/libavahi-client.so.3",
    "/usr/lib/libavahi-client.so.3",
    "/usr/lib64/libavahi-client.so.3",
    /* 32-bit variants */
    "/usr/lib/i386-linux-gnu/libavahi-client.so.3"
};

static const char *getavahipath();

int loadavahi()
{
    const char *path = getavahipath();

    avahi_handle = dlopen(path, RTLD_LAZY);

    if (!avahi_handle)
    {
        return -1;
    }

    avahi_simple_poll_quit_ptr = dlsym(avahi_handle, "avahi_simple_poll_quit");
    avahi_address_snprint_ptr = dlsym(avahi_handle, "avahi_address_snprint");
    avahi_service_resolver_free_ptr = dlsym(avahi_handle, "avahi_service_resolver_free");
    avahi_client_errno_ptr = dlsym(avahi_handle, "avahi_client_errno");
    avahi_strerror_ptr = dlsym(avahi_handle, "avahi_strerror");
    avahi_service_resolver_new_ptr = dlsym(avahi_handle, "avahi_service_resolver_new");
    avahi_service_browser_get_client_ptr = dlsym(avahi_handle, "avahi_service_browser_get_client");
    avahi_service_resolver_get_client_ptr = dlsym(avahi_handle, "avahi_service_resolver_get_client");
    avahi_simple_poll_new_ptr = dlsym(avahi_handle, "avahi_simple_poll_new");
    avahi_simple_poll_get_ptr = dlsym(avahi_handle, "avahi_simple_poll_get");
    avahi_client_new_ptr = dlsym(avahi_handle, "avahi_client_new");
    avahi_simple_poll_loop_ptr = dlsym(avahi_handle, "avahi_simple_poll_loop");
    avahi_service_browser_free_ptr = dlsym(avahi_handle, "avahi_service_browser_free");
    avahi_client_free_ptr = dlsym(avahi_handle, "avahi_client_free");
    avahi_simple_poll_free_ptr = dlsym(avahi_handle, "avahi_simple_poll_free");
    avahi_service_browser_new_ptr = dlsym(avahi_handle, "avahi_service_browser_new");

    return 0;
}

static const char *getavahipath()
{
    const char *env = getenv("AVAHI_PATH");
    struct stat sb;

    if (stat(env, &sb) == 0)
    {
        return env;
    }

    for (int i = 0; i < sizeof(paths)/sizeof(paths[0]); i++)
    {
        if (stat(paths[i], &sb) == 0)
        {
            return paths[i];
        }
    }

    return NULL;
}
