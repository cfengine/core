#!/bin/bash

logdir=serverd-multi-versions-logs
statsdir=serverd-multi-versions-stats

mkdir -p hub
mkdir -p client
mkdir -p $logdir
mkdir -p $statsdir
cat >server.sh <<EOF
#!/bin/sh

LD_LIBRARY_PATH=$PWD/hub/var/cfengine/lib \
CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR=$PWD/hub/var/cfengine/lib \
CFENGINE_TEST_OVERRIDE_WORKDIR=$PWD/work_here \
$PWD/hub/var/cfengine/bin/cf-serverd "\$@"
EOF
cat >agent.sh <<EOF
#!/bin/sh
LD_LIBRARY_PATH=$PWD/agent/var/cfengine/lib \
CFENGINE_TEST_OVERRIDE_EXTENSION_LIBRARY_DIR=$PWD/agent/var/cfengine/lib \
CFENGINE_TEST_OVERRIDE_WORKDIR=$PWD/work_here \
$PWD/agent/var/cfengine/bin/cf-agent "\$@"
EOF
chmod a+x *.sh
sed -i "s%\$(sys.cf_agent)%$PWD/agent.sh%g;s%\$(sys.cf_serverd)%$PWD/server.sh%g" \
tests/acceptance/run_with_server.cf.sub
chmod -R 700 tests
sed -i 's/\s*#.*//;/^\s*$/d' tests/acceptance/serverd-multi-versions-blacklist.txt

# Prepare stub workdir
mkdir -p work_stub/inputs/
cp -a tests/acceptance work_stub/inputs/
rm -rf work_stub/inputs/acceptance/0*
mkdir -p work_stub/bin/
touch work_stub/bin/cf-promises
chmod a+x work_stub/bin/cf-promises

sudo hostname localhost
# Ensure reverse hostname resolution is correct and 127.0.0.1 is always 'localhost'.
# There's no nice shell command to test it but this one:
# python -c 'import socket;print socket.gethostbyaddr("127.0.0.1")'
sudo sed -i -e '1s/^/127.0.0.1 localhost localhost.localdomain\n/' /etc/hosts

# downloads $3 (if provided), saves it as $2 and extracts it to $1
# if $2 not provided - installs currently built version
function install(){
    test -z "$1" && return 1
    rm -rf "$1"
    if test -z "$2"
    then
        ./configure --enable-debug --with-init-script --prefix=$PWD/$1/var/cfengine >/dev/null 2>&1
        make >/dev/null 2>&1
        make install >/dev/null 2>&1
    else
        test ! -z "$3" -a ! -f "$2" && wget "$3" -O "$2"
        dpkg -x "$2" "$1"
    fi
}

function testit(){
    echo "===== $1"
    ./server.sh -V
    ./agent.sh -V
    ls tests/acceptance/16_cf-serverd/serial/*.cf \
        | grep -v -f tests/acceptance/serverd-multi-versions-blacklist.txt \
        | sed 's%^tests/%%' \
        | while read file
    do
        logfilename="$1-$(basename $file)"
        logfile="$logdir/$logfilename"
        ./server.sh -V >> $logfile.log
        ./agent.sh -V >> $logfile.log

        # delete dirs left here from previous testing round, if any.
        # We keep deleting them until they disappear - they can be kept back by
        # still-running process from previous testing round.
        for d in  /tmp/TESTDIR.cfengine  work_here
        do
            while test -d  "$d"
            do
                rm -rf "$d"  ||  (
                    # Try to debug why dir failed to delete
                    ps auxww  |  grep '[c]f-'
                    ls -lR  "$d"
                )
            done
        done

        cp -a work_stub work_here
        # actually run the test
        ./agent.sh -Kf $file -D AUTO,debug_server >> $logfile.log
        retval=$?
        result=unknown
        if expr "$file" : '.*\.x\.cf$' >/dev/null
        then
                # test should exit with non-zero status
                # if it exits with status 128 or higher - then it's a CRASH
                if [ $retval -gt 0 ] && [ $retval -lt 128 ]
                then
                    result=passed
                else
                    result=failed
                fi
        else
            # test is expected to pass
            if [ $retval != 0 ]
            then
                result=failed
            else
                if grep "R: $PWD/work_here/inputs/$file Pass" $logfile.log  > /dev/null
                then
                    result=passed
                else
                    result=failed
                fi
            fi
        fi
        echo $logfile > $statsdir/$result-$logfilename

        if [ "$result" != "passed" ]
        then
            echo "$file  FAIL"
            mv work_here $logfile.workdir
            mv /tmp/TESTDIR.cfengine $logfile.testdir
        else
            echo "$file  Pass"
        fi
    done
}

install hub
install agent 3.10.deb https://cfengine-package-repos.s3.amazonaws.com/community_binaries/Community-3.10.2/agent_debian7_x86_64/cfengine-community_3.10.2-1_amd64-debian7.deb
testit master-hub-with-3.10.2-agent

install agent 3.7.deb https://cfengine-package-repos.s3.amazonaws.com/community_binaries/Community-3.7.6/agent_deb_x86_64/cfengine-community_3.7.6-1_amd64-debian4.deb
testit master-hub-with-3.7.6-agent

install agent
install hub 3.10.deb
testit master-agent-with-3.10.2-hub

install hub 3.7.deb
testit master-agent-with-3.7.6-hub

echo '===== Printing results ====='
echo "Passed tests:   $(ls $statsdir/pass* 2>/dev/null | wc -l)"
echo "FAILED tests:   $(ls $statsdir/fail* 2>/dev/null | wc -l)"
echo "Unknown result: $(ls $statsdir/unknown* 2>/dev/null | wc -l)"
echo '===== Failed tests ====='
cat $statsdir/fail* 2>/dev/null || NO_FAILED=true
echo '===== Unknown results ====='
cat $statsdir/unknown* 2>/dev/null || NO_UNKNOWNS=true

# Return code
test -z "$NO_FAILED" && exit 1
test -z "$NO_UNKNOWNS" && exit 2
exit 0
