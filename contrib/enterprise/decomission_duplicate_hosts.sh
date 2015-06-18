#!/bin/sh

# Author: Aleksey Tsalolikhin <aleksey@verticalsysadmin.com>

# Run this script on the hub to purge "ghost" hosts --
# old entries that result when a host is re-bootstrapped (for example,
# to fix a broken update policy on the client)
# Needs to run on the hub.

USERNAME="admin"
PASSWORD="admin"
URL="http://localhost"

# TODO: Can support doing this remotely by only relying on REST API
# May use enterprise API for getting host list
cf-key -s > /root/cf-key.out

# list hosts that appear more than once in "cf-key -s" output
grep Incoming /root/cf-key.out | awk '{print $3}' | sort | uniq -d > /root/dupes

for f in `cat /root/dupes`
do
    # get the datestamps for both entries -- we're assuming two entries per host
    DATE1=`grep " ${f}" /root/cf-key.out | awk '{print $4, $5, $6, $7, $8}' | head -1`
    DATE2=`grep " ${f}" /root/cf-key.out | awk '{print $4, $5, $6, $7, $8}' | tail -1`

    # get the SHA IDs for each host, we'll need them later to delete
    # the host with the older last_seen time
    SHA1=`grep " ${f}" /root/cf-key.out | awk '{print $NF}' | head -1`
    SHA2=`grep " ${f}" /root/cf-key.out | awk '{print $NF}' | tail -1`

    # convert dates from human-readable to epoch (seconds) so we can compare them
    DATE1_s=`date --date="$DATE1" +%s`
    DATE2_s=`date --date="$DATE2" +%s`

    if [ "$DATE1_s" -gt "$DATE2_s" ];
    then
	echo noop > /dev/null;
	echo "host $f SHA1 $SHA1 DATE1 $DATE1 is newer than SHA2 $SHA2 DATE2 $DATE2";
	echo "deleting $SHA2"
	curl --user $USERNAME:$PASSWORD $URL/api/host/${SHA1} -X DELETE
    else
	echo "host $f SHA1 $SHA1 DATE1 $DATE1 is older than SHA2 $SHA2 DATE2 $DATE2";
	echo "deleting $SHA1"
	curl --user $USERNAME:$PASSWORD $URL/api/host/${SHA1} -X DELETE
    fi;
done 
