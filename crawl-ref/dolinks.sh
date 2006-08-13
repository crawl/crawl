#!/bin/sh

mkdir -p NORMAL
mkdir -p WIZARD

pushd NORMAL ; ln -s ../source/*.h ../source/*.cc ../source/makefile* . ; popd
pushd WIZARD ; ln -s ../source/*.h ../source/*.cc ../source/makefile* . ; popd

