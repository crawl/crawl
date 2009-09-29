#!/usr/bin/env bash
#jmf: automate making normal & wizard binaries, until they merge
#jmf: N.B.: requires symlink directories; run ./dolinks.sh to make

MAKE=make
if [ "$(uname -s)" == "FreeBSD" ]; then
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
