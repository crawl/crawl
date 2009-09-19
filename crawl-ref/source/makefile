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

# number of concurrent jobs -- good value is (#_of_CPUs * 2) + 1
#     cuts build time a lot on multi-cpu machines
# Note: do not use enable this for builds of the tiles version.
#OTHER=-j2

all:
	$(MAKE) $(OTHER) -f $(MAKEFILE)

profile:
	$(MAKE) $(OTHER) -f $(MAKEFILE) EXTRA_FLAGS='-pg'

noopt:
	$(MAKE) $(OTHER) -f $(MAKEFILE)

install:
	$(MAKE) $(OTHER) -f $(MAKEFILE) install

clean:
	$(MAKE) $(OTHER) -f $(MAKEFILE) clean

distclean:
	$(MAKE) $(OTHER) -f $(MAKEFILE) distclean

depend:
	$(MAKE) $(OTHER) -f $(MAKEFILE) depend

# WIZARD mode currently includes asserts, bounds checking, item checking and
# monster checking.
wizard:
	$(MAKE) $(OTHER) -f $(MAKEFILE) debug EXTRA_FLAGS='-g -DWIZARD -DDEBUG -DDEBUG_ITEM_SCAN -DDEBUG_MONS_SCAN'

# DEBUG mode includes WIZARD mode as well as copious debugging input
debug:
	$(MAKE) $(OTHER) -f $(MAKEFILE) DEBUG_CRAWL=y debug EXTRA_FLAGS='-g -DFULLDEBUG -DWIZARD'

# [dshaligram] The individual makefile need not necessarily support
# package-source; only makefile.unix does at the moment.
package-source:
	$(MAKE) -f $(MAKEFILE) package-source

prebuildyacc:
	$(MAKE) -f $(MAKEFILE) prebuildyacc

# DO NOT DELETE THIS LINE -- $(MAKE) depend depends on it.


