#!/bin/sh
# Author: Mike Weilgart, 19 September 2016
# Updated by: Aleksey Tsalolikhin, 30 Aug 2018
echo
/var/cfengine/bin/cf-agent -V
echo
ps -ef | grep 'cf-[a-z]*d' || echo "No CFEngine daemons running"
if [ -f /var/cfengine/inputs/policy_version.dat ]; then
  echo
  printf 'Policy version: '
  cat /var/cfengine/inputs/policy_version.dat
fi
if [ -f /var/cfengine/inputs/policy_commit_id.dat ]; then
  echo
  printf 'Policy commit id: '
  cat /var/cfengine/inputs/policy_commit_id.dat
fi
if [ -f /var/cfengine/policy_server.dat ]; then
  echo
  printf "Bootstrapped to: %s\n" "$(cat /var/cfengine/policy_server.dat)"
else
  echo
  echo "Not bootstrapped to a policy server"
fi
echo
printf 'Policy channel assignment: %s\n' "$(cat /var/cfengine/state/policy_channel.txt 2>/dev/null || echo Not assigned)"
echo
echo "Host key:"
/var/cfengine/bin/cf-key -p /var/cfengine/ppkeys/localhost.pub
echo
awk -F '[:,]' -v time="$(date +%s)" 'END {printf "Last cf-agent run started %d minutes ago and lasted %d seconds\n", (time - $1)/60, $2 - $1}' /var/cfengine/promise_summary.log
echo
tail -n 2 /var/cfengine/promise_summary.log | sed 's/.*Total promise compliance: \([^.]*\).*/\1/;1s/^/update.cf compliance:   /;2s/^/promises.cf compliance: /'
echo
touch /var/cfengine/test.tmp && ( echo Filesystem writeable ; \rm /var/cfengine/test.tmp)
echo
df -P /var/cfengine | awk 'END {printf "Filesystem utilization: %s %s\n", $6, $5}'
echo
pgrep -fl cf-twin && printf "\ncf-twin process is running. Check for failed CFEngine upgrade (if found wipe and reinstall).\n\n"
