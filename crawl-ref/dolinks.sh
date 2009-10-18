#!/usr/bin/env bash

if [ -f source/build.h ]; then
	true
else
	source/util/gen_ver.pl source/build.h
fi

mkdir -p NORMAL
mkdir -p WIZARD

pushd NORMAL ; mkdir -p source ; cd source ; ln -sf ../../source/* . ; popd
pushd WIZARD ; mkdir -p source ; cd source ; ln -sf ../../source/* . ; popd

if [ -d dat ]; then
	true
else
	ln -sf source/dat dat
fi
