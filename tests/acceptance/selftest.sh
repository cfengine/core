#!/bin/sh


# because results in test.xml should not be parsed by CI or otherwise
# failures there may be expected and will be checked by grep below
clean ()
{
  rm -f test.xml
}
check ()
{
  expect=$1
  log=$2
  if ! grep "$expect" "$log" >/dev/null
  then
    echo "error: failed to match regex \"$expect\" in log file $log"
    return 1
  fi
}

mkdir -p workdir

./testall selftest > workdir/selftest_normal.log
ret=$?
if [ "$ret" -ne "1" ]
then
  echo "error: exit code for selftest should be 1 but was $ret"
  clean
  exit 1
fi

log="workdir/selftest_normal.log"

errors=0
_pwd=$(pwd)

for regex in \
"./selftest/fail.cf FAIL (Suppressed, R: $_pwd/./selftest/fail.cf XFAIL)" \
"./selftest/flakey_meta_fail.cf Flakey fail (ENT-6947)" \
"./selftest/flakey_meta_no_ticket.cf FAIL (Tried to suppress failure, but no issue number is provided) (UNEXPECTED FAILURE)" \
"./selftest/flaky_fail.cf Flakey fail (R: $_pwd/./selftest/flaky_fail.cf FLAKEY)" \
"./selftest/flaky_pass.cf Pass" \
"./selftest/pass.cf Pass" \
"./selftest/skipped.cf Skipped (Test needs work)" \
"./selftest/soft.cf Soft fail (R: $_pwd/./selftest/soft.cf SFAIL)" \
"./selftest/soft_no_ticket.cf FAIL (Tried to suppress failure, but no issue number is provided) (UNEXPECTED FAILURE)" \
"Passed tests:    2" \
"Failed tests:    3 (1 are known and suppressed)" \
"Skipped tests:   1" \
"Soft failures:   1" \
"Flakey failures: 2" \
"Total tests:     9"
do
  check "$regex" "$log" || errors=$((errors + 1))
done
if [ "$errors" -ne "0" ]
then
  echo "error: $errors error(s) occurred for selftest (flakey is not fail)"
  echo "=== BEGIN $log ==="
  cat $log
  echo "=== END $log ==="
  clean
  exit 1
fi


FLAKEY_IS_FAIL=1 ./testall selftest/flakey_meta_fail.cf selftest/flaky_fail.cf selftest/flaky_pass.cf > workdir/selftest_flakey_is_fail.log
ret=$?
if [ "$ret" -ne "4" ]
then
  echo "error: exit code for testall with FLAKEY_IS_FAIL=1 should be 4 but was $ret"
  clean
  exit 1
fi

errors=0
log="workdir/selftest_flakey_is_fail.log"

for regex in \
"./selftest/flakey_meta_fail.cf Flakey fail (ENT-6947)" \
"./selftest/flaky_fail.cf Flakey fail (R: $_pwd/./selftest/flaky_fail.cf FLAKEY)" \
"./selftest/flaky_pass.cf Pass" \
"Passed tests:    1" \
"Failed tests:    0" \
"Skipped tests:   0" \
"Soft failures:   0" \
"Flakey failures: 2" \
"Total tests:     3"
do
  check "$regex" "$log" || errors=$((errors + 1))
done
if [ "$errors" -ne "0" ]
then
  echo "error: $errors error(s) occurred for selftest (flakey is fail)"
  echo "=== BEGIN $log ==="
  cat $log
  echo "=== END $log ==="
  clean
  exit 1
fi
