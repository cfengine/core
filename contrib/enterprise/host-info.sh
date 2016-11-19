#!/bin/bash

# Author: Mike Weilgart
# Date: 14 November 2016

# This is intended to be run on a CFEngine hub/policy server.
# My own copy of this script is called 'hi.sh' (for 'host info').

usage() {
cat >&2 <<EOF
SYNOPSIS
        $0 --help
        $0 [ -v ] [ -n ] { -i IPADDRESS | -h HOSTNAME | -s HOSTSHA }

DESCRIPTION
        Show host information from the Postgres 'variables' table
        for the specified host.

OPTIONS
        -v                  Include CFEngine array variables
        -n                  Dry-run mode, just print the query
EOF
}

[ "$1" = --help ] && { usage; exit 0;}

filt="and variablename not like '%[%'"
modes=0

while getopts :vs:i:h:n opt; do
  case "$opt" in
    v)
      filt=""
      ;;
    s)
      ((modes++))
      sha="${OPTARG#SHA=}"
      [ "${#sha}" -eq 64 ] && [ "$sha" = "${sha//[^0-9a-f]}" ] || {
        printf 'Invalid SHA: %s\n' "$OPTARG" >&2
        usage
        exit 1
      }
      choice="hostkey = 'SHA=$sha'"
      ;;
    i)
      ((modes++))
      [ "$OPTARG" = "${OPTARG//[^0-9.]}" ] || {
        printf 'Invalid IP address: %s\n' "$OPTARG" >&2
        usage
        exit 1
      }
      choice="hostkey in (select distinct hostkey from hosts where ipaddress = '$OPTARG' union select distinct hostkey from variables where variablename = 'ipv4' and bundle = 'sys' and variablevalue = '$OPTARG')"
      ;;
    h)
      ((modes++))
      [ "$OPTARG" = "${OPTARG//[^0-9a-zA-Z_.-]}" ] || {
        printf 'Invalid hostname: %s\n' "$OPTARG" >&2
        usage
        exit 1
      }
      choice="hostkey in (select distinct hostkey from hosts where hostname = '$OPTARG' union select distinct hostkey from variables where variablename = 'fqhost' and bundle = 'sys' and variablevalue = '$OPTARG')"
      ;;
    n)
      dryrun='non-empy string'
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

if [ "$modes" -eq 0 ]; then
  printf "Missing required option\n" >&2
  usage
  exit 1
elif [ "$modes" -gt 1 ]; then
  printf "Error: -h, -i and -s are mutually exclusive and may only be used once\n" >&2
  usage
  exit 1
fi

sel="select comp, variablevalue from variables"
ord="order by hostkey, 1"

setvals=( "SET host.identifier = 'default.sys.fqhost';" "SET rbac.filter = '!(any)';" )

printf '%s\n' "${setvals[@]}" "$sel where $choice $filt $ord;" | {
  if [ "$dryrun" ]; then
    cat
  else
    psql -X -U cfpostgres -d cfdb
  fi
}
