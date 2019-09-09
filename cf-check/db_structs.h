#ifndef CFENGINE_DB_STRUCTS_H
#define CFENGINE_DB_STRUCTS_H

// All structs needed for cf-check pretty printing
// Structs from libutils are included normally via headers
// Structs from libpromises are duplicated; cf-check doesn't use libpromises

#include <statistics.h>
// typedef struct
// {
//     double q;
//     double expect;
//     double var;
//     double dq;
// } QPoint;

// Struct used for quality entries in /var/cfengine/state/cf_lastseen.lmdb:
typedef struct
{
    time_t lastseen;
    QPoint Q;
} KeyHostSeen; // Keep in sync with lastseen.h

// Struct used for lock entries in /var/cfengine/state/cf_lock.lmdb:
typedef struct
{
    pid_t pid;
    time_t time;
    time_t process_start_time;
} LockData; // Keep in sync with cf3.defs.h

#define OBSERVABLES_APPLY(apply_macro) \
    apply_macro(users)                 \
    apply_macro(rootprocs)             \
    apply_macro(otherprocs)            \
    apply_macro(diskfree)              \
    apply_macro(loadavg)               \
    apply_macro(netbiosns_in)          \
    apply_macro(netbiosns_out)         \
    apply_macro(netbiosdgm_in)         \
    apply_macro(netbiosdgm_out)        \
    apply_macro(netbiosssn_in)         \
    apply_macro(netbiosssn_out)        \
    apply_macro(imap_in)               \
    apply_macro(imap_out)              \
    apply_macro(cfengine_in)           \
    apply_macro(cfengine_out)          \
    apply_macro(nfsd_in)               \
    apply_macro(nfsd_out)              \
    apply_macro(smtp_in)               \
    apply_macro(smtp_out)              \
    apply_macro(www_in)                \
    apply_macro(www_out)               \
    apply_macro(ftp_in)                \
    apply_macro(ftp_out)               \
    apply_macro(ssh_in)                \
    apply_macro(ssh_out)               \
    apply_macro(wwws_in)               \
    apply_macro(wwws_out)              \
    apply_macro(icmp_in)               \
    apply_macro(icmp_out)              \
    apply_macro(udp_in)                \
    apply_macro(udp_out)               \
    apply_macro(dns_in)                \
    apply_macro(dns_out)               \
    apply_macro(tcpsyn_in)             \
    apply_macro(tcpsyn_out)            \
    apply_macro(tcpack_in)             \
    apply_macro(tcpack_out)            \
    apply_macro(tcpfin_in)             \
    apply_macro(tcpfin_out)            \
    apply_macro(tcpmisc_in)            \
    apply_macro(tcpmisc_out)           \
    apply_macro(webaccess)             \
    apply_macro(weberrors)             \
    apply_macro(syslog)                \
    apply_macro(messages)              \
    apply_macro(temp0)                 \
    apply_macro(temp1)                 \
    apply_macro(temp2)                 \
    apply_macro(temp3)                 \
    apply_macro(cpuall)                \
    apply_macro(cpu0)                  \
    apply_macro(cpu1)                  \
    apply_macro(cpu2)                  \
    apply_macro(cpu3)                  \
    apply_macro(microsoft_ds_in)       \
    apply_macro(microsoft_ds_out)      \
    apply_macro(www_alt_in)            \
    apply_macro(www_alt_out)           \
    apply_macro(imaps_in)              \
    apply_macro(imaps_out)             \
    apply_macro(ldap_in)               \
    apply_macro(ldap_out)              \
    apply_macro(ldaps_in)              \
    apply_macro(ldaps_out)             \
    apply_macro(mongo_in)              \
    apply_macro(mongo_out)             \
    apply_macro(mysql_in)              \
    apply_macro(mysql_out)             \
    apply_macro(postgresql_in)         \
    apply_macro(postgresql_out)        \
    apply_macro(ipp_in)                \
    apply_macro(ipp_out)               \
    apply_macro(spare)

// Macros to apply to each element:

// Double # useful for creating an identifier, expands to ob_postgresql_in,
#define GENERATE_OB_ENUM(OB_NAME) ob_##OB_NAME,
// Single # useful for creating string literals, expands to: "postgresql_in",
#define GENERATE_OB_STRING(OB_NAME) #OB_NAME,

// Use apply macro to generate enum and string array

typedef enum Observable
{
    OBSERVABLES_APPLY(GENERATE_OB_ENUM) observables_max
} Observable;

static const char *const observable_strings[] =
{
    OBSERVABLES_APPLY(GENERATE_OB_STRING) NULL
};

// Not the actual count, just the room we set aside in struct (and LMDB):
#define CF_OBSERVABLES 100

typedef struct Averages
{
    time_t last_seen;
    QPoint Q[CF_OBSERVABLES];
} Averages; // Keep in sync with cf3.defs.h

typedef enum
{
    CONTEXT_STATE_POLICY_RESET,                    /* Policy when trying to add already defined persistent states */
    CONTEXT_STATE_POLICY_PRESERVE
} PersistentClassPolicy; // Keep in sync with cf3.defs.h

typedef struct
{
    unsigned int expires;
    PersistentClassPolicy policy;
    char tags[]; // variable length, must be zero terminated
} PersistentClassInfo; // Keep in sync with cf3.defs.h
// Note that tags array does not increase the sizeof() this struct
// It allows us to index bytes after the policy variable (plus padding)
// As far as C is concerned, tags can be zero length,
// we do however require that it is at least 1 (NUL) byte

#endif
