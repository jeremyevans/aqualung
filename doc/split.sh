#!/bin/sh

OUTPUT='/dev/null'

while read line; do

    if [ "$line" = "&&" ] ; then
	read OUTPUT;
	echo '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">' >> $OUTPUT

    else
	echo "$line" >> $OUTPUT
    fi

done
