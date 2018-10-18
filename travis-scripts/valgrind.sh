#!/usr/bin/env bash
set -e

if [ -d /var/cfengine ]; then
    rm -rf /var/cfengine
fi

echo "Installing CFEngine community (core)"
git fetch --unshallow
git remote add upstream https://github.com/cfengine/core.git  \
    && git fetch upstream 'refs/tags/*:refs/tags/*'

./autogen.sh --enable-debug
make
make install

echo "Downloading and installing masterfiles"
cd ../
git clone https://github.com/cfengine/masterfiles.git
cd masterfiles
./autogen.sh
make install

/var/cfengine/bin/cf-agent --version

VG_OPTS="--leak-check=full --track-origins=yes --error-exitcode=1"

valgrind $VG_OPTS /var/cfengine/bin/cf-key 2>&1 | tee cf-key.txt
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -B "$(ifconfig | grep -A1 Ethernet | sed '2!d;s/.*addr:\([0-9.]*\).*/\1/')" 2>&1 | tee bootstrap.txt
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f update.cf 2>&1 | tee update.txt
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f promises.cf 2>&1 | tee promises.txt
valgrind $VG_OPTS /var/cfengine/bin/cf-check lmdump -x /var/cfengine/state/cf_lock.lmdb 2>&1 | tee check.txt

echo "Killing cfengine processes"
ps aux | grep [c]f-

pkill -f cf-execd
pkill -f cf-serverd
pkill -f cf-monitord

echo "Done killing"
sleep 5

echo "Check that all outputs have '0 errors' in them"
grep "0 errors" cf-key.txt
grep "0 errors" bootstrap.txt
grep "0 errors" update.txt
grep "0 errors" promises.txt

echo "Check that bootstrap was successful"
grep "This host assumes the role of policy server" bootstrap.txt
grep "completed successfully!" bootstrap.txt

echo "Filtering output"
cat *.txt | sed "/ 0 errors/d" | sed "/and suppressed error/d" | sed "/a memory error detector/d" > filtered.txt

set +e
echo "Looking for errors in filtered output"
grep -i "error" filtered.txt
grep -i "at 0x" filtered.txt
grep -i "by 0x" filtered.txt
grep -i "error" filtered.txt && exit 1
grep -i "at 0x" filtered.txt && exit 1
grep -i "by 0x" filtered.txt && exit 1

echo "valgrind.sh success!"
