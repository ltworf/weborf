#!/bin/bash
make
origdir=`pwd`/
version=`$origdir/weborf -v | fgrep Weborf | cut -d' ' -f2`
export WEBORF_VERSION=$version

pwd
echo Weborf version $version

make weborf
rm -rf /tmp/weborf_$version || echo ok
mkdir /tmp/weborf_$version || echo ok
cp -vrp * /tmp/weborf_$version
echo "Copied in /tmp"

cd /tmp/weborf_$version
make clean

echo creating tarball
cd /tmp
tar -cvvzf $origdir/../weborf_$version.tar.gz weborf_$version

echo generating dsc file
$origdir/debscript/gencontrol.sh dsc > $origdir/../weborf_$version.dsc

echo appending hashes
echo "Files:" >> $origdir/../weborf_$version.dsc

md5=`md5sum $origdir/../weborf_$version.tar.gz | cut -d' ' -f1`
size=`ls -l $origdir/../weborf_$version.tar.gz  | cut -d' ' -f5`
filename=weborf_$version.tar.gz
echo " $md5 $size $filename">> $origdir/../weborf_$version.dsc
echo "">> $origdir/../weborf_$version.dsc

echo "sign dsc file"
#debsign $origdir/../weborf_$version.dsc
gpg --clearsign -s $origdir/../weborf_$version.dsc
mv $origdir/../weborf_$version.dsc.asc $origdir/../weborf_$version.dsc
