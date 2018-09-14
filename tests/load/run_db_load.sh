#!/bin/sh -e
echo "Starting run_db_load.sh test"
#for threads in 1 5 10 50; do
while false; do
  echo db_load $threads
  ./db_load $threads
done
