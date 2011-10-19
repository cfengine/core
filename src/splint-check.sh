#!/bin/bash
splint -I. -I/usr/include -I/usr/include/x86_64-linux-gnu/ -D__gnuc_va_list=va_list -UXEN_CPUID_SUPPORT -warnposix +weak $*