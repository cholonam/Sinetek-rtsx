#!/bin/bash

# This program will overwrite the files from BSD into this repo (overwriting any
# changes!).

function my_copy() {
	DIR1="$1"
	DIR2="$2"

	echo "Copying files from '$DIR1' into '$DIR2'"
	for i in "$DIR1/"*; do
		file="${i##*/}"
		if [ -e "$DIR2/$file" ]; then
			cp "$i" "$DIR2/$file"
		fi
	done
}

echo "WARNING: This program will overwrite openbsd-files in this repo."
echo "Are you sure (type YES to continue)??"
read resp
if [ "$resp" != "YES" ]; then
	echo "Aborting..."
	exit 1
fi

DIR1="$HOME/Downloads/src-master/sys/dev/sdmmc"
DIR2="`dirname $0`/Sinetek-rtsx/3rdParty/openbsd"
my_copy "$DIR1" "$DIR2"

DIR1="$HOME/Downloads/src-master/sys/dev/ic"
my_copy "$DIR1" "$DIR2"

DIR1="$HOME/Downloads/src-master/sys/sys"
my_copy "$DIR1" "$DIR2"
