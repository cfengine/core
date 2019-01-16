#!/bin/jq -f
#
# Author: Mike Weilgart
# Date: 18 January 2019
# Purpose: This is a supporting script for cf-doc.sh
#
# Its input should be the output of cf-promises with
# argument '-p json-full'.  Also it expects to have
# a variable called "collection" (jq variable) defined prior
# to running.  The collection variable should be defined
# as a json object with two keys, "metapromisers" and
# "metatags".
#
# Here is an example:
#
#   cf-promises -p json-full |
#   jq --argjson collection '
#   {
#     "metapromisers": {
#       "inventory": "Inventory",
#       "config": "Configuration"
#     },
#     "metatags": {
#       "docinv": "Inventory",
#       "docconfig": "Configuration"
#     }
#   }
#   ' -f thisscript
#
# The final output is (potentially) a JSON object for each
# distinct innermost value in the "collection" variable.
#
# Each of these values becomes a "header" value in its own
# output object.  So in the above example, there would be
# two output objects, one with a "header" value of "Inventory"
# and the other with a "header" value of "Configuration".
#
# The output JSON objects have only two keys: "header" and "info".
# The value for the "info" key is an array of objects,
# each with three keys: "file", "linenumber" and "text".
#
# The first two are self explanatory; together they refer to
# an exact line of CFEngine policy.
#
# "text" refers to the string attribute of a meta promise,
# OR the right hand side of a meta tag.  For example, with
# the above value for the collection variable, information
# for the docs could be written as a meta promise like so:
#
#   meta:
#     context_is_ignored::
#       "config"
#         string => "Line of text to go in the doc";
#
# OR, equivalently:
#
#   files: # Or any promise type
#     context_still_ignored::
#       "/promiser/does/not/matter"
#         meta => { "docconfig=Line of text to go in the doc" },
#         create => "true"; # Only the meta tag matters
#
# Either one of these will produce an output object like so:
#
#   {
#     "header": "Configuration",
#     "info": [
#       {
#         "file": "/var/cfengine/inputs/path/to/policy.cf",
#         "linenumber": 42,
#         "text": "Line of text to go in the doc"
#       }
#     ]
#   }

[
  .bundles[]
  | {file: .sourcePath}
    +
    (.promiseTypes[]
    | select(.name == "meta")
    | .contexts[].promises[]
    | select(.promiser|in($collection.metapromisers))
    | {category: $collection.metapromisers[.promiser]}
      +
      (.attributes[]
      | select(.lval == "string")
      | {linenumber: .line,text: .rval.value }
      )
    )
]
+
[
  .bundles[]
  | {file: .sourcePath}
    +
    (.promiseTypes[].contexts[].promises[].attributes[]
    | select(.lval == "meta")
    | {linenumber: .line}
      +
      (.rval.value[].value
      | split("=")
      | select(.[0]|in($collection.metatags))
      | {category: $collection.metatags[.[0]]}
        +
        {text: .[1]}
      )
    )
]
| group_by(.category)[]
# Hat tip to https://stackoverflow.com/a/43221520/5419599
| {header: (.[0].category),info: [.[]|del(.category)]}
