#!/bin/sh

mkdir -p NORMAL
mkdir -p WIZARD

pushd NORMAL ; ln -sf ../source . ; popd
pushd WIZARD ; ln -sf ../source . ; popd

if [ -d dat ]; then
	true
else
	ln -sf source/dat dat
fi
