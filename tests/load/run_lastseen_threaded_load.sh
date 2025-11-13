#!/bin/sh

for skip_label in PACKAGES_x86_64_solaris_10 PACKAGES_ia64_hpux_11.23; do
  if [ "$label" = "$skip_label" ]; then
    echo "Skipping $0 on label $skip_label"
    exit 0;
  fi
done

echo "Starting run_lastseen_threaded_load.sh test"

./lastseen_threaded_load -c 1   4 1 1
