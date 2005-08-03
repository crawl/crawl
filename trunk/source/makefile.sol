# Make file for Dungeon Crawl (solaris)

#
# Modified for Crawl Reference by $Author$ on $Date$
#

APPNAME = crawl

# this file contains a list of the libraries.
# it will make a variable called OBJECTS that contains all the libraries
include makefile.obj

OBJECTS += libunix.o

CXX = g++
DELETE = rm -f
COPY = cp
GROUP = games
MOVE = mv
OS_TYPE = SOLARIS

CFLAGS = -Wall -Wwrite-strings -Wstrict-prototypes \
            -Wmissing-prototypes -Wmissing-declarations \
            -g -D$(OS_TYPE) $(EXTRA_FLAGS)

LDFLAGS = -static
MCHMOD = 2755
INSTALLDIR = /opt/local/newcrawl/bin
LIB = -lcurses

all:            $(APPNAME)

install:        $(APPNAME)
		#strip $(APPNAME)
		$(MOVE) ${INSTALLDIR}/${APPNAME} ${INSTALLDIR}/${APPNAME}.old
		$(COPY) $(APPNAME) ${INSTALLDIR}
		chgrp ${GROUP} ${INSTALLDIR}/${APPNAME}
		chmod ${MCHMOD} ${INSTALLDIR}/$(APPNAME)

clean:
		$(DELETE) *.o

distclean:
		$(DELETE) *.o 
		$(DELETE) bones.*
		$(DELETE) morgue.txt
		$(DELETE) scores 
		$(DELETE) $(APPNAME) 
		$(DELETE) *.sav
		$(DELETE) core
		$(DELETE) *.0*
		$(DELETE) *.lab


$(APPNAME):	$(OBJECTS)
		${CXX} ${LDFLAGS} $(INCLUDES) $(CFLAGS) $(OBJECTS) -o $(APPNAME) $(LIB)

debug:		$(OBJECTS)
		${CXX} ${LDFLAGS} $(INCLUDES) $(CFLAGS) $(OBJECTS) -o $(APPNAME) $(LIB)

profile:	$(OBJECTS)
		${CXX} -g -p ${LDFLAGS} $(INCLUDES) $(CFLAGS) $(OBJECTS) -o $(APPNAME) $(LIB)

.cc.o:
		${CXX} ${CFLAGS} -c $< ${INCLUDE}

.h.cc:
		touch $@
