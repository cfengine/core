#!/bin/bash

# Author: Mike Weilgart
# Date: 19 April 2017

# This script removes host records from a CFEngine Hub

########################################################################
#                      USAGE MESSAGE                                   #
########################################################################
usage() {
cat >&2 <<EOF
SYNOPSIS
        $0 --help
        $0 [-h CFEHUB] [-u USER] [-n] ( -f SHAFILE | -s SHALIST )

DESCRIPTION
        Given a file containing CFEngine hostkeys (with -f) or a
        comma-separated list of CFEngine hostkeys (with -s), remove
        all of them from the optionally-specified CFEngine Hub
        (default "localhost") using the REST API over HTTPS.

        The script will prompt for password before running.
        There is no option to specify the password on the command
        line; this is to prevent security breaches as it
        would then appear in the shell history file in plaintext.

OPTIONS
        -h CFEHUB       The CFEngine Hub from which to remove the
                        CFEngine records.  Default: localhost

        -u USER         The credentials to use to perform the deletions.
                        Must have "admin" rights on the hub.  Defaults
                        to the user "admin" if not specified on the
                        command line.

        -f SHAFILE      A text file containing a list of CFEngine
                        hostkeys (SHAs) to be removed, separated by
                        newlines.  This file will be validated before
                        use; if it contains anything but hostkeys, this
                        script will abort.

        -s SHALIST      A comma-separated list of hostkeys for removal.
                        Note that for ease of copy-paste, the leading
                        "SHA=" may be omitted from hostkeys (though
                        they may not in the file used with "-f").

        -n              Dry run.  Just print the commands that would be
                        executed (and do not prompt for password).
EOF
}

########################################################################
#                     OPTIONS HANDLING                                 #
########################################################################
[ "$1" = --help ] && { usage; exit 0;}

authuser=admin
hub=localhost
shaspectype=none
dryrun=''

while getopts :h:u:f:s:n opt; do
  case "$opt" in
    h)
      if [[ $OPTARG =~ ^[-._[:alnum:]]+$ ]]; then
        hub="${OPTARG}"
      else
        printf 'Invalid hub "%s"\n' "$OPTARG" >&2
        exit 1
      fi
      ;;
    u)
      if [[ $OPTARG =~ ^[[:alnum:]]+$ ]]; then
        authuser="${OPTARG}"
      else
        printf 'Invalid username "%s"\n' "$OPTARG" >&2
        exit 1
      fi
      ;;
    f)
      if [ "$shaspectype" = none ]; then
        shaspectype=file
        shafile="$OPTARG"
      else
        printf 'Error: You may only specify one of "-f" and "-s"\n' >&2
        exit 1
      fi
      ;;
    s)
      if [ "$shaspectype" = none ]; then
        shaspectype=list
        IFS=, read -ra shas <<< "$OPTARG"
      else
        printf 'Error: You may only specify one of "-f" and "-s"\n' >&2
        exit 1
      fi
      ;;
    n)
      dryrun='non-empy string for boolean true'
      ;;
    :)
      printf 'Option -%s requires an argument\n' "$OPTARG" >&2
      usage
      exit 1
      ;;
    *)
      printf 'Invalid option: -%s\n' "$OPTARG" >&2
      usage
      exit 1
      ;;
  esac
done
shift "$((OPTIND-1))"

########################################################################
#                         SHA VALIDATION                               #
########################################################################
case "$shaspectype" in
  list)
    for s in "${shas[@]}"; do
      sha="${s#SHA=}"
      [ "${#sha}" -eq 64 ] && [ "$sha" = "${sha//[^0-9a-f]}" ] || {
        printf 'Invalid SHA: %s\n' "$s" >&2
        exit 1
      }
    done
    ;;
  file)
    [ -f "$shafile" ] && [ -r "$shafile" ] || {
      printf 'Unreadable or invalid shafile: %s\n' "$shafile" >&2
      exit 1
    }
    # Validate file
    # See https://unix.stackexchange.com/a/354564/135943
    POSIXLY_CORRECT=nonempty awk '
        NF != 1 {
          print "Error: Illegal whitespace in shafile:"
          print "\t" $0
          print "\tFile: " FILENAME
          print "\tLine: " FNR
          exit 1
        }

        $1 !~ /^SHA=[a-f0-9]{64}$/ {
          print "Error: Invalid SHA:"
          print "\t" $1
          print "\tFile: " FILENAME
          print "\tLine: " FNR
          exit 1
        }

        $1 in shas {
          print "Error: SHA appears twice:"
          print "\t" $1
          print "\tFile: " FILENAME
          print shas[$1]
          print "\tLine: " FNR
          exit 1
        }

        {
          shas[$1] = "\tLine: " FNR
        }' "$shafile" || {
          printf 'No host records have been removed.\n' >&2
          exit 1
        }
    shas=($(cat "$shafile"))
    # Note that this will probably fail if given a GIGANTIC file -
    # but, we shouldn't be automatically removing that many records
    # anyway, so that's fine.
    ;;
  none|*)
    printf 'Error: you must specify either "-f" or "-s"\n' >&2
    exit 1
    ;;
esac

########################################################################
#                         AUTHENTICATION                               #
########################################################################

if [ "$dryrun" ]; then
  printf '# Dry run\n' >&2
else
  IFS= read -rsp "Enter password for CFEngine user $authuser: " password
  echo
  # Lazy man's JSON parsing; I just counted the spaces that occur before roles
  curl -s --user "$authuser":"$password" https://"$hub"/api/user/"$authuser" -k |
    awk  '/^        "admin"/ {exit 0}
          /Not authenticated|Error/ {print; exit 1}
          /^\}$/ {print "Error: User does not have admin role on hub"; exit 1}' ||
    exit 1
fi

########################################################################
#                              ACTION                                  #
########################################################################

for s in "${shas[@]}"; do
  sha="${s#SHA=}"
  if [ "$dryrun" ]; then
    printf '%s\n' "curl --user $authuser https://$hub/api/host/SHA=$sha -k -X delete"
  else
    printf 'Removing host record SHA=%s\n' "$sha"
    curl -s --user "$authuser":"$password" https://"$hub"/api/host/SHA="$sha" -k -X delete
  fi
done
