#!/usr/bin/env bash
#jmf: automate making normal & wizard binaries, until they merge
#jmf: N.B.: requires symlink directories; run ./dolinks.sh to make

MAKE=make
make --version &> /dev/null
if [ $? != 0 ]; then
	# BSD make returns an error code if you try '--version'. GNU make doesn't.
	MAKE=gmake
fi

pushd NORMAL/source ; $MAKE ; popd
pushd WIZARD/source ; $MAKE wizard ; popd

if [ -f NORMAL/source/crawl ]; then
	ln -sf NORMAL/source/crawl ncrawl
fi

if [ -f WIZARD/source/crawl ]; then
	ln -sf WIZARD/source/crawl wcrawl
fi
