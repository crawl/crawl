#!/bin/sh
#jmf: automate making normal & wizard binaries, until they merge
#jmf: N.B.: requires symlink directories; run ./dolinks.sh to make

pushd NORMAL && make && popd && pushd WIZARD && make wizard ; popd

if [ -f NORMAL/crawl ]; then
	ln -sf NORMAL/crawl ncrawl
fi
if [ -f WIZARD/crawl ]; then
	ln -sf WIZARD/crawl wcrawl
fi
