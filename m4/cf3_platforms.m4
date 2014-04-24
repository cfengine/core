#
# OS kernels conditionals. Don't use those unless it is really needed (if code
# depends on the *kernel* feature, and even then -- some kernel features are
# shared by different kernels).
#
# Good example: use LINUX to select code which uses inotify and netlink sockets.
# Bad example: use LINUX to select code which parses output of coreutils' ps(1).
#
AM_CONDITIONAL([LINUX], [test -n "`echo ${target_os} | grep linux`"])
AM_CONDITIONAL([MACOSX], [test -n "`echo ${target_os} | grep darwin`"])
AM_CONDITIONAL([SOLARIS], [test -n "`(echo ${target_os} | egrep 'solaris|sunos')`"])
AM_CONDITIONAL([NT], [test -n "`(echo ${target_os} | egrep 'mingw|cygwin')`"])
AM_CONDITIONAL([CYGWIN], [test -n "`(echo ${target_os} | egrep 'cygwin')`"])
AM_CONDITIONAL([AIX], [test -n "`(echo ${target_os} | grep aix)`"])
AM_CONDITIONAL([HPUX], [test -n "`(echo ${target_os} | egrep 'hpux|hp-ux')`"])
AM_CONDITIONAL([FREEBSD], [test -n "`(echo ${target_os} | grep freebsd)`"])
AM_CONDITIONAL([NETBSD], [test -n "`(echo ${target_os} | grep netbsd)`"])
AM_CONDITIONAL([XNU], [test -n "`(echo ${target_os} | grep darwin)`"])
