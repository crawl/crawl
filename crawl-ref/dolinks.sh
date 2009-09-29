#!/usr/bin/env bash

mkdir -p NORMAL
mkdir -p WIZARD

pushd NORMAL ; mkdir -p source ; cd source ; ln -sf ../../source/* . ; popd
pushd WIZARD ; mkdir -p source ; cd source ; ln -sf ../../source/* . ; popd

if [ -d dat ]; then
	true
else
	ln -sf source/dat dat
fi
