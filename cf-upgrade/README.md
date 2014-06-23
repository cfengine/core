# Introduction
cf-upgrade is a small tool to upgrade CFEngine. It works by copying itself
to a temporary location and then running the specified commands.

Before running the upgrade command it creates a backup of the current CFEngine
folder. If at any time during the upgrade process fatal errors are detected,
it then restores the CFEngine folder to its previous contents.

The detection of problems is done by the return code of the commands it
executes, therefore if a package manager only returns 0, cf-upgrade has no way
to identify problems.

# Usage
cf-upgrade needs the following command-line arguments:
  * -b: backup script
  * -s: backup archive (full path including extension)
  * -f: CFEngine folder, by default /var/cfengine
  * -i: Upgrade command and its arguments, for instance "dpkg --install package.deb"

Besides the main arguments, cf-upgrade accepts the following:
  * -h: print the help screen
  * -v: print the version
  * -c: Change the location of the copy of cf-upgrade. Default /tmp/cf-upgrade

Hidden arguments for internal usage:
  * -x: After cf-upgrade copies and re-executes itself, the original -i
        argument will be replaced with -x, to flag that upgrade can start.

# Some notes about the backup script
The backup script could be written in any language, in fact it might even be a
binary. This script needs to be provided by the packager and is not distributed
with CFEngine Core since it might be system-specific.

The script must accept the following parameters:
  * BACKUP|RESTORE: Whether to backup or to restore the CFEngine folder.
  * backup archive path: Full path to the location of the backup archive.

In addition this script needs to accept a final argument, the CFEngine folder.
If not given it should assume "/var/cfengine".

# Tests
cf-upgrade has some unit test coverage, however it is limited to its internal
structure and logic. There is a much more comprehensive test suite in the tests
folder. This test suite is manual and needs to be run after cf-upgrade has been
built.
