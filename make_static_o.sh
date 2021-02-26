#!/bin/bash
#
# 因為無法指定 .a 檔案的 export symbols; (我不知到有什麼更簡單的方法).
# 所以用底下的方法, 建立一個特殊的 .o 檔案, 用來取代 .a;
# 執行時必須提供3個參數:
#  dst.o  src.a  建立so時的version-script
#

set -e
dst=$1
src=$2
expsrc=$3

echo Building [$dst] for static link.

tmpdir=$dst.tmpd
rm -rf  $tmpdir
mkdir   $tmpdir
cp $src $tmpdir/tmp.a
pushd $tmpdir >/dev/null
ar x     tmp.a
ld -r -o tmp.ro *.o
popd          >/dev/null

sed 's/[ {}]//g' $expsrc |sed 's/;/\n/g' >$tmpdir/exports.txt
objcopy $tmpdir/tmp.ro $dst --keep-global-symbols=$tmpdir/exports.txt
strip -x $dst

rm -rf $tmpdir

echo [$dst] is ready for static link.
