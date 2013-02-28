#!/bin/sh
ntpdate jupiter1.si.c-s.fr
if [ $? -eq 0 ] ; then
	cat $1 | sendmail -t -fchristophe.leroy@c-s.fr
fi
