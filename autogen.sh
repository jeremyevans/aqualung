#!/bin/sh

echo "
checking basic configuration tools ...
"

for tool in autoconf autoreconf automake autopoint
do
    if command -v $tool >/dev/null 2>&1
    then
	echo "$tool ... found."
    else
	echo "$tool ... not found.

*** You do not have $tool correctly installed. You cannot build aqualung without this tool."
	exit 1
    fi
done

echo

if autoreconf -i "$@"
then
    echo "
You can now run:

    ./configure
    make
    make install

"
fi
