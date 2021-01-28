#!/bin/sh
#
# Sample custom promise type, uses cfengine.sh library located in same dir.
#
# Use it in the policy like this:
# promise agent cp
# {
#     interpreter => "/bin/bash";
#     path => "$(sys.inputdir)/cp.sh";
# }
# bundle agent main
# {
#   cp:
#     "/home/vagrant/dst"
#       from => "/home/vagrant/src";
# }

obligatory_attributes="from"
optional_attributes=""
all_attributes_are_valid="no"

do_evaluate() {
    if diff -q "$request_attribute_from" "$request_promiser" 2>/dev/null; then
        response_result="kept"
        return 0
    fi
    if cp -a "$request_attribute_from" "$request_promiser"; then
        response_result="repaired"
    else
        response_result="not_kept"
    fi
}

. "$(dirname "$0")/cfengine.sh"
module_main "cp" "1.0"
