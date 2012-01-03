#!/bin/sh

echo "
checking basic configuration tools ...
"

for tool in autoconf autoreconf automake
do
    echo -n "$tool ... "
    if command -v $tool >/dev/null 2>&1
    then
	echo "found."
    else
	echo "not found.

*** You do not have $tool correctly installed. You cannot build aqualung without this tool."
	exit 1
    fi
done

echo

if autoreconf -i
then
    echo "
You can now run:

    ./configure
    make
    make install

Please take the time to subscribe to the project mailing list at:
http://lists.sourceforge.net/lists/listinfo/aqualung-friends

"
fi
