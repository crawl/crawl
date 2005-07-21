#Makefile chooser.  Choose one:

MAKEFILE = makefile.lnx
#MAKEFILE = makefile.sgi
#MAKEFILE = makefile.dos
#MAKEFILE = makefile.emx
#MAKEFILE = makefile.sol

#jmf: number of concurrent jobs -- good value is (#_of_CPUs * 2) + 1
#     cuts build time a lot on multi-cpu machines
OTHER=-j2

all:
	$(MAKE) $(OTHER) -f $(MAKEFILE) EXTRA_FLAGS='-O2 -fno-strength-reduce'

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
	$(MAKE) $(OTHER) -f $(MAKEFILE) debug EXTRA_FLAGS='-g -DFULLDEBUG -DWIZARD'

# DO NOT DELETE THIS LINE -- $(MAKE) depend depends on it.


