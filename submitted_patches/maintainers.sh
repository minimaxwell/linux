#!/bin/sh
PATH=`pwd`
echo $PWD
cd ../linux
scripts/get_maintainer.pl $PATH/$1
