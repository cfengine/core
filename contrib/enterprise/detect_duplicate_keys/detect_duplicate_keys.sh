#!/bin/sh

# Detect duplicate CFEngine keys from different hosts.
# Run it for over 30 minutes (1800 seconds) to catch
# different hosts checking in with the same hostkey.

for n in {1..180}
do 
  sleep 10
  cf-key -s | sed -n 's/^Incoming *\([0-9.]*\).*\(\(SHA\|MD5\)=[0-9a-f]*\)$/\2\t\1/p'
done | ./detect_duplicate_keys.pl | sort | uniq
