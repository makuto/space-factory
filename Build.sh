#!/bin/sh

CAKELISP_DIR=Dependencies/cakelisp

# Build Cakelisp itself
echo "\n\nCakelisp\n\n"
cd $CAKELISP_DIR
./Build.sh || exit $?

cd ../..

echo "\n\nSpace Factory\n\n"

CAKELISP=./Dependencies/cakelisp/bin/cakelisp

$CAKELISP --verbose-processes --execute \
		  src/Config_Linux.cake \
		  src/SpaceFactory.cake || exit $?
