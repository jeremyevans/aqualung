#!/bin/sh

MAJOR=0
MINOR=107
MICRO=0

if [ $# -eq 0 ] ; then
    echo $MAJOR.$MINOR.$MICRO
    exit 0
fi

case $1 in

   remake) 
       sed -e "s/@MAJOR@/$MAJOR/" \
           -e "s/@MINOR@/$MINOR/" \
	   -e "s/@MICRO@/$MICRO/" < version.h.in > version.h
       ;;

   major) echo $MAJOR ;;
   minor) echo $MINOR ;;
   micro) echo $MICRO ;;

   [0-9]*)   
        major=`echo $1 | cut -d. -f 1`
        minor=`echo $1 | cut -d. -f 2`
        micro=`echo $1 | cut -d. -f 3`
        sed -e 's/^MAJOR.*/MAJOR='$major'/' \
	    -e 's/^MINOR.*/MINOR='$minor'/' \
	    -e 's/^MICRO.*/MICRO='$micro'/' < version.sh > version.sh.new && mv version.sh.new version.sh
        ;;
esac

exit 0
