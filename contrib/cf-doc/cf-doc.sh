#!/bin/bash
# Author: Mike Weilgart
# Date: 18 January 2019
# Purpose: Extract summary documentation from specially prepared CFEngine policy.

# url_prefix should have a trailing slash
url_prefix=https://example.com/cfengine/masterfiles/blob/

collection='
{
  "metapromisers": {
    "inventory": "Inventory",
    "config": "Configuration"
  },
  "metatags": {
    "docinv": "Inventory",
    "docconfig": "Configuration"
  }
}
'

########################################################################
#                      USAGE MESSAGE                                   #
########################################################################
usage() {
cat >&2 <<EOF
SYNOPSIS
        cf-doc --help
        cf-doc [-f FILE] [-p VERSION] [-u URL] [-t]

DESCRIPTION
        cf-doc will extract particular meta tags and meta promises
        from CFEngine policy and will output a list of them in
        Markdown format, to document and index a codebase of
        CFEngine policy.

        Each line of output is formatted as a URL linking back
        to the line of policy from which it was extracted.
        The -t option can also be used to generate plain text
        (no hyperlinks).

        cf-promises is used internally to parse CFEngine policy and
        generate json output, which is further parsed to produce
        the final script output.

        The particular meta tags and meta promises to be collected
        are as defined in the "collection" variable in this script.
        For further details on this see the inline documentation
        in the supporting script 'extract-cf-meta.jq'.

OPTIONS
        -f FILE         File to pass to cf-promises for parsing.
                        If not specified cf-promises will use its own
                        default of /var/cfengine/inputs/promises.cf

        -p VERSION      Version of the policy that is being checked,
                        for inclusion in the URL.  Default is master.

        -u URL          URL prefix to use for all links.  Default is
                        coded into the script at the top and is intended
                        for modification to suit your site, but can also
                        be overridden on the command line with -u.
                        Current default is $url_prefix

        -t              Text only mode.  The -v and -u switches do nothing
                        if this option is passed, as the output will not
                        be markdown formatted as URLs.
EOF
}

########################################################################
#                     OPTIONS HANDLING                                 #
########################################################################
[ "$1" = --help ] && { usage; exit 0;}

unset filepassed
policy_version=master
# url_prefix is set at the top for easy customization by users
textonly=''

while getopts :f:p:u:t opt; do
  case "$opt" in
    f)
      filepassed="${OPTARG}"
      ;;
    p)
      policy_version="${OPTARG}"
      ;;
    u)
      url_prefix="${OPTARG}"
      ;;
    t)
      textonly='non-empty string for boolean true'
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
#                     MAIN ACTION                                      #
########################################################################

thisdir="$(dirname "$0")"

# Take note the rest of the script is ONE pipeline.

# Generation
cf-promises -p json-full ${filepassed+-f "$filepassed"} |

  # Extraction
  jq --argjson collection "$collection" -f "$thisdir"/extract-cf-meta.jq |

  # Formatting
  if [ "$textonly" ]; then

    # Omit links from the output, just show the text of the various meta info
    jq -r '"\n# " + .header , (.info[] | "- " + .text)'

  else

    # Proper URLs for e.g. GitLab will need the prefix stripped from
    # the file paths that cf-promises will output, so let's clean those up.
    if [ "${filepassed%/*}" = "$filepassed" ]; then
      # This means no slash in the filepassed, or no file passed at all.
      # Either way we use the same default as cf-promises.
      trimstring='/var/cfengine/inputs/'
    else
      # If an explicit file was passed, we use its prefix to trim cf-promise output.
      trimstring="$(dirname "$filepassed")/"
    fi

    # Print formatted output with URLs
    jq --arg url_prefix "$url_prefix" --arg policy_version "$policy_version" --arg trimstring "$trimstring" -r '
      "\n# " + .header
      ,
      (.info[]
      | "- ["
        + .text
        + "]("
        + $url_prefix
        + $policy_version
        + "/"
        + (.file|ltrimstr($trimstring))
        + "#L"
        + (.linenumber|tostring)
        + ")"
      )
    '

  fi
