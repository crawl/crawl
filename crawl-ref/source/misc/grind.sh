#!/bin/sh

# Convenience caller for the valgrind memory debugger

nice -n 7 valgrind --tool=memcheck --leak-check=full --log-file=grind.log \
	--suppressions=misc/valgrind-suppress.txt ./crawl "$@"
