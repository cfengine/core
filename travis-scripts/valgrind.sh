#!/usr/bin/env bash
set -e

function print_ps {
    set +e
    echo "CFEngine processes:"
    ps aux | grep [c]f-

    echo "Valgrind processes:"
    ps aux | grep [v]algrind
    set -e
}

function no_errors {
    set +e
    grep -i "error" $1
    grep -i "error" $1 && exit 1
    set -e
}

function check_daemon_output {
    echo "Examining $1:"
    no_errors $1
}

function check_output {
    set -e
    if [ ! -f "$1" ]; then
        echo "$1 does not exists!"
        exit 1
    fi
    echo "Looking for problems in $1:"
    grep -i "ERROR SUMMARY: 0 errors" "$1"
    cat $1 | sed -e "/ 0 errors/d" -e "/and suppressed error/d" -e "/a memory error detector/d" -e "/This database contains unknown binary data/d" > filtered.txt
    no_errors filtered.txt
    set +e
    grep -i "at 0x" filtered.txt
    grep -i "at 0x" filtered.txt && exit 1
    grep -i "by 0x" filtered.txt
    grep -i "by 0x" filtered.txt && exit 1
    grep -i "Failed to connect" filtered.txt
    grep -i "Failed to connect" filtered.txt && exit 1
    set -e
}

function check_serverd_valgrind_output {
    if [ ! -f "$1" ]; then
        echo "$1 does not exists!"
        exit 1
    fi
    set -e
    echo "Serverd has 1 expected valgrind error in travis because of old glibc"
    echo "Because of this we use special assertions on output"
    echo "Looking for problems in $1:"
    grep -i "definitely lost" $1
    grep -i "indirectly lost" $1
    grep -i "ERROR SUMMARY" $1
    grep -i "definitely lost: 0 bytes in 0 blocks" $1
    grep -i "indirectly lost: 0 bytes in 0 blocks" $1
    grep -i "ERROR SUMMARY: 0 errors" "$1"

    cat $1 | sed -e "/ERROR SUMMARY/d" -e "/and suppressed error/d" -e "/a memory error detector/d" > filtered.txt

    no_errors filtered.txt
    set +e
    grep -i "at 0x" filtered.txt
    grep -i "at 0x" filtered.txt && exit 1
    grep -i "by 0x" filtered.txt
    grep -i "by 0x" filtered.txt && exit 1
    set -e
}

function check_masterfiles_and_inputs {
    set -e
    echo "Comparing promises.cf from inputs and masterfiles:"
    diff /var/cfengine/inputs/promises.cf /var/cfengine/masterfiles/promises.cf
}

VG_OPTS="--leak-check=full --track-origins=yes --error-exitcode=1"
BOOTSTRAP_IP="$(ifconfig | grep -C1 Ethernet | sed 's/.*inet \([0-9.]*\).*/\1/;t;d')"

valgrind $VG_OPTS /var/cfengine/bin/cf-key 2>&1 | tee cf-key.txt
check_output cf-key.txt
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -B $BOOTSTRAP_IP 2>&1 | tee bootstrap.txt
check_output bootstrap.txt

# Validate all databases here, because later, we cannot validate
# cf_lastseen.lmdb:
echo "Running cf-check diagnose --validate on all databases:"
valgrind $VG_OPTS /var/cfengine/bin/cf-check diagnose --validate 2>&1 | tee cf_check_validate_all.txt
check_output cf_check_validate_all.txt

check_masterfiles_and_inputs

print_ps

echo "Stopping service to relaunch under valgrind"
systemctl stop cfengine3
sleep 10
print_ps

# The IP we bootstrapped to cannot actually be used for communication.
# This ensures that cf-serverd binds to loopback interface, and cf-net
# connects to it:
echo "127.0.0.1" > /var/cfengine/policy_server.dat

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
check_output get.txt

echo "Checking promises.cf diff (from cf-net GET):"
diff ./promises.cf /var/cfengine/masterfiles/promises.cf

echo "Running update.cf:"
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f update.cf 2>&1 | tee update.txt
check_output update.txt
check_masterfiles_and_inputs
echo "Running update.cf without local copy:"
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f update.cf -D mpf_skip_local_copy_optimization 2>&1 | tee update2.txt
check_output update2.txt
check_masterfiles_and_inputs
echo "Running promises.cf:"
valgrind $VG_OPTS /var/cfengine/bin/cf-agent -K -f promises.cf 2>&1 | tee promises.txt
check_output promises.txt

# Dump all databases, use grep to filter the JSON lines
# (optional whitespace then double quote or curly brackets).
# Some of the databases have strings containing "error"
# which check_output greps for.
echo "Running cf-check dump:"
valgrind $VG_OPTS /var/cfengine/bin/cf-check dump 2>&1 | grep -E '\s*[{}"]' --invert-match | tee cf_check_dump.txt
check_output cf_check_dump.txt

echo "Running cf-check diagnose on all databases"
valgrind $VG_OPTS /var/cfengine/bin/cf-check diagnose 2>&1 | tee cf_check_diagnose.txt
check_output cf_check_diagnose.txt

# Because of the hack with bootstrap IP / policy_server.dat above
# lastseen would not pass validation:
echo "Running cf-check diagnose --validate on all databases except cf_lastseen.lmdb:"
find /var/cfengine/state -name '*.lmdb' ! -name 'cf_lastseen.lmdb' -exec \
    valgrind $VG_OPTS /var/cfengine/bin/cf-check diagnose --validate {} + 2>&1 \
    | tee cf_check_validate_no_lastseen.txt
check_output cf_check_validate_no_lastseen.txt

echo "Checking that bootstrap ID doesn't change"
/var/cfengine/bin/cf-agent --show-evaluated-vars | grep bootstrap_id > id_a
/var/cfengine/bin/cf-agent -K --show-evaluated-vars | grep bootstrap_id > id_b
cat id_a
diff id_a id_b

echo "Checking that bootstrap ID has expected length"
[ `cat id_a | awk '{print $2}' | wc -c` -eq 41 ]

print_ps

echo "Checking that serverd and execd PIDs are still correct/alive:"
ps -p $exec_pid
ps -p $server_pid

echo "Killing valgrind cf-execd"
kill $exec_pid
echo "Killing valgrind cf-serverd"
kill $server_pid
sleep 30

echo "Output from cf-execd in valgrind:"
cat execd.txt
check_output execd.txt
check_daemon_output execd_output.txt

echo "Output from cf-serverd in valgrind:"
cat serverd.txt
check_serverd_valgrind_output serverd.txt
check_daemon_output serverd_output.txt

echo "Stopping cfengine3 service"
systemctl stop cfengine3

echo "Done killing"
sleep 10
print_ps

echo "Check that bootstrap was successful"
grep "This host assumes the role of policy server" bootstrap.txt
grep "completed successfully!" bootstrap.txt

echo "valgrind_health_check successful! (valgrind.sh)"
