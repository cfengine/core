#
#  Copyright 2018 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation; version 3.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#
# To the extent this program is licensed as part of the Enterprise
# versions of CFEngine, the applicable Commercial Open Source License
# (COSL) may apply to this file if you as a licensee so wish it. See
# included file COSL.txt.
#
noinst_LTLIBRARIES = libutils.la

# TODO remove the openssl dependency! It's only there for base64 encoding.
AM_CFLAGS   = $(PCRE_CFLAGS) $(OPENSSL_CFLAGS)
AM_CPPFLAGS = $(PCRE_CPPFLAGS) $(OPENSSL_CPPFLAGS)
AM_LDFLAGS  = $(PCRE_LDFLAGS) $(OPENSSL_LDFLAGS)

libutils_la_LIBADD = ../libcompat/libcompat.la
libutils_la_LIBS     = $(PCRE_LIBS) $(OPENSSL_LIBS)

libutils_la_SOURCES = \
	alloc.c alloc.h \
	cleanup.c cleanup.h \
	compiler.h \
	deprecated.h \
	dir.h dir_priv.h \
	hashes.c hashes.h hash_method.h \
	sequence.c sequence.h \
	set.c set.h \
	statistics.c statistics.h \
	string_lib.c string_lib.h \
	pcre_include.h \
	platform.h \
	proc_keyvalue.c proc_keyvalue.h \
	bool.h \
	json.c json.h json-priv.h \
	json-utils.c json-utils.h \
	json-yaml.c json-yaml.h \
	refcount.c refcount.h \
	list.c list.h \
	logging.c logging.h logging_priv.h \
	buffer.c buffer.h \
	ip_address.c ip_address.h \
	map.c map.h map_common.h \
	mutex.c mutex.h \
	array_map.c array_map_priv.h \
	hash_map.c hash_map_priv.h \
	misc_lib.c misc_lib.h \
	writer.c writer.h \
	csv_writer.c csv_writer.h \
	csv_parser.c csv_parser.h \
	xml_writer.c xml_writer.h \
	file_lib.c file_lib.h \
	man.c man.h \
	rb-tree.c rb-tree.h \
	mustache.c mustache.h \
	cfversion.c cfversion.h \
	passopenfile.c passopenfile.h \
	unicode.c unicode.h \
	hash.c hash.h \
	queue.c queue.h \
	stack.c stack.h \
	ring_buffer.c ring_buffer.h \
	regex.c regex.h \
	encode.c encode.h \
	pcre_wrap.c pcre_wrap.h \
	libcrypto-compat.c libcrypto-compat.h \
	printsize.h

if !NT
  libutils_la_SOURCES += unix_dir.c
endif
if CYGWIN
  libutils_la_SOURCES += unix_dir.c
endif

CLEANFILES = *.gcno *.gcda

#
# Some basic clean ups
#
MOSTLYCLEANFILES = *~ *.orig *.rej

#
# Get everything removed down to where rebuilding requires:
# "aclocal; autoconf; autoheader; automake --add-missing"
# "configure; make; make install"
#
MAINTAINERCLEANFILES = config.h.in
