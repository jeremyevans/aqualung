echo "
checking basic compilation tools ...
"

for tool in pkg-config aclocal autoheader autoconf automake gettext
do
    if which $tool >/dev/null 2>&1 ; then
	echo "	$tool: found."
    else
	echo "
You do not have $tool correctly installed. You cannot build aqualung without this tool."
	exit 1
    fi
done

aclocal
autoheader
autoconf
automake

echo "
You can now run:

    ./configure
    make
    make install

Please take the time to subscribe to the project mailing list at:
http://lists.sourceforge.net/lists/listinfo/aqualung-friends

"
