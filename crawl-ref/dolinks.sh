#!/bin/sh

mkdir -p NORMAL
mkdir -p WIZARD

pushd NORMAL ; ln -sf ../source/util . ; ln -sf ../source/*.h ../source/*.cc ../source/makefile* . ; popd
pushd WIZARD ; ln -sf ../source/util . ; ln -sf ../source/*.h ../source/*.cc ../source/makefile* . ; popd

if [ -d dat ]; then
	true
else
	ln -sf source/dat dat
fi
