CALLOC_HOOK_ACTIVE=1 ./alloc_checkresult_test
if [ $? -eq 255 ]; then
  exit 0
else
  echo "Expected test to exit with code 255."
  exit 42
fi
