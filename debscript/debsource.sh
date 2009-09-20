#!/bin/bash
make
origdir=`pwd`/
version=`$origdir/weborf -v | fgrep Weborf | cut -d' ' -f2`
pwd
echo Weborf version $version

make weborf
rm -rf /tmp/weborf_$version || echo ok
mkdir /tmp/weborf_$version || echo ok
cp -vrp * /tmp/weborf_$version
echo "Copied in /tmp"

cd /tmp/weborf_$version
make clean

echo generating dsc file
$origdir/debscript/gencontrol.sh dsc > $origdir/../weborf_$version.dsc

echo creating tarball
cd /tmp
tar -cvvzf $origdir/../weborf_$version.tar.gz weborf_$version
