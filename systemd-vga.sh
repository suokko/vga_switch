#!/bin/sh

PATH=/sbin:/usr/sbin:/bin:/usr/bin

/sbin/vga_switch -s ${1}
exit $?
