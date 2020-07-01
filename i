make CFLAGS=-DPRIVATE_DEBUG=0   # special macro def for needed logging at warning level to debug report_level at promise level
cf-agent/cf-agent "$@" -Kf ./test.cf | tee log
#cf-agent/cf-agent --verbose -f ./test.cf | tee log
