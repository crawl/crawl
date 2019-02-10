#!/bin/bash

set -eu
set -o pipefail

if [[ $(uname) != Darwin ]] ; then
    echo "error: This script only works on OSX for now." >&2
    exit 1
fi

NPROC=$(sysctl -n hw.ncpu)
QW_RC="../../../qw/qw.rc" # separate checkout
QW_START_SEED=1 # What seed should qw start at. Use $RANDOM
QW_NUM_RUNS=1 # How many runs should qw complete
CLEANUP_TEMP_DIRS="true" # "" disables cleanup, anything else enables it

######################
# Make sure we're running niced
renice -n 10 $$

######################
echo '=> Build the instrumented binary'
make -j "$NPROC" PGO_GENERATE=y

######################
echo '=> Generate some profile data'

CRAWL_TEMP_DIR=$(mktemp -d)
LLVM_TEMP_DIR=$(mktemp -d)

function cleanup() {
  echo '=> Cleaning up temporary directories'
  rm -rf "$CRAWL_TEMP_DIR"
  rm -rf "$LLVM_TEMP_DIR"
}
if [[ -n $CLEANUP_TEMP_DIRS ]] ; then
  trap cleanup EXIT
fi

export LLVM_PROFILE_FILE="$LLVM_TEMP_DIR/%m-%p.profraw"

echo '===> qw'
# TODO parallel processes
# This doesn't work though, the crawl processes hang when qw dies
# seq $START_SEED $((START_SEED+10)) | LLVM_PROFILE_FILE="%m-%p.profraw" xargs -t -P$NPROC -n 1 -I {} script -qr qw{}.ttyrec ./crawl -rc "$QW_RC" -seed {} -name qw{} >/dev/null
for i in $(seq 1 "$QW_NUM_RUNS") ; do
  seed=$((QW_START_SEED + i - 1))
  echo "=====> run $i/$QW_NUM_RUNS"
  time script -qr /dev/null ./crawl -rc "$QW_RC" -seed "$seed" >/dev/null
done

echo '===> stress tests (to be implemented)'

######################
echo '=> Process raw profile data'
PROFDATA_PATH="$LLVM_TEMP_DIR/crawl.profdata"
llvm-profdata merge -output="$PROFDATA_PATH" "$LLVM_TEMP_DIR"/*.profraw

######################
echo '=> Build the PGO-optimised binary'
CCACHE_DISABLE=1 make -j "$NPROC" "PGO_USE=$PROFDATA_PATH"

######################
if [[ -n $CLEANUP_TEMP_DIRS ]] ; then
  cleanup
  trap - EXIT
fi
