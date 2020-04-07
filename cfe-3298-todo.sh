find -name '*.c' | xargs grep -sn EvalContextVariableGet | grep -v _test.c | grep -v "CRAIG"
find -name '*.c' | xargs grep -sn ExpandScalar | grep -v _test.c | grep -v "CRAIG"
