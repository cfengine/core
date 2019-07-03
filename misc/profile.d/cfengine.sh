if ! echo ${PATH} | /bin/grep /var/cfengine/bin > /dev/null ; then
    export PATH=$PATH:/var/cfengine/bin
fi

if ! echo ${MANPATH} | /bin/grep /var/cfengine/share/man > /dev/null ; then
    export MANPATH=$MANPATH:/var/cfengine/share/man
fi
