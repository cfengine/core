# Overview of LMDB databases in /var/cfengine/state

Documentation to help understand the data in our LMDB databases.
This is not meant as user-level documentation.
It has technical details and shows C source code.

## What is LMDB?

Lightning Memory-mapped Database (LMDB) is a high performance local key-value store database.
We use LMDB to store data which is shared between different CFEngine binaries and agent runs.
Examples include incoming/outgoing network connections (cf_lastseen), promise locks (cf_lock), performance information and state data about the host.

In our LMDB databases, we use NUL-terminated C strings as keys, and values may either be plain text (C strings) or binary data (C structs).
LMDB database files `*.lmdb` are platform specific, the representation and sizes of various types in C are different on different systems and architectures.

## Getting a list of all databases

### Community policy server

On a fresh community install, these LMDB databases are created:

```
$ ls -al /var/cfengine/state/*.lmdb
-rw-r--r-- 1 root root  12288 Aug 23 08:37  /var/cfengine/state/cf_lastseen.lmdb
-rw------- 1 root root  32768 Aug 23 08:49  /var/cfengine/state/cf_lock.lmdb
-rw------- 1 root root  49152 Aug 23 08:49  /var/cfengine/state/cf_observations.lmdb
-rw-r--r-- 1 root root   8192 Aug 23 08:37  /var/cfengine/state/cf_state.lmdb
-rw------- 1 root root  16384 Aug 23 08:37  /var/cfengine/state/history.lmdb
-rw------- 1 root root   8192 Aug 23 08:37  /var/cfengine/state/nova_measures.lmdb
-rw------- 1 root root   8192 Aug 23 08:37  /var/cfengine/state/nova_static.lmdb
-rw------- 1 root root 581632 Aug 23 08:47  /var/cfengine/state/packages_installed_apt_get.lmdb
-rw-r--r-- 1 root root   8192 Aug 23 08:37 '/var/cfengine/state/packages_installed_$(package_module_knowledge.platform_default).lmdb'
-rw------- 1 root root  28672 Aug 23 08:47  /var/cfengine/state/packages_updates_apt_get.lmdb
-rw-r--r-- 1 root root  32768 Aug 23 08:47  /var/cfengine/state/performance.lmdb
```

### Enterprise hub package

Similarly, installing an enterprise hub package:

```
$ ls -al /var/cfengine/state/*.lmdb
-rw------- 1 root root  32768 Aug 23 09:00  /var/cfengine/state/cf_changes.lmdb
-rw------- 1 root root  28672 Aug 23 09:00  /var/cfengine/state/cf_lastseen.lmdb
-rw------- 1 root root  61440 Aug 23 09:01  /var/cfengine/state/cf_lock.lmdb
-rw------- 1 root root  28672 Aug 23 09:00  /var/cfengine/state/cf_observations.lmdb
-rw------- 1 root root  20480 Aug 23 09:00  /var/cfengine/state/cf_state.lmdb
-rw------- 1 root root  16384 Aug 23 09:00  /var/cfengine/state/history.lmdb
-rw------- 1 root root  32768 Aug 23 09:01  /var/cfengine/state/nova_agent_execution.lmdb
-rw------- 1 root root   8192 Aug 23 09:00  /var/cfengine/state/nova_measures.lmdb
-rw------- 1 root root  20480 Aug 23 09:00  /var/cfengine/state/nova_static.lmdb
-rw------- 1 root root  12288 Aug 23 09:00  /var/cfengine/state/nova_track.lmdb
-rw------- 1 root root 425984 Aug 23 09:01  /var/cfengine/state/packages_installed_apt_get.lmdb
-rw------- 1 root root   8192 Aug 23 09:00 '/var/cfengine/state/packages_installed_$(package_module_knowledge.platform_default).lmdb'
-rw------- 1 root root  32768 Aug 23 09:01  /var/cfengine/state/packages_updates_apt_get.lmdb
-rw------- 1 root root  32768 Aug 23 09:01  /var/cfengine/state/performance.lmdb
```

### Examining the source code

From `dbm_api.c`, these look like all the database files which are possible:

```C
static const char *const DB_PATHS_STATEDIR[] = {
    [dbid_classes] = "cf_classes",
    [dbid_variables] = "cf_variables",
    [dbid_performance] = "performance",
    [dbid_checksums] = "checksum_digests",
    [dbid_filestats] = "stats",
    [dbid_changes] = "cf_changes",
    [dbid_observations] = "cf_observations",
    [dbid_state] = "cf_state",
    [dbid_lastseen] = "cf_lastseen",
    [dbid_audit] = "cf_audit",
    [dbid_locks] = "cf_lock",
    [dbid_history] = "history",
    [dbid_measure] = "nova_measures",
    [dbid_static] = "nova_static",
    [dbid_scalars] = "nova_pscalar",
    [dbid_windows_registry] = "mswin",
    [dbid_cache] = "nova_cache",
    [dbid_license] = "nova_track",
    [dbid_value] = "nova_value",
    [dbid_agent_execution] = "nova_agent_execution",
    [dbid_bundles] = "bundles",
    [dbid_packages_installed] = "packages_installed",
    [dbid_packages_updates] = "packages_updates"
};
```

Some of them are deprecated:

```C
typedef enum
{
    dbid_classes,   // Deprecated
    dbid_variables, // Deprecated
    dbid_performance,
    dbid_checksums, // Deprecated
    dbid_filestats, // Deprecated
    dbid_changes,
    dbid_observations,
    dbid_state,
    dbid_lastseen,
    dbid_audit,
    dbid_locks,
    dbid_history,
    dbid_measure,
    dbid_static,
    dbid_scalars,
    dbid_windows_registry,
    dbid_cache,
    dbid_license,
    dbid_value,
    dbid_agent_execution,
    dbid_bundles,   // Deprecated
    dbid_packages_installed, //new package promise installed packages list
    dbid_packages_updates,   //new package promise list of available updates

    dbid_max
} dbid;
```

## Individual database files

### cf_lastseen.lmdb

See `lastseen.c` for more details.

#### Example cf-check dump

```
$ cf-check dump -a /var/cfengine/state/cf_lastseen.lmdb
key: 0x7fba331acf34[16] a192.168.100.10, data: 0x7fba331acf44[37] MD5=6fcc943142f461f4c0aa59abe871bc57
key: 0x7fba331acf72[38] kMD5=6fcc943142f461f4c0aa59abe871bc57, data: 0x7fba331acf98[15] 192.168.100.10
key: 0x7fba331acfb0[39] qoMD5=6fcc943142f461f4c0aa59abe871bc57, data: 0x7fba331acfd7[40] Fc]
```

#### Example mdb_dump

```
$ mdb_dump -n -p /var/cfengine/state/cf_lastseen.lmdb
VERSION=3
format=print
type=btree
mapsize=104857600
maxreaders=126
db_pagesize=4096
HEADER=END
 a192.168.100.10\00
 MD5=6fcc943142f461f4c0aa59abe871bc57\00
 kMD5=6fcc943142f461f4c0aa59abe871bc57\00
 192.168.100.10\00
 qoMD5=6fcc943142f461f4c0aa59abe871bc57\00
 F\bfc]\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00
DATA=END
```

#### Version entry

Denotes the database schema used by this database file.

```
key: "version"
value: "1"
```

#### Address entries (reverse lookup)

```
key: a<address> (IPv6 or IPv6)
value: <hostkey>
```

#### Quality entries

```
key: q<direction><hostkey> (direction: 'i' for incoming, 'o' for outgoing)
value: struct KeyHostSeen
```

##### KeyHostSeen struct

From `lastseen.h`:

```C
typedef struct
{
    time_t lastseen;
    QPoint Q;
} KeyHostSeen;
```

The QPoint is a rolling weighted average of the time between connections:

```
    newq.Q = QAverage(q.Q, newq.lastseen - q.lastseen, 0.4);
```

#### Hostkey entries
```
key: k<hostkey> ("MD5-ffffefefeefef..." or "SHA-abacabaca...")
value: <address> (IPv4 or IPv6)
```

### cf_lock.lmdb

See `locks.c` for more details.

#### Example cf-check dump

```
root@dev ~ $ cf-check dump -a /var/cfengine/state/cf_lock.lmdb
key: 0x7f6b0f4e4b36[33] 05315dcd049a9e89c6d85520d505f600, data: 0x7f6b0f4e4b57[24] =
key: 0x7f6b0f4e4c3e[33] 0c8f50c64db3673c7f2d8eca5d8a475b, data: 0x7f6b0f4e4c5f[24] =
key: 0x7f6b0f4e48a2[33] 0f121ab519b8b23bae54714d3f1a83e1, data: 0x7f6b0f4e48c3[24] =
key: 0x7f6b0f4e4d74[33] 37e5b5e86d2e7474b4106be6f395401c, data: 0x7f6b0f4e4d95[24] =
key: 0x7f6b0f4e49aa[33] 398e0e0964c2608b419b50cf5d038fd6, data: 0x7f6b0f4e49cb[24] =
[...]
key: 0x7f6b0f4e4d46[13] lock_horizon, data: 0x7f6b0f4e4d53[24]
root@dev ~ $
```

#### Example mdb_dump

```
root@dev ~ $ mdb_dump -n -p /var/cfengine/state/cf_lock.lmdb
VERSION=3
format=print
type=btree
mapsize=104857600
maxreaders=210
db_pagesize=4096
HEADER=END
 05315dcd049a9e89c6d85520d505f600\00
 \f1=\00\00\00\00\00\00,\d4c]\00\00\00\00r\16\00\00\00\00\00\00
 0c8f50c64db3673c7f2d8eca5d8a475b\00
 \f1=\00\00\00\00\00\00,\d4c]\00\00\00\00r\16\00\00\00\00\00\00
 0f121ab519b8b23bae54714d3f1a83e1\00
 \f1=\00\00\00\00\00\00,\d4c]\00\00\00\00r\16\00\00\00\00\00\00
 37e5b5e86d2e7474b4106be6f395401c\00
 \f1=\00\00\00\00\00\00,\d4c]\00\00\00\00r\16\00\00\00\00\00\00
 398e0e0964c2608b419b50cf5d038fd6\00
 \f1=\00\00\00\00\00\00,\d4c]\00\00\00\00r\16\00\00\00\00\00\00
[...]
 lock_horizon\00
 \00\00\00\00\00\00\00\00F\bfc]\00\00\00\00\00\00\00\00\00\00\00\00
DATA=END
root@dev ~ $
```

#### Lock keys

For each promise, 2 strings are generated like this:

```C
char cflock[CF_BUFSIZE] = "";
snprintf(cflock, CF_BUFSIZE, "lock.%.100s.%s.%.100s_%d_%s",
         bundle_name, cc_operator, cc_operand, sum, str_digest);

char cflast[CF_BUFSIZE] = "";
snprintf(cflast, CF_BUFSIZE, "last.%.100s.%s.%.100s_%d_%s",
         bundle_name, cc_operator, cc_operand, sum, str_digest);

Log(LOG_LEVEL_DEBUG, "Locking bundle '%s' with lock '%s'",
    bundle_name, cflock);
```

The result can be seen in debug log:

```
root@dev ~ $ cf-agent --log-level debug | grep -F Locking
   debug: Locking bundle 'inventory_control' with lock 'lock.inventory_control.reports.-dev.inventory_control__LSB_module_enabled_5317_MD5=17cc5c06fc415f344762d927f53723b7'
   debug: Locking bundle 'inventory_control' with lock 'lock.inventory_control.reports.-dev.inventory_control__dmidecode_module_enabled_4618_MD5=ecdad15e8a457659d321be7aaf3465e0'
   debug: Locking bundle 'inventory_control' with lock 'lock.inventory_control.reports.-dev.inventory_control__mtab_module_enabled_5516_MD5=926e972f2d6bee317a0c1b09c0a05029'
   debug: Locking bundle 'inventory_control' with lock 'lock.inventory_control.reports.-dev.inventory_control__fstab_module_enabled_4364_MD5=f743f4810ceac5dc7f29c246b27ef4f4'
   debug: Locking bundle 'inventory_control' with lock 'lock.inventory_control.reports.-dev.inventory_control__proc_module_enabled_5328_MD5=b55e6f18cba2d498fc49cf9643edc9fe'
   debug: Locking bundle 'inventory_control' with lock 'lock.inventory_control.reports.-dev.inventory_control__package_refresh_module_enabled_5998_MD5=8ed765eab0a379b899bb20ffff361149'
   debug: Locking bundle 'inventory_autorun' with lock 'lock.inventory_autorun.methods.usebundle.handle.-dev.method_packages_refresh__7849_MD5=bf16d30d8791bfd0e9b9bdde5a2e3f1d'
[...]
root@dev ~ $
```

These strings are then hashed using MD5, and the MD5 digest is used as a key in LMDB.

#### Lock values

This is the C struct stored as a value in LMDB:

```C
typedef struct
{
    pid_t pid;
    time_t time;
    time_t process_start_time;
} LockData;
```

Seems to be created like this:

```C
static bool WriteLockDataCurrent(CF_DB *dbp, const char *lock_id)
{
    LockData lock_data = { 0 };
    lock_data.pid = getpid();
    lock_data.time = time(NULL);
    lock_data.process_start_time = GetProcessStartTime(getpid());

    return WriteLockData(dbp, lock_id, &lock_data);
}
```

Quite simple:

* A PID and a start time of the process which locked this promise.
* The time the promise was locked.
