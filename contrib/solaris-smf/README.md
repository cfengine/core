Contributed by Brian Bennett <bahamat@digitalelf.net>

To use these XML definitions:

```
sudo svccfg import cf-execd.xml
sudo svccfg import cf-monitord.xml
sudo svccfg import cf-serverd.xml
```

You should manually bootstrap cf-agent first (or make that part of
your provisioning process) then stop all cfengine processes. Then use
SMF to start the services.

The default failsafe.cf (or update.cf) will start these processes
automatically. You should remove those, or make them not apply to
Solaris, if you use the SMF framework.
