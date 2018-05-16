set breakpoint pending on
b unix_iface.c:349
commands
  set fd=-1
  c
end
run
quit $_exitcode
