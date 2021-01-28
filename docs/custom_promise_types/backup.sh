#!/bin/sh
#
# Backup custom promise type, uses cfengine.sh library located in same dir,
# and https://github.com/Lex-2008/backup3
#
# Installation:
# 1. Clone the above repo next to this file
# 2. pick a directory to keep backups, preferrably on separate disk/partition,
#    since backups are configured to fill up to 90% of disk space by default.
#    We'll use /var/backups in this example.
# 3. Run init.sh to initialise backup dir, like this:
#    BACKUP_ROOT=/var/backups backup3/init.sh
# 4. Use it in the policy like this:
#      promise agent backup
#      {
#          interpreter => "/bin/bash";
#          path => "$(sys.inputdir)/backup.sh";
#      }
#      bundle agent main
#      {
#        backup:
#          "homes"
#            from => "/home/",
#            root => "/var/backups";
#      }

obligatory_attributes="from"
optional_attributes="root"
all_attributes_are_valid="no"

do_evaluate() {
    # Prepare environment for wrapped script
    BACKUP_ROOT="$request_attribute_root"
    BACKUP_KEEP_TMP="1"
    run_this() {
        run_rsync always "$request_promiser" "$request_attribute_from"
    }

    # call the script
    . "$(dirname "$0")/backup3/backup.sh"

    # Analyse its results
    if [ "$?" = 0 -o ! -f "$BACKUP_TMP".files ]; then
        response_result="not_kept"
        return 0
    fi
    # Note: $BACKUP_TMP".files file contains lists of created and deleted files,
    # one file per line, separated by line saying "separator", and first line
    # being literally "first line". So if script succeeded, but there were no
    # changed files, then this file has only 2 lines.
    local filelist_lines="$(cat "$BACKUP_TMP".files | wc -l)"
    if [ "$filelist_lines" -gt 2 ]; then
        response_result="repaired"
    else
        response_result="kept"
    fi

    # Clean up
    rm "$BACKUP_TMP".sql "$BACKUP_TMP".files
}

. "$(dirname "$0")/cfengine.sh"
module_main "backup" "1.0"
