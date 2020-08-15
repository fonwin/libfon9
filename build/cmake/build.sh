#!/bin/sh
#
# fon9/build/cmake/build.sh
#

fon9path=`pwd`"$0"
fon9path="${fon9path%/*}"
fon9path="${fon9path%/*}"
fon9path="${fon9path%/*}"
develpath="${fon9path%/*}"

set -x

SOURCE_DIR=${fon9path}
BUILD_DIR=${BUILD_DIR:-${develpath}/output/fon9}
BUILD_TYPE=${BUILD_TYPE:-release}
INSTALL_DIR=${INSTALL_DIR:-${develpath}/${BUILD_TYPE}}
FON9_BUILD_UINT_TEST=${FON9_BUILD_UINT_TEST:-1}

mkdir -p $BUILD_DIR/$BUILD_TYPE \
  && cd $BUILD_DIR/$BUILD_TYPE \
  && cmake \
           -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
           -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
           -DCMAKE_FON9_BUILD_UINT_TEST=$FON9_BUILD_UINT_TEST \
           $SOURCE_DIR \
  && make $*
