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

const char *DAY_TEXT[] =
{
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
    "Sunday",
    NULL
};

const char *MONTH_TEXT[] =
{
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
    NULL
};

const char *SHIFT_TEXT[] =
{
    "Night",
    "Morning",
    "Afternoon",
    "Evening",
    NULL
};

const char *CF_DATATYPES[] =    /* see enum cfdatatype */
{
    "string",
    "int",
    "real",
    "slist",
    "ilist",
    "rlist",
    "(menu option)",
    "(option list)",
    "(ext body)",
    "(ext bundle)",
    "class",
    "clist",
    "irange [int,int]",
    "rrange [real,real]",
    "counter",
    "<notype>",
};

const char *CF_AGENTTYPES[] =   /* see enum cfagenttype */
{
    CF_COMMONC,
    CF_AGENTC,
    CF_SERVERC,
    CF_MONITORC,
    CF_EXECC,
    CF_RUNC,
    CF_KNOWC,
    CF_REPORTC,
    CF_KEYGEN,
    CF_HUBC,
    CF_GENDOC,
    "<notype>",
};

/* These string lengths should not exceed CF_MAXDIGESTNAMELEN
   characters for packing */

const char *CF_DIGEST_TYPES[10][2] =
{
    {"md5", "m"},
    {"sha224", "c"},
    {"sha256", "C"},
    {"sha384", "h"},
    {"sha512", "H"},
    {"sha1", "S"},
    {"sha", "s"},               /* Should come last, since substring */
    {"best", "b"},
    {"crypt", "o"},
    {NULL, NULL}
};

const int CF_DIGEST_SIZES[10] =
{
    CF_MD5_LEN,
    CF_SHA224_LEN,
    CF_SHA256_LEN,
    CF_SHA384_LEN,
    CF_SHA512_LEN,
    CF_SHA1_LEN,
    CF_SHA_LEN,
    CF_BEST_LEN,
    CF_CRYPT_LEN,
    0
};

/*******************************************************************/
/* Anomaly                                                         */
/*******************************************************************/

const Sock ECGSOCKS[ATTR] =     /* extended to map old to new using enum */
{
    {"137", "netbiosns", ob_netbiosns_in, ob_netbiosns_out},
    {"138", "netbiosdgm", ob_netbiosdgm_in, ob_netbiosdgm_out},
    {"139", "netbiosssn", ob_netbiosssn_in, ob_netbiosssn_out},
    {"445", "microsoft_ds", ob_microsoft_ds_in, ob_microsoft_ds_out},
    {"5308", "cfengine", ob_cfengine_in, ob_cfengine_out},
    {"2049", "nfsd", ob_nfsd_in, ob_nfsd_out},
    {"25", "smtp", ob_smtp_in, ob_smtp_out},
    {"80", "www", ob_www_in, ob_www_out},
    {"8080", "www-alt", ob_www_alt_in, ob_www_alt_out},
    {"21", "ftp", ob_ftp_in, ob_ftp_out},
    {"22", "ssh", ob_ssh_in, ob_ssh_out},
    {"443", "wwws", ob_wwws_in, ob_wwws_out},
    {"143", "imap", ob_imap_in, ob_imap_out},
    {"993", "imaps", ob_imaps_in, ob_imaps_out},
    {"389", "ldap", ob_ldap_in, ob_ldap_out},
    {"636", "ldaps", ob_ldaps_in, ob_ldaps_out},
    {"27017", "mongo", ob_mongo_in, ob_mongo_out},
    {"3306", "mysql", ob_mysql_in, ob_mysql_out},
    {"5432", "postgresql", ob_postgresql_in, ob_postgresql_out},
    {"631", "ipp", ob_ipp_in, ob_ipp_out},
};

const char *TCPNAMES[CF_NETATTR] =
{
    "icmp",
    "udp",
    "dns",
    "tcpsyn",
    "tcpack",
    "tcpfin",
    "misc"
};

const char *OBS[CF_OBSERVABLES][2] =
{
    {"users", "Users with active processes - including system users"},
    {"rootprocs", "Sum privileged system processes"},
    {"otherprocs", "Sum non-privileged process"},
    {"diskfree", "Free disk on / partition"},
    {"loadavg", "Kernel load average utilization (sum over cores)"},
    {"netbiosns_in", "netbios name lookups (in)"},
    {"netbiosns_out", "netbios name lookups (out)"},
    {"netbiosdgm_in", "netbios name datagrams (in)"},
    {"netbiosdgm_out", "netbios name datagrams (out)"},
    {"netbiosssn_in", "Samba/netbios name sessions (in)"},
    {"netbiosssn_out", "Samba/netbios name sessions (out)"},
    {"imap_in", "imap mail client sessions (in)"},
    {"imap_out", "imap mail client sessions (out)"},
    {"cfengine_in", "cfengine connections (in)"},
    {"cfengine_out", "cfengine connections (out)"},
    {"nfsd_in", "nfs connections (in)"},
    {"nfsd_out", "nfs connections (out)"},
    {"smtp_in", "smtp connections (in)"},
    {"smtp_out", "smtp connections (out)"},
    {"www_in", "www connections (in)"},
    {"www_out", "www connections (out)"},
    {"ftp_in", "ftp connections (in)"},
    {"ftp_out", "ftp connections (out)"},
    {"ssh_in", "ssh connections (in)"},
    {"ssh_out", "ssh connections (out)"},
    {"wwws_in", "wwws connections (in)"},
    {"wwws_out", "wwws connections (out)"},
    {"icmp_in", "ICMP packets (in)"},
    {"icmp_out", "ICMP packets (out)"},
    {"udp_in", "UDP dgrams (in)"},
    {"udp_out", "UDP dgrams (out)"},
    {"dns_in", "DNS requests (in)"},
    {"dns_out", "DNS requests (out)"},
    {"tcpsyn_in", "TCP sessions (in)"},
    {"tcpsyn_out", "TCP sessions (out)"},
    {"tcpack_in", "TCP acks (in)"},
    {"tcpack_out", "TCP acks (out)"},
    {"tcpfin_in", "TCP finish (in)"},
    {"tcpfin_out", "TCP finish (out)"},
    {"tcpmisc_in", "TCP misc (in)"},
    {"tcpmisc_out", "TCP misc (out)"},
    {"webaccess", "Webserver hits"},
    {"weberrors", "Webserver errors"},
    {"syslog", "New log entries (Syslog)"},
    {"messages", "New log entries (messages)"},
    {"temp0", "CPU Temperature 0"},
    {"temp1", "CPU Temperature 1"},
    {"temp2", "CPU Temperature 2"},
    {"temp3", "CPU Temperature 3"},
    {"cpu", "%CPU utilization (all)"},
    {"cpu0", "%CPU utilization 0"},
    {"cpu1", "%CPU utilization 1"},
    {"cpu2", "%CPU utilization 2"},
    {"cpu3", "%CPU utilization 3"},
    {"microsoft_ds_in", "Samba/MS_ds name sessions (in)"},
    {"microsoft_ds_out", "Samba/MS_ds name sessions (out)"},
    {"www_alt_in", "Alternative web service connections (in)"},
    {"www_alt_out", "Alternative web client connections (out)"},
    {"imaps_in", "encrypted imap mail service sessions (in)"},
    {"imaps_out", "encrypted imap mail client sessions (out)"},
    {"ldap_in", "LDAP directory service service sessions (in)"},
    {"ldap_out", "LDAP directory service client sessions (out)"},
    {"ldaps_in", "LDAP directory service service sessions (in)"},
    {"ldaps_out", "LDAP directory service client sessions (out)"},
    {"mongo_in", "Mongo database service sessions (in)"},
    {"mongo_out", "Mongo database client sessions (out)"},
    {"mysql_in", "MySQL database service sessions (in)"},
    {"mysql_out", "MySQL database client sessions (out)"},
    {"postgres_in", "PostgreSQL database service sessions (in)"},
    {"postgres_out", "PostgreSQL database client sessions (out)"},
    {"ipp_in", "Internet Printer Protocol (in)"},
    {"ipp_out", "Internet Printer Protocol (out)"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
    {"spare", "unused"},
};

/*****************************************************************/

char *AGENT_TYPESEQUENCE[] =
{
    "meta",
    "vars",
    "defaults",
    "classes",                  /* Maelstrom order 2 */
    "outputs",
    "interfaces",
    "files",
    "packages",
    "guest_environments",
    "methods",
    "processes",
    "services",
    "commands",
    "storage",
    "databases",
    "reports",
    NULL
};

