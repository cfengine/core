# systemd service files for CFEngine

On systems running systemd as their init process, the cfengine agents should be
controlled via "systemctl start" and "systemctl stop".  This will put cfengine
into separate cgroups and handle logging appropriately.
