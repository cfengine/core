#!/bin/sh

# Unfortunately splint is crashing in system libraries, so it's not that useful

# Other possibly useful flags:
#   +weak -warnposixheaders -unrecog

splint  \
    +posixstrictlib +unixlib +gnuextensions  \
    -I/usr/include/x86_64-linux-gnu/ -Ilibutils/ \
    -DHAVE_CONFIG_H -D__linux__ -D__gnuc_va_list=va_list \
    libutils/*.[ch]
