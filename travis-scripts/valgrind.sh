#!/usr/bin/env bash
set -e

if [ -d /var/cfengine ]; then
    rm -rf /var/cfengine
fi

echo "Building CFEngine core"
set +e
git fetch --unshallow 2>&1 >> /dev/null
git remote add upstream https://github.com/cfengine/core.git  \
    && git fetch upstream 'refs/tags/*:refs/tags/*' 2>&1 >> /dev/null
set -e

./autogen.sh --enable-debug
make
make install

echo "Downloading and installing masterfiles"
cd ../
if [ ! -d masterfiles ]; then
    git clone https://github.com/cfengine/masterfiles.git
fi
cd masterfiles
./autogen.sh
make install

function print_ps {
    set +e
    echo "CFEngine processes:"
    ps aux | grep [c]f-

    echo "Valgrind processes:"
    ps aux | grep [v]algrind
    set -e
}

function check_output {
    set -e
    if [ ! -f "$1" ]; then
        echo "$1 does not exists!"
        exit 1
    fi
    echo "Looking for problems in $1:"
    grep "ERROR SUMMARY: 0 errors" "$1" || cat "$1"
    grep "ERROR SUMMARY: 0 errors" "$1" 2>&1 > /dev/null
    cat $1 | sed -e "/ 0 errors/d" -e "/and suppressed error/d" -e "/a memory error detector/d" > filtered.txt
    set +e
    grep -i "error" filtered.txt
    grep -i "error" filtered.txt && exit 1
    grep -i "at 0x" filtered.txt
    grep -i "at 0x" filtered.txt && exit 1
    grep -i "by 0x" filtered.txt
    grep -i "by 0x" filtered.txt && exit 1
    set -e
}

/var/cfengine/bin/cf-agent --version

VG_OPTS="--leak-check=full --track-origins=yes --error-exitcode=1"
BOOTSTRAP_IP="$(ifconfig | grep -A1 Ethernet | sed '2!d;s/.*addr:\([0-9.]*\).*/\1/')"

valgrind $VG_OPTS /var/cfengine/bin/cf-key 2>&1 | tee cf-key.txt
check_output cf-key.txt
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -B $BOOTSTRAP_IP 2>&1 | tee bootstrap.txt
check_output bootstrap.txt

echo "Killing serverd and execd to relaunch under valgrind"
ps aux | grep [c]f-
pkill -f cf-execd
pkill -f cf-serverd
sleep 5
print_ps

echo "Starting cf-serverd with valgrind in background:"
valgrind $VG_OPTS  --run-libc-freeres=no --log-file=serverd.txt /var/cfengine/bin/cf-serverd --no-fork 2>&1 &
server_pid="$!"
echo "Starting cf-execd with valgrind in background:"
valgrind $VG_OPTS --log-file=execd.txt /var/cfengine/bin/cf-execd --no-fork 2>&1 &
exec_pid="$!"
sleep 5
print_ps

echo "Running update.cf:"
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f update.cf 2>&1 | tee update.txt
check_output update.txt
echo "Running update.cf without local copy:"
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f update.cf -D mpf_skip_local_copy_optimization 2>&1 | tee update2.txt
check_output update2.txt
echo "Running promises.cf:"
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f promises.cf 2>&1 | tee promises.txt
check_output promises.txt
echo "Running cf-check:"
valgrind $VG_OPTS /var/cfengine/bin/cf-check lmdump -x /var/cfengine/state/cf_lock.lmdb 2>&1 | tee check.txt
check_output check.txt

print_ps

echo "Checking that serverd and execd PIDs are still correct/alive:"
ps -p $exec_pid
ps -p $server_pid

echo "Killing valgrind"
kill $exec_pid
kill $server_pid
sleep 5

echo "Output from cf-execd in valgrind:"
cat execd.txt
check_output execd.txt

echo "Output from cf-serverd in valgrind:"
cat serverd.txt
check_output serverd.txt

echo "Killing cf-monitord"
pkill -f cf-monitord

echo "Done killing"
sleep 5
print_ps

echo "Check that bootstrap was successful"
grep "This host assumes the role of policy server" bootstrap.txt
grep "completed successfully!" bootstrap.txt

echo "valgrind_health_check successful! (valgrind.sh)"
