#!/bin/sh

mkdir -p NORMAL
mkdir -p WIZARD

pushd NORMAL ; ln -s ../source/util . ; ln -s ../source/*.h ../source/*.cc ../source/makefile* . ; popd
pushd WIZARD ; ln -s ../source/util . ; ln -s ../source/*.h ../source/*.cc ../source/makefile* . ; popd

ln -s source/dat dat
