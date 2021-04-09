#!/bin/sh


# because results in test.xml should not be parsed by CI or otherwise
# failures there may be expected and will be checked by grep below
clean ()
{
  rm test.xml
  rm summary.log
  rm test.log
  rm "$WORKDIR/$LOG"
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

WORKDIR=workdir
MY_XML=selftest.xml
LOG=selftest.log
mkdir -p "$WORKDIR"
touch "$WORKDIR/$MY_XML"

./testall selftest > "$WORKDIR/$LOG"
ret=$?
if [ "$ret" -ne "0" ]
then
  echo "error: exit code for selftest should be 0 but was $ret"
  clean
  exit 1
fi

errors=0
_pwd=$(pwd)

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
  check "$regex" "$WORKDIR/$LOG" || errors=$((errors + 1))
done
if [ "$errors" -ne "0" ]
then
  echo "error: $errors error(s) occurred for selftest (flakey is not fail)"
  echo "=== BEGIN $WORKDIR/$LOG ==="
  cat $WORKDIR/$LOG
  echo "=== END $WORKDIR/$LOG ==="
  clean
  exit 1
fi
FLAKEY_IS_FAIL=1 ./testall selftest > "$WORKDIR/$LOG"
ret=$?
if [ "$ret" -ne "4" ]
then
  echo "error: exit code for testall with FLAKEY_IS_FAIL=1 should be 4 but was $ret"
  clean
  exit 1
fi

errors=0

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
  check "$regex" "$WORKDIR/$LOG" || errors=$((errors + 1))
done
if [ "$errors" -ne "0" ]
then
  echo "error: $errors error(s) occurred for selftest (flakey is fail)"
  echo "=== BEGIN $WORKDIR/$LOG ==="
  cat $WORKDIR/$LOG
  echo "=== END $WORKDIR/$LOG ==="
  clean
  exit 1
fi

clean

exit 0
