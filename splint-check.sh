#!/bin/bash
splint -I. -I/usr/include -I/usr/local/include -I/usr/include/x86_64-linux-gnu/ -I../nova/src -D__gnuc_va_list=va_list -warnposix +weak $*
