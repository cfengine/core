#!/bin/sh

if [ -z "$PKEY" ]; then
# if PKEY is not specified, run ssh using default keyfile
    ssh "$@"
else
    ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=quiet -i "$PKEY" "$@"
fi
