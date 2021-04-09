#!/bin/sh


teardown ()
{
  # because results in test.xml should not be parsed by CI or otherwise
  # failures there may be expected and will be checked by grep below
  rm test.xml
  rm summary.log
  rm test.log
  rm "$MY_LOG"

  # borrowed from testall (finish_xml)
  mv "$MY_XML" xml.tmp
  (
    cat <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="$(pwd)/selftest"
  timestamp="$(date "+%F %T")"
  hostname="localhost"
  tests="$TESTS_COUNT"
  failures="$FAILED_TESTS"
  time="$((END_TIME - START_TIME)) seconds">
EOF
    cat xml.tmp
    cat <<EOF
</testsuite>
EOF
  ) >> "$MY_XML"
  rm -f xml.tmp
}

fail ()
{
  local name="$1"
  local run_type="$2"
  cat <<EOF >> "$MY_XML"
<testcase name="./selftest.sh($run_type) $name">
  <failure type="FAIL"/>
</testcase>
EOF
}

pass ()
{
  local name="$1"
  local run_type="$2"
  cat <<EOF >> "$MY_XML"
<testcase name="./selftest.sh($run_type) $name"/>
EOF
}

check ()
{
  local expect="$1"
  local log="$2"
  local run_type="$3"
  if ! grep "$expect" "$log" >/dev/null
  then
    fail "$expect" "$run_type"
    return 1
  else
    pass "$expect" "$run_type"
    return 0
  fi
}

WORKDIR=workdir
MY_XML=selftest/test.xml
rm -f "$MY_XML"
MY_LOG="$WORKDIR/selftest.log"
mkdir -p "$WORKDIR"
FAILED_TESTS=0
TESTS_COUNT=0

./testall selftest > "$MY_LOG"
ret=$?
if [ "$ret" -ne "0" ]
then
  fail "exit code for selftest shoul be 0 but was $ret" "default"
  teardown
  exit 1
fi

_pwd=$(pwd)

for regex in \
"./selftest/fail.cf xFAIL (Suppressed, R: $_pwd/./selftest/fail.cf XFAIL)" \
"./selftest/flaky_fail.cf Flakey fail (R: $_pwd/./selftest/flaky_fail.cf FLAKEY)" \
"./selftest/flaky_pass.cf Pass" \
"./selftest/pass.cf Pass" \
"./selftest/skipped.cf Skipped (Test needs work)" \
"./selftest/soft.cf Soft fail (R: $_pwd/./selftest/soft.cf SFAIL)" \
"Passed tests:    2" \
"Failed tests:    1 (1 are known and suppressed)" \
"Skipped tests:   1" \
"Soft failures:   1" \
"Flakey failures: 1" \
"Total tests:     6"
do
  TESTS_COUNT=$((TESTS_COUNT + 1))
  check "$regex" "$MY_LOG" "default" || FAILED_TESTS=$((FAILED_TESTSs + 1))
done

FLAKEY_IS_FAIL=1 ./testall selftest > "$MY_LOG"
ret=$?
if [ "$ret" -ne "4" ]
then
  fail "error: exit code for testall with FLAKEY_IS_FAIL=1 should be 4 but was $ret" "FLAKEY_IS_FAIL=1"
  teardown
  exit 1
fi

#FAILED_TESTS=0

for regex in \
"./selftest/fail.cf FAIL (Suppressed, R: $_pwd/./selftest/fail.cf XFAIL)" \
"./selftest/flaky_fail.cf Flakey fail (R: $_pwd/./selftest/flaky_fail.cf FLAKEY)" \
"./selftest/flaky_pass.cf Pass" \
"./selftest/pass.cf Pass" \
"./selftest/skipped.cf Skipped (Test needs work)" \
"./selftest/soft.cf Soft fail (R: $_pwd/./selftest/soft.cf SFAIL)" \
"Passed tests:    2" \
"Failed tests:    1 (1 are known and suppressed)" \
"Skipped tests:   1" \
"Soft failures:   1" \
"Flakey failures: 1" \
"Total tests:     6"
do
  TESTS_COUNT=$((TESTS_COUNT + 1))
  check "$regex" "$MY_LOG" "FLAKEY_IS_FAIL=1" || FAILED_TESTS=$((FAILED_TESTS + 1))
done

teardown

exit 0
