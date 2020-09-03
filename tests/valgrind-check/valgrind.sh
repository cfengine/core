#!/bin/bash

function print_ps {
    set +e
    echo "CFEngine processes:"
    ps aux | grep [c]f-

    echo "Valgrind processes:"
    ps aux | grep [v]algrind
    set -e
}

function stop_daemons {
    echo "Stopping cfengine daemons"
    pkill -f cf-serverd  || echo "Did not kill cf-serverd"
    pkill -f cf-execd    || echo "Did not kill cf-execd"
    pkill -f cf-monitord || echo "Did not kill cf-monitord"
}

function no_errors {
    set +e
    grep -i "error" $1
    grep -q -i "error" $1 && exit 1
    set -e
}

function check_daemon_output {
    echo "Examining $1:"
    no_errors $1
}

function check_valgrind_output {
    set -e
    if [ ! -f "$1" ]; then
        echo "$1 does not exists!"
        exit 1
    fi
    echo "Looking for problems in $1:"
    grep -i "ERROR SUMMARY: 0 errors" "$1"
    cat $1 | sed -e "/ 0 errors/d" -e "/and suppressed error/d" -e "/a memory error detector/d" > filtered.txt
    no_errors filtered.txt
    set +e
    grep -i "at 0x" filtered.txt
    grep -i -q "at 0x" filtered.txt && exit 1
    grep -i "by 0x" filtered.txt
    grep -i -q "by 0x" filtered.txt && exit 1
    grep -i "Failed to connect" filtered.txt
    grep -i -q "Failed to connect" filtered.txt && exit 1
    set -e
}

function check_masterfiles_and_inputs {
    set -e
    echo "Comparing promises.cf from inputs and masterfiles:"
    diff -u /var/cfengine/inputs/promises.cf /var/cfengine/masterfiles/promises.cf
}

set -e
set -x

# Assume we are in core directory
if [ -f ./configure ] ; then
  ./configure -C --enable-debug
else
  ./autogen.sh -C --enable-debug
fi
make
make install

cd ../masterfiles
if [ -f ./configure ] ; then
  ./configure -C --enable-debug
else
  ./autogen.sh -C --enable-debug
fi
make
make install

/var/cfengine/bin/cf-agent --version

VG_OPTS="--leak-check=full --track-origins=yes --error-exitcode=1"
BOOTSTRAP_IP="127.0.0.1"

valgrind $VG_OPTS /var/cfengine/bin/cf-key 2>&1 | tee cf-key.txt
check_valgrind_output cf-key.txt
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -B $BOOTSTRAP_IP 2>&1 | tee bootstrap.txt
check_valgrind_output bootstrap.txt

echo "Running cf-check diagnose --validate on all databases:"
valgrind $VG_OPTS /var/cfengine/bin/cf-check diagnose --validate 2>&1 | tee cf_check_validate_all_1.txt
check_valgrind_output cf_check_validate_all_1.txt

check_masterfiles_and_inputs

print_ps

echo "Stopping before relaunch under valgrind"
stop_daemons
sleep 10
print_ps

echo "Starting cf-serverd with valgrind in background:"
valgrind $VG_OPTS --log-file=serverd.txt /var/cfengine/bin/cf-serverd --no-fork 2>&1 > serverd_output.txt &
server_pid="$!"
sleep 20

echo "Starting cf-execd with valgrind in background:"
valgrind $VG_OPTS --log-file=execd.txt /var/cfengine/bin/cf-execd --no-fork 2>&1 > execd_output.txt &
exec_pid="$!"
sleep 10

print_ps

echo "Running cf-net:"
valgrind $VG_OPTS /var/cfengine/bin/cf-net GET /var/cfengine/masterfiles/promises.cf 2>&1 | tee get.txt
check_valgrind_output get.txt

echo "Checking promises.cf diff (from cf-net GET):"
diff -u ./promises.cf /var/cfengine/masterfiles/promises.cf

echo "Running update.cf:"
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f update.cf 2>&1 | tee update.txt
check_valgrind_output update.txt
check_masterfiles_and_inputs
echo "Running update.cf without local copy:"
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f update.cf -D mpf_skip_local_copy_optimization 2>&1 | tee update2.txt
check_valgrind_output update2.txt
check_masterfiles_and_inputs
echo "Running promises.cf:"
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f promises.cf 2>&1 | tee promises.txt
check_valgrind_output promises.txt

# Dump all databases, use grep to filter the JSON lines
# (optional whitespace then double quote or curly brackets).
# Some of the databases have strings containing "error"
# which check_valgrind_output greps for.
echo "Running cf-check dump:"
valgrind $VG_OPTS /var/cfengine/bin/cf-check dump 2>&1 | grep -E '\s*[{}"]' --invert-match | tee cf_check_dump.txt
check_valgrind_output cf_check_dump.txt

echo "Running cf-check diagnose on all databases"
valgrind $VG_OPTS /var/cfengine/bin/cf-check diagnose 2>&1 | tee cf_check_diagnose.txt
check_valgrind_output cf_check_diagnose.txt

echo "Running cf-check diagnose --validate on all databases:"
valgrind $VG_OPTS /var/cfengine/bin/cf-check diagnose --validate 2>&1 | tee cf_check_validate_all_2.txt
check_valgrind_output cf_check_validate_all_2.txt

echo "Checking that bootstrap ID doesn't change"
/var/cfengine/bin/cf-agent --show-evaluated-vars | grep bootstrap_id > id_a
/var/cfengine/bin/cf-agent -K --show-evaluated-vars | grep bootstrap_id > id_b
cat id_a
diff id_a id_b

echo "Checking that bootstrap ID has expected length"
[ `cat id_a | awk '{print $2}' | wc -c` -eq 41 ]

print_ps

echo "cf-execd outputs:"
tail execd_output.txt
tail execd.txt

echo "cf-serverd outputs:"
tail serverd_output.txt
tail serverd.txt

echo "Checking that serverd and execd PIDs are still correct/alive:"
ps -p $exec_pid
ps -p $server_pid

echo "Killing valgrind cf-execd"
kill $exec_pid
echo "Killing valgrind cf-serverd"
kill $server_pid

wait $exec_pid
wait $server_pid

echo "Output from cf-execd in valgrind:"
cat execd.txt
check_valgrind_output execd.txt
check_daemon_output execd_output.txt

echo "Output from cf-serverd in valgrind:"
cat serverd.txt
check_valgrind_output serverd.txt
check_daemon_output serverd_output.txt

stop_daemons

echo "Done killing"
sleep 10
print_ps

echo "Check that bootstrap was successful"
grep "This host assumes the role of policy server" bootstrap.txt
grep "completed successfully!" bootstrap.txt

echo "valgrind_health_check successful! (valgrind.sh)"
