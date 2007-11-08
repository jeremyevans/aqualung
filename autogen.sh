#!/bin/sh

echo "
checking basic compilation tools ...
"

for tool in pkg-config aclocal autoheader autoconf automake gettext msgfmt
do
    echo -n "$tool ... "
    if which $tool >/dev/null 2>&1 ; then
	echo "found."
    else
	echo "not found.

*** You do not have $tool correctly installed. You cannot build aqualung without this tool."
	exit 1
    fi
done

echo

for tool in aclocal autoheader autoconf automake
do
    echo -n "running $tool ... "
    if $tool; then
	echo "done."
    else
	echo "failed.

*** $tool returned an error."
	exit 1
    fi
done


echo "
You can now run:

    ./configure
    make
    make install

Please take the time to subscribe to the project mailing list at:
http://lists.sourceforge.net/lists/listinfo/aqualung-friends

"
