#!/bin/sh
#jmf: automate making normal & wizard binaries, until they merge
#jmf: N.B.: requires symlink directories; run ./dolinks.sh to make

pushd NORMAL/source ; make ; popd
pushd WIZARD/source ; make wizard ; popd

if [ -f NORMAL/source/crawl ]; then
	ln -sf NORMAL/source/crawl ncrawl
fi

if [ -f WIZARD/source/crawl ]; then
	ln -sf WIZARD/source/crawl wcrawl
fi
