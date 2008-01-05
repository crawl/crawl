#!/bin/sh

mkdir -p NORMAL
mkdir -p WIZARD

pushd NORMAL ; ln -sf ../source/rltiles . ; ln -sf ../source/util . ; ln -sf ../source/*.h ../source/*.cc ../source/makefile* . ; popd
pushd WIZARD ; ln -sf ../source/rltiles . ; ln -sf ../source/util . ; ln -sf ../source/*.h ../source/*.cc ../source/makefile* . ; popd

if [ -L tiles ]; then
	true
else
	ln -sf source/rltiles tiles
fi

if [ -L dat ]; then
	true
else
	ln -sf source/dat dat
fi
