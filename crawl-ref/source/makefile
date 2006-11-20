#
# Modified for Crawl Reference by $Author$ on $Date$
#

#Makefile chooser.  Choose one:

MAKEFILE = makefile.unix
#MAKEFILE = makefile.dos
#MAKEFILE = makefile.osx
#MAKEFILE = makefile.mgw

#jmf: number of concurrent jobs -- good value is (#_of_CPUs * 2) + 1
#     cuts build time a lot on multi-cpu machines
#OTHER=-j2

all:
	$(MAKE) $(OTHER) -f $(MAKEFILE) EXTRA_FLAGS='-O3'

noopt:
	$(MAKE) $(OTHER) -f $(MAKEFILE)

install:
	$(MAKE) $(OTHER) -f $(MAKEFILE) install

clean:
	$(MAKE) $(OTHER) -f $(MAKEFILE) clean

distclean:
	$(MAKE) $(OTHER) -f $(MAKEFILE) distclean

# WIZARD mode currently includes asserts, bounds checking, and item checking
wizard:
	$(MAKE) $(OTHER) -f $(MAKEFILE) debug EXTRA_FLAGS='-g -DWIZARD -DDEBUG -DDEBUG_ITEM_SCAN'

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


