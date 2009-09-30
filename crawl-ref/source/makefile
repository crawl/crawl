#
# Modified for Crawl Reference by $Author$ on $Date$
#

#Makefile chooser.  Choose one:

MAKEFILE ?= makefile.unix
#MAKEFILE ?= makefile.dos
#MAKEFILE ?= makefile.osx
#MAKEFILE ?= makefile.mgw
#MAKEFILE ?= makefile_tiles.mgw
#MAKEFILE ?= makefile_tiles.unix

#These are for cross-compiling purposes on crawl.develz.org, here for convenience:

#MAKEFILE ?= makefile.mgw_unix
#MAKEFILE ?= makefile_tiles.mgw_unix

all:
	+$(MAKE) -f $(MAKEFILE)

profile:
	+$(MAKE) -f $(MAKEFILE) EXTRA_FLAGS='-pg'

noopt:
	+$(MAKE) -f $(MAKEFILE)

install:
	+$(MAKE) -f $(MAKEFILE) install

clean:
	+$(MAKE) -f $(MAKEFILE) clean

distclean:
	+$(MAKE) -f $(MAKEFILE) distclean

depend:
	+$(MAKE) -f $(MAKEFILE) depend

# WIZARD mode currently includes asserts, bounds checking, item checking and
# monster checking.
wizard:
	+$(MAKE) -f $(MAKEFILE) debug EXTRA_FLAGS='-g -DWIZARD -DDEBUG -DDEBUG_ITEM_SCAN -DDEBUG_MONS_SCAN'

# DEBUG mode includes WIZARD mode as well as copious debugging input
debug:
	+$(MAKE) -f $(MAKEFILE) DEBUG_CRAWL=y debug EXTRA_FLAGS='-g -DFULLDEBUG -DWIZARD'

# [dshaligram] The individual makefile need not necessarily support
# package-source; only makefile.unix does at the moment.
package-source:
	+$(MAKE) -f $(MAKEFILE) package-source

prebuildyacc:
	+$(MAKE) -f $(MAKEFILE) prebuildyacc

# DO NOT DELETE THIS LINE -- $(MAKE) depend depends on it.


