#!/bin/bash

readonly MY_DIR=$(dirname "$0")

if [[ -d "$MY_DIR/test/Sinetek-rtsx.kext" ]]; then
	echo "Deleting kext"
	sudo rm -rf "$MY_DIR/test/Sinetek-rtsx.kext" || echo "=> Failed!"
fi
if [[ -d "$MY_DIR/DerivedData" ]]; then
	echo "Deleting DerivedData"
	sudo rm -rf "$MY_DIR/DerivedData" || echo "=> Failed!"
fi
