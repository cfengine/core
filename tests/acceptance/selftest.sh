#!/bin/sh
set -x

teardown ()
{
  # if selftest.sh is run by itself or before testall there is no test.xml with a summary to change
  if [ ! -f all-test.xml ]
  then
    cat <<EOF > all-test.xml
<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="$(pwd)"
           timestamp="$(date +\"%F %T\")"
           hostname="localhost"
           tests="0"
           failures="0"
           skipped="0"
           time="1 seconds">
EOF
  fi

  # remove the closing </testsuite> tag in all-test.xml
  sed -i '/<\/testsuite>/d' all-test.xml

  # adjust the testsuite tests= and failures= counts
  local OLD_FAILED_TESTS=$(awk 'BEGIN{FS="="} /failures=/ {gsub(/"/, ""); print $2}' all-test.xml)
  local OLD_TESTS_COUNT=$(awk 'BEGIN{FS="="} /tests=/ {gsub(/"/, ""); print $2}' all-test.xml)
  local NEW_FAILED_TESTS=$((OLD_FAILED_TESTS + FAILED_TESTS))
  local NEW_TESTS_COUNT=$((OLD_TESTS_COUNT + TESTS_COUNT))
  sed -i "s,tests=\"$OLD_TESTS_COUNT\",tests=\"$NEW_TESTS_COUNT\"," all-test.xml
  sed -i "s,failures=\"$OLD_FAILED_TESTS\",failures=\"$NEW_FAILED_TESTS\"," all-test.xml

  # append <testcase/> elements for each of my tests from the built-up selftest.xml file
  cat selftest.xml >> all-test.xml
  rm -f selftest.xml # to avoid eager CI from parsing

  # add back the closing testsuite tag
  echo "</testsuite>" >> all-test.xml

  mv all-test.xml test.xml
  mv all-summary.log summary.log
  mv all-test.log test.log
}

fail ()
{
  local name="$1"
  local classname=$(echo $name | awk '{print $1}')
  local run_type="$2"
  cat <<EOF >> "$MY_XML"
<testcase name="$name"
  classname="./selftest.sh/$run_type/$classname">
  <failure type="FAIL"/>
</testcase>
EOF
}

pass ()
{
  local name="$1"
  local classname=$(echo $name | awk '{print $1}')
  local run_type="$2"
  cat <<EOF >> "$MY_XML"
<testcase name="$name"
  classname="./selftest.sh/$run_type/$classname">
</testcase>
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
MY_XML=selftest.xml
rm -f "$MY_XML"
MY_LOG="$WORKDIR/selftest.log"
mkdir -p "$WORKDIR"
FAILED_TESTS=0
TESTS_COUNT=0

# make backups of test.xml, summary.log and test.log
mv test.xml all-test.xml
mv summary.log all-summary.log
mv test.log all-test.log


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
  check "$regex" "$MY_LOG" "default" || FAILED_TESTS=$((FAILED_TESTS + 1))
done

FLAKEY_IS_FAIL=1 ./testall selftest > "$MY_LOG"
ret=$?
if [ "$ret" -ne "4" ]
then
  fail "error: exit code for testall with FLAKEY_IS_FAIL=1 should be 4 but was $ret" "FLAKEY_IS_FAIL=1"
  teardown
  exit 1
fi

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
  check "$regex" "$MY_LOG" "FLAKEY_IS_FAIL" || FAILED_TESTS=$((FAILED_TESTS + 1))
done

teardown

exit 0
