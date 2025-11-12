#!/bin/bash
# ***********************************************************************
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# ***********************************************************************

usage() {
    echo "Usage: $0 [ -h | --help ] [ -t | --type <build_type> ]"
    echo "build_type: [debug, release, clean]"
    echo "Examples:"
    echo "1) ./build.sh -t debug  2) ./build.sh -t release 3) ./build.sh -t clean"
    exit 1;
}

BUILD_TYPE=Debug
BUILD_TOOL=make

while true; do
    para1="${1:-default}"
    case "$para1" in
        -t | --type )
            type="${2:-default}"
            type=${type,,}
            [[ "$type" != "debug" && "$type" != "release" && "$type" != "clean" ]] && echo "Invalid build type $2" && usage
            if [[ "$type" == 'debug' ]]; then
              BUILD_TYPE=Debug
            elif [[ "$type" == 'release' ]]; then
              BUILD_TYPE=Release
            elif [[ "$type" == 'clean' ]]; then
              BUILD_TYPE=CLEAN
            fi
            if [[ $# -lt 2 ]]; then
              break
            fi
            shift 2
            ;;
        -h | --help )
            usage
            exit 0
            ;;
        * )
            break;;
    esac
done

PROJ_DIR="$(dirname "${BASH_SOURCE[0]}")"
PROJ_DIR="$(realpath "${PROJ_DIR}")"

if [ "$BUILD_TYPE" == "CLEAN" ]; then
    rm -rf $PROJ_DIR/build && rm -rf $PROJ_DIR/output
    exit 0
fi

LIB_BOUNDS_CHECK_FILE=/usr/lib64/libboundscheck.so
if [[ ! -f "$LIB_BOUNDS_CHECK_FILE " ]]
then
    git submodule update --init --recursive
    cd ${PROJ_DIR}/3rdparty/secure/libboundscheck
    make CC=gcc
    cp -f lib/libboundscheck.so /usr/lib64/
    cd -
fi

BUILD_DIR=$PROJ_DIR/build
if [[ ! -d "$BUILD_DIR" ]]
then
  mkdir -p $BUILD_DIR
fi

cd $BUILD_DIR || {
  echo "Fatal! Cannot enter $BUILD_DIR."
  exit 1;
}

N_CPUS=$(grep processor /proc/cpuinfo | wc -l)
echo "$N_CPUS processors detected."

CMAKE_CMD="cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE $PROJ_DIR"
BUILD_CMD="$BUILD_TOOL -j $((N_CPUS-2)) install"

echo $CMAKE_CMD
$CMAKE_CMD || {
    echo "Failed to configure SMAP build!"
    exit 1
}
echo
echo "Done configuring SMAP build"
echo
echo $BUILD_CMD
$BUILD_CMD || {
    echo "Failed to build SMAP"
    exit 1
}
echo
echo Success