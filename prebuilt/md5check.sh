#! /bin/bash
# Changes to directory listed in first arguement and then 
# checks the sum against the list in the second arguement
# Created by ViViDboarder #TeamWin for checksum verification

ZIPDIR=$(dirname $1)
ZIPNAME=$(basename $1)
MD5NAME="$ZIPNAME.md5"

cd $ZIPDIR

if [ -f $MD5NAME ]; then
    ZIPNAMEF=$(awk '{print $2}' $MD5NAME)
    if [ $ZIPNAME == $ZIPNAMEF ]; then
        if [ -f $ZIPNAME ]; then
            md5sum -c $MD5NAME | awk '{print $2}'
        else
            echo "NOZ"
        fi
    else
        echo "DF"
    fi
else
    echo "NO5"
fi

