#!/usr/bin/env bash

dolinks() {
    mkdir -p source
    cd source
    ln -sf ../../source/*.{cc,h} .
    ln -sf ../../source/makefile* .
    ln -sf ../../source/{util,contrib,rltiles,prebuilt,dat} .
}

mkdir -p NORMAL
mkdir -p WIZARD

pushd NORMAL ; dolinks ; popd
pushd WIZARD ; dolinks ; popd

if [ -d dat ]; then
	true
else
	ln -sf source/dat dat
fi
