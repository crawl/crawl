# Make file for Dungeon Crawl (solaris)

APPNAME = crawl

# this file contains a list of the libraries.
# it will make a variable called OBJECTS that contains all the libraries
include makefile.obj

OBJECTS += liblinux.o

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
# DO NOT DELETE THIS LINE -- make depend depends on it.


abl-show.o: abl-show.h AppHdr.h beam.h debug.h defines.h direct.h effects.h
abl-show.o: enum.h externs.h FixAry.h FixVec.h food.h it_use2.h liblinux.h
abl-show.o: libutil.h message.h misc.h monplace.h player.h religion.h
abl-show.o: skills2.h skills.h spells1.h spells2.h spells3.h spells4.h
abl-show.o: spl-cast.h stuff.h transfor.h /usr/include/alloca.h
abl-show.o: /usr/include/asm/errno.h /usr/include/assert.h
abl-show.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
abl-show.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
abl-show.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
abl-show.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
abl-show.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
abl-show.o: /usr/include/bits/sched.h /usr/include/bits/select.h
abl-show.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
abl-show.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
abl-show.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
abl-show.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
abl-show.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
abl-show.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
abl-show.o: /usr/include/fcntl.h /usr/include/features.h
abl-show.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
abl-show.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
abl-show.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
abl-show.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
abl-show.o: /usr/include/g++-3/std/bastring.cc
abl-show.o: /usr/include/g++-3/std/bastring.h
abl-show.o: /usr/include/g++-3/std/straits.h
abl-show.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
abl-show.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
abl-show.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
abl-show.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
abl-show.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
abl-show.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
abl-show.o: /usr/include/g++-3/stl_uninitialized.h
abl-show.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
abl-show.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
abl-show.o: /usr/include/_G_config.h /usr/include/gconv.h
abl-show.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
abl-show.o: /usr/include/libio.h /usr/include/limits.h
abl-show.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
abl-show.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
abl-show.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
abl-show.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
abl-show.o: /usr/include/sys/types.h /usr/include/time.h
abl-show.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
abl-show.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
abl-show.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
abl-show.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
abl-show.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
abl-show.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
abl-show.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
abl-show.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
abl-show.o: view.h
abyss.o: abyss.h AppHdr.h cloud.h debug.h defines.h dungeon.h enum.h
abyss.o: externs.h FixAry.h FixVec.h items.h lev-pand.h liblinux.h libutil.h
abyss.o: message.h monplace.h randart.h stuff.h /usr/include/alloca.h
abyss.o: /usr/include/asm/errno.h /usr/include/assert.h
abyss.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
abyss.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
abyss.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
abyss.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
abyss.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
abyss.o: /usr/include/bits/sched.h /usr/include/bits/select.h
abyss.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
abyss.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
abyss.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
abyss.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
abyss.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
abyss.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
abyss.o: /usr/include/fcntl.h /usr/include/features.h
abyss.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
abyss.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
abyss.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
abyss.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
abyss.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
abyss.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
abyss.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
abyss.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
abyss.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
abyss.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
abyss.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
abyss.o: /usr/include/g++-3/stl_relops.h
abyss.o: /usr/include/g++-3/stl_uninitialized.h
abyss.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
abyss.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
abyss.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
abyss.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
abyss.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
abyss.o: /usr/include/stdlib.h /usr/include/string.h /usr/include/sys/cdefs.h
abyss.o: /usr/include/sys/select.h /usr/include/sys/stat.h
abyss.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
abyss.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
abyss.o: /usr/include/xlocale.h
abyss.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
abyss.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
abyss.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
abyss.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
abyss.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
abyss.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
abyss.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
acr.o: abl-show.h abyss.h AppHdr.h chardump.h command.h debug.h defines.h
acr.o: delay.h describe.h direct.h effects.h enum.h externs.h fight.h files.h
acr.o: FixAry.h FixVec.h food.h hiscores.h initfile.h invent.h itemname.h
acr.o: items.h item_use.h it_use2.h it_use3.h lev-pand.h liblinux.h libutil.h
acr.o: message.h misc.h monplace.h monstuff.h mon-util.h mutation.h newgame.h
acr.o: ouch.h output.h overmap.h player.h randart.h religion.h skills2.h
acr.o: skills.h spells1.h spells3.h spells4.h spl-book.h spl-cast.h
acr.o: spl-util.h stuff.h tags.h transfor.h /usr/include/alloca.h
acr.o: /usr/include/asm/errno.h /usr/include/asm/sigcontext.h
acr.o: /usr/include/assert.h /usr/include/bits/confname.h
acr.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
acr.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
acr.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
acr.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
acr.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
acr.o: /usr/include/bits/select.h /usr/include/bits/sigaction.h
acr.o: /usr/include/bits/sigcontext.h /usr/include/bits/siginfo.h
acr.o: /usr/include/bits/signum.h /usr/include/bits/sigset.h
acr.o: /usr/include/bits/sigstack.h /usr/include/bits/sigthread.h
acr.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
acr.o: /usr/include/bits/time.h /usr/include/bits/types.h
acr.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
acr.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
acr.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
acr.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
acr.o: /usr/include/features.h /usr/include/g++-3/alloc.h
acr.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
acr.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
acr.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
acr.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
acr.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
acr.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
acr.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
acr.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
acr.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
acr.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
acr.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
acr.o: /usr/include/g++-3/stl_uninitialized.h /usr/include/g++-3/stl_vector.h
acr.o: /usr/include/g++-3/streambuf.h /usr/include/g++-3/string
acr.o: /usr/include/g++-3/type_traits.h /usr/include/_G_config.h
acr.o: /usr/include/gconv.h /usr/include/getopt.h /usr/include/gnu/stubs.h
acr.o: /usr/include/libio.h /usr/include/limits.h /usr/include/linux/errno.h
acr.o: /usr/include/linux/limits.h /usr/include/signal.h /usr/include/stdio.h
acr.o: /usr/include/stdlib.h /usr/include/string.h /usr/include/sys/cdefs.h
acr.o: /usr/include/sys/select.h /usr/include/sys/stat.h
acr.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
acr.o: /usr/include/sys/ucontext.h /usr/include/time.h
acr.o: /usr/include/ucontext.h /usr/include/unistd.h /usr/include/wchar.h
acr.o: /usr/include/xlocale.h
acr.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
acr.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/float.h
acr.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
acr.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
acr.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
acr.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
acr.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
acr.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
acr.o: wpn-misc.h
beam.o: AppHdr.h beam.h cloud.h debug.h defines.h direct.h effects.h enum.h
beam.o: externs.h fight.h FixAry.h FixVec.h itemname.h items.h it_use2.h
beam.o: liblinux.h libutil.h message.h misc.h monplace.h monstuff.h
beam.o: mon-util.h mstuff2.h ouch.h player.h religion.h skills.h spells1.h
beam.o: spells3.h spells4.h stuff.h /usr/include/alloca.h
beam.o: /usr/include/asm/errno.h /usr/include/assert.h
beam.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
beam.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
beam.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
beam.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
beam.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
beam.o: /usr/include/bits/sched.h /usr/include/bits/select.h
beam.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
beam.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
beam.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
beam.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
beam.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
beam.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
beam.o: /usr/include/fcntl.h /usr/include/features.h
beam.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
beam.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
beam.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
beam.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
beam.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
beam.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
beam.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
beam.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
beam.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
beam.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
beam.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
beam.o: /usr/include/g++-3/stl_relops.h
beam.o: /usr/include/g++-3/stl_uninitialized.h
beam.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
beam.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
beam.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
beam.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
beam.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
beam.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
beam.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
beam.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
beam.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
beam.o: /usr/include/wchar.h /usr/include/xlocale.h
beam.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
beam.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
beam.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
beam.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
beam.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
beam.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
beam.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
chardump.o: AppHdr.h chardump.h debug.h defines.h describe.h enum.h externs.h
chardump.o: FixAry.h FixVec.h itemname.h items.h liblinux.h libutil.h
chardump.o: message.h mutation.h player.h religion.h shopping.h skills2.h
chardump.o: spl-book.h spl-cast.h spl-util.h stuff.h /usr/include/alloca.h
chardump.o: /usr/include/asm/errno.h /usr/include/assert.h
chardump.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
chardump.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
chardump.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
chardump.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
chardump.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
chardump.o: /usr/include/bits/sched.h /usr/include/bits/select.h
chardump.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
chardump.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
chardump.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
chardump.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
chardump.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
chardump.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
chardump.o: /usr/include/fcntl.h /usr/include/features.h
chardump.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
chardump.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
chardump.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
chardump.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
chardump.o: /usr/include/g++-3/std/bastring.cc
chardump.o: /usr/include/g++-3/std/bastring.h
chardump.o: /usr/include/g++-3/std/straits.h
chardump.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
chardump.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
chardump.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
chardump.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
chardump.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
chardump.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
chardump.o: /usr/include/g++-3/stl_uninitialized.h
chardump.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
chardump.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
chardump.o: /usr/include/_G_config.h /usr/include/gconv.h
chardump.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
chardump.o: /usr/include/libio.h /usr/include/limits.h
chardump.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
chardump.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
chardump.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
chardump.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
chardump.o: /usr/include/sys/types.h /usr/include/time.h
chardump.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
chardump.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
chardump.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
chardump.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
chardump.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
chardump.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
chardump.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
chardump.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
chardump.o: version.h
cloud.o: AppHdr.h cloud.h debug.h defines.h enum.h externs.h FixAry.h
cloud.o: FixVec.h liblinux.h libutil.h message.h stuff.h
cloud.o: /usr/include/alloca.h /usr/include/asm/errno.h /usr/include/assert.h
cloud.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
cloud.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
cloud.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
cloud.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
cloud.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
cloud.o: /usr/include/bits/sched.h /usr/include/bits/select.h
cloud.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
cloud.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
cloud.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
cloud.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
cloud.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
cloud.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
cloud.o: /usr/include/fcntl.h /usr/include/features.h
cloud.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
cloud.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
cloud.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
cloud.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
cloud.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
cloud.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
cloud.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
cloud.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
cloud.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
cloud.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
cloud.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
cloud.o: /usr/include/g++-3/stl_relops.h
cloud.o: /usr/include/g++-3/stl_uninitialized.h
cloud.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
cloud.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
cloud.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
cloud.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
cloud.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
cloud.o: /usr/include/stdlib.h /usr/include/string.h /usr/include/sys/cdefs.h
cloud.o: /usr/include/sys/select.h /usr/include/sys/stat.h
cloud.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
cloud.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
cloud.o: /usr/include/xlocale.h
cloud.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
cloud.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
cloud.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
cloud.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
cloud.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
cloud.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
cloud.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
command.o: AppHdr.h command.h debug.h defines.h enum.h externs.h FixAry.h
command.o: FixVec.h invent.h itemname.h items.h liblinux.h libutil.h
command.o: message.h ouch.h spl-cast.h spl-util.h stuff.h
command.o: /usr/include/alloca.h /usr/include/asm/errno.h
command.o: /usr/include/assert.h /usr/include/bits/confname.h
command.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
command.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
command.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
command.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
command.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
command.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
command.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
command.o: /usr/include/bits/time.h /usr/include/bits/types.h
command.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
command.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
command.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
command.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
command.o: /usr/include/features.h /usr/include/g++-3/alloc.h
command.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
command.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
command.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
command.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
command.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
command.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
command.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
command.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
command.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
command.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
command.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
command.o: /usr/include/g++-3/stl_uninitialized.h
command.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
command.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
command.o: /usr/include/_G_config.h /usr/include/gconv.h
command.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
command.o: /usr/include/libio.h /usr/include/limits.h
command.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
command.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
command.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
command.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
command.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
command.o: /usr/include/wchar.h /usr/include/xlocale.h
command.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
command.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
command.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
command.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
command.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
command.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
command.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
command.o: version.h wpn-misc.h
debug.o: AppHdr.h debug.h defines.h direct.h dungeon.h enum.h externs.h
debug.o: FixAry.h FixVec.h invent.h itemname.h items.h liblinux.h libutil.h
debug.o: message.h misc.h monplace.h mon-util.h mutation.h player.h randart.h
debug.o: religion.h skills2.h skills.h spl-cast.h spl-util.h stuff.h
debug.o: /usr/include/alloca.h /usr/include/asm/errno.h /usr/include/assert.h
debug.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
debug.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
debug.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
debug.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
debug.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
debug.o: /usr/include/bits/sched.h /usr/include/bits/select.h
debug.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
debug.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
debug.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
debug.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
debug.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
debug.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
debug.o: /usr/include/fcntl.h /usr/include/features.h
debug.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
debug.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
debug.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
debug.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
debug.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
debug.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
debug.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
debug.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
debug.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
debug.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
debug.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
debug.o: /usr/include/g++-3/stl_relops.h
debug.o: /usr/include/g++-3/stl_uninitialized.h
debug.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
debug.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
debug.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
debug.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
debug.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
debug.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
debug.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
debug.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
debug.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
debug.o: /usr/include/wchar.h /usr/include/xlocale.h
debug.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
debug.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
debug.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
debug.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
debug.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
debug.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
debug.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
decks.o: AppHdr.h debug.h decks.h defines.h direct.h effects.h enum.h
decks.o: externs.h FixAry.h FixVec.h food.h items.h it_use2.h liblinux.h
decks.o: libutil.h message.h misc.h monplace.h mutation.h ouch.h player.h
decks.o: religion.h spells1.h spells3.h spl-cast.h stuff.h
decks.o: /usr/include/alloca.h /usr/include/asm/errno.h /usr/include/assert.h
decks.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
decks.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
decks.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
decks.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
decks.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
decks.o: /usr/include/bits/sched.h /usr/include/bits/select.h
decks.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
decks.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
decks.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
decks.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
decks.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
decks.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
decks.o: /usr/include/fcntl.h /usr/include/features.h
decks.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
decks.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
decks.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
decks.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
decks.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
decks.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
decks.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
decks.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
decks.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
decks.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
decks.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
decks.o: /usr/include/g++-3/stl_relops.h
decks.o: /usr/include/g++-3/stl_uninitialized.h
decks.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
decks.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
decks.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
decks.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
decks.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
decks.o: /usr/include/stdlib.h /usr/include/string.h /usr/include/sys/cdefs.h
decks.o: /usr/include/sys/select.h /usr/include/sys/stat.h
decks.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
decks.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
decks.o: /usr/include/xlocale.h
decks.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
decks.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
decks.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
decks.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
decks.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
decks.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
decks.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
delay.o: AppHdr.h debug.h defines.h delay.h enum.h externs.h fight.h FixAry.h
delay.o: FixVec.h food.h itemname.h items.h item_use.h it_use2.h liblinux.h
delay.o: libutil.h message.h misc.h monstuff.h ouch.h output.h player.h
delay.o: randart.h stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
delay.o: /usr/include/assert.h /usr/include/bits/confname.h
delay.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
delay.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
delay.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
delay.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
delay.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
delay.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
delay.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
delay.o: /usr/include/bits/time.h /usr/include/bits/types.h
delay.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
delay.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
delay.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
delay.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
delay.o: /usr/include/features.h /usr/include/g++-3/alloc.h
delay.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
delay.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
delay.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
delay.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
delay.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
delay.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
delay.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
delay.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
delay.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
delay.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
delay.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
delay.o: /usr/include/g++-3/stl_uninitialized.h
delay.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
delay.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
delay.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
delay.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
delay.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
delay.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
delay.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
delay.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
delay.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
delay.o: /usr/include/wchar.h /usr/include/xlocale.h
delay.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
delay.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
delay.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
delay.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
delay.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
delay.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
delay.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
describe.o: AppHdr.h debug.h defines.h describe.h enum.h externs.h fight.h
describe.o: FixAry.h FixVec.h itemname.h liblinux.h libutil.h message.h
describe.o: mon-util.h player.h randart.h religion.h skills2.h spl-util.h
describe.o: stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
describe.o: /usr/include/assert.h /usr/include/bits/confname.h
describe.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
describe.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
describe.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
describe.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
describe.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
describe.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
describe.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
describe.o: /usr/include/bits/time.h /usr/include/bits/types.h
describe.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
describe.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
describe.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
describe.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
describe.o: /usr/include/features.h /usr/include/g++-3/alloc.h
describe.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
describe.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
describe.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
describe.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
describe.o: /usr/include/g++-3/std/bastring.h
describe.o: /usr/include/g++-3/std/straits.h
describe.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
describe.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
describe.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
describe.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
describe.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
describe.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
describe.o: /usr/include/g++-3/stl_uninitialized.h
describe.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
describe.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
describe.o: /usr/include/_G_config.h /usr/include/gconv.h
describe.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
describe.o: /usr/include/libio.h /usr/include/limits.h
describe.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
describe.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
describe.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
describe.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
describe.o: /usr/include/sys/types.h /usr/include/time.h
describe.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
describe.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
describe.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
describe.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
describe.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
describe.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
describe.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
describe.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
describe.o: wpn-misc.h
direct.o: AppHdr.h debug.h defines.h describe.h direct.h enum.h externs.h
direct.o: FixAry.h FixVec.h itemname.h liblinux.h libutil.h message.h
direct.o: monstuff.h mon-util.h player.h shopping.h spells4.h stuff.h
direct.o: /usr/include/alloca.h /usr/include/asm/errno.h
direct.o: /usr/include/assert.h /usr/include/bits/confname.h
direct.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
direct.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
direct.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
direct.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
direct.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
direct.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
direct.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
direct.o: /usr/include/bits/time.h /usr/include/bits/types.h
direct.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
direct.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
direct.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
direct.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
direct.o: /usr/include/features.h /usr/include/g++-3/alloc.h
direct.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
direct.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
direct.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
direct.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
direct.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
direct.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
direct.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
direct.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
direct.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
direct.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
direct.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
direct.o: /usr/include/g++-3/stl_uninitialized.h
direct.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
direct.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
direct.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
direct.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
direct.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
direct.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
direct.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
direct.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
direct.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
direct.o: /usr/include/wchar.h /usr/include/xlocale.h
direct.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
direct.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
direct.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
direct.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
direct.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
direct.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
direct.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
dungeon.o: abyss.h AppHdr.h debug.h defines.h dungeon.h enum.h externs.h
dungeon.o: FixAry.h FixVec.h itemname.h items.h liblinux.h libutil.h maps.h
dungeon.o: message.h mon-pick.h monplace.h mon-util.h randart.h spl-book.h
dungeon.o: stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
dungeon.o: /usr/include/assert.h /usr/include/bits/confname.h
dungeon.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
dungeon.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
dungeon.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
dungeon.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
dungeon.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
dungeon.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
dungeon.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
dungeon.o: /usr/include/bits/time.h /usr/include/bits/types.h
dungeon.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
dungeon.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
dungeon.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
dungeon.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
dungeon.o: /usr/include/features.h /usr/include/g++-3/alloc.h
dungeon.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
dungeon.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
dungeon.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
dungeon.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
dungeon.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
dungeon.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
dungeon.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
dungeon.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
dungeon.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
dungeon.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
dungeon.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
dungeon.o: /usr/include/g++-3/stl_uninitialized.h
dungeon.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
dungeon.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
dungeon.o: /usr/include/_G_config.h /usr/include/gconv.h
dungeon.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
dungeon.o: /usr/include/libio.h /usr/include/limits.h
dungeon.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
dungeon.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
dungeon.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
dungeon.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
dungeon.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
dungeon.o: /usr/include/wchar.h /usr/include/xlocale.h
dungeon.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
dungeon.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
dungeon.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
dungeon.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
dungeon.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
dungeon.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
dungeon.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
dungeon.o: wpn-misc.h
effects.o: AppHdr.h beam.h debug.h defines.h direct.h dungeon.h effects.h
effects.o: enum.h externs.h fight.h FixAry.h FixVec.h itemname.h items.h
effects.o: liblinux.h libutil.h message.h misc.h monplace.h monstuff.h
effects.o: mon-util.h mutation.h newgame.h ouch.h player.h skills2.h
effects.o: spells3.h spl-book.h spl-util.h stuff.h /usr/include/alloca.h
effects.o: /usr/include/asm/errno.h /usr/include/assert.h
effects.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
effects.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
effects.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
effects.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
effects.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
effects.o: /usr/include/bits/sched.h /usr/include/bits/select.h
effects.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
effects.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
effects.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
effects.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
effects.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
effects.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
effects.o: /usr/include/fcntl.h /usr/include/features.h
effects.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
effects.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
effects.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
effects.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
effects.o: /usr/include/g++-3/std/bastring.cc
effects.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
effects.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
effects.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
effects.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
effects.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
effects.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
effects.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
effects.o: /usr/include/g++-3/stl_uninitialized.h
effects.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
effects.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
effects.o: /usr/include/_G_config.h /usr/include/gconv.h
effects.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
effects.o: /usr/include/libio.h /usr/include/limits.h
effects.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
effects.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
effects.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
effects.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
effects.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
effects.o: /usr/include/wchar.h /usr/include/xlocale.h
effects.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
effects.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
effects.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
effects.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
effects.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
effects.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
effects.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
effects.o: wpn-misc.h
fight.o: AppHdr.h beam.h cloud.h debug.h defines.h delay.h direct.h effects.h
fight.o: enum.h externs.h fight.h FixAry.h FixVec.h food.h itemname.h items.h
fight.o: it_use2.h liblinux.h libutil.h message.h misc.h mon-pick.h
fight.o: monplace.h monstuff.h mon-util.h mstuff2.h mutation.h ouch.h
fight.o: player.h randart.h religion.h skills.h spells1.h spells3.h spells4.h
fight.o: spl-cast.h stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
fight.o: /usr/include/assert.h /usr/include/bits/confname.h
fight.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
fight.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
fight.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
fight.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
fight.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
fight.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
fight.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
fight.o: /usr/include/bits/time.h /usr/include/bits/types.h
fight.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
fight.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
fight.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
fight.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
fight.o: /usr/include/features.h /usr/include/g++-3/alloc.h
fight.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
fight.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
fight.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
fight.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
fight.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
fight.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
fight.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
fight.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
fight.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
fight.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
fight.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
fight.o: /usr/include/g++-3/stl_uninitialized.h
fight.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
fight.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
fight.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
fight.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
fight.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
fight.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
fight.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
fight.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
fight.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
fight.o: /usr/include/wchar.h /usr/include/xlocale.h
fight.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
fight.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
fight.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
fight.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
fight.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
fight.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
fight.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
fight.o: wpn-misc.h
files.o: AppHdr.h cloud.h debug.h defines.h dungeon.h enum.h externs.h
files.o: files.h FixAry.h FixVec.h itemname.h items.h liblinux.h libutil.h
files.o: message.h misc.h monstuff.h mon-util.h mstuff2.h player.h randart.h
files.o: skills2.h stuff.h tags.h /usr/include/alloca.h
files.o: /usr/include/asm/errno.h /usr/include/assert.h
files.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
files.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
files.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
files.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
files.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
files.o: /usr/include/bits/sched.h /usr/include/bits/select.h
files.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
files.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
files.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
files.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
files.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
files.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
files.o: /usr/include/fcntl.h /usr/include/features.h
files.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
files.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
files.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
files.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
files.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
files.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
files.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
files.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
files.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
files.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
files.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
files.o: /usr/include/g++-3/stl_relops.h
files.o: /usr/include/g++-3/stl_uninitialized.h
files.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
files.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
files.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
files.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
files.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
files.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
files.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
files.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
files.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
files.o: /usr/include/wchar.h /usr/include/xlocale.h
files.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
files.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
files.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
files.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
files.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
files.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
files.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
files.o: wpn-misc.h
food.o: AppHdr.h debug.h defines.h delay.h enum.h externs.h FixAry.h FixVec.h
food.o: food.h invent.h itemname.h items.h item_use.h it_use2.h liblinux.h
food.o: libutil.h message.h misc.h mon-util.h mutation.h player.h religion.h
food.o: skills2.h spells2.h stuff.h /usr/include/alloca.h
food.o: /usr/include/asm/errno.h /usr/include/assert.h
food.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
food.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
food.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
food.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
food.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
food.o: /usr/include/bits/sched.h /usr/include/bits/select.h
food.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
food.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
food.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
food.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
food.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
food.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
food.o: /usr/include/fcntl.h /usr/include/features.h
food.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
food.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
food.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
food.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
food.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
food.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
food.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
food.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
food.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
food.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
food.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
food.o: /usr/include/g++-3/stl_relops.h
food.o: /usr/include/g++-3/stl_uninitialized.h
food.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
food.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
food.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
food.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
food.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
food.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
food.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
food.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
food.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
food.o: /usr/include/wchar.h /usr/include/xlocale.h
food.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
food.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
food.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
food.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
food.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
food.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
food.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
food.o: wpn-misc.h
hiscores.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
hiscores.o: hiscores.h liblinux.h libutil.h message.h mon-util.h player.h
hiscores.o: tags.h /usr/include/alloca.h /usr/include/asm/errno.h
hiscores.o: /usr/include/assert.h /usr/include/bits/confname.h
hiscores.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
hiscores.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
hiscores.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
hiscores.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
hiscores.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
hiscores.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
hiscores.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
hiscores.o: /usr/include/bits/time.h /usr/include/bits/types.h
hiscores.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
hiscores.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
hiscores.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
hiscores.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
hiscores.o: /usr/include/features.h /usr/include/g++-3/alloc.h
hiscores.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
hiscores.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
hiscores.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
hiscores.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
hiscores.o: /usr/include/g++-3/std/bastring.h
hiscores.o: /usr/include/g++-3/std/straits.h
hiscores.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
hiscores.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
hiscores.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
hiscores.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
hiscores.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
hiscores.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
hiscores.o: /usr/include/g++-3/stl_uninitialized.h
hiscores.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
hiscores.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
hiscores.o: /usr/include/_G_config.h /usr/include/gconv.h
hiscores.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
hiscores.o: /usr/include/libio.h /usr/include/limits.h
hiscores.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
hiscores.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
hiscores.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
hiscores.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
hiscores.o: /usr/include/sys/types.h /usr/include/time.h
hiscores.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
hiscores.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
hiscores.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
hiscores.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
hiscores.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
hiscores.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
hiscores.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
hiscores.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
hiscores.o: view.h
initfile.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
initfile.o: initfile.h items.h liblinux.h libutil.h message.h
initfile.o: /usr/include/alloca.h /usr/include/asm/errno.h
initfile.o: /usr/include/assert.h /usr/include/bits/confname.h
initfile.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
initfile.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
initfile.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
initfile.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
initfile.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
initfile.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
initfile.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
initfile.o: /usr/include/bits/time.h /usr/include/bits/types.h
initfile.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
initfile.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
initfile.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
initfile.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
initfile.o: /usr/include/features.h /usr/include/g++-3/alloc.h
initfile.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
initfile.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
initfile.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
initfile.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
initfile.o: /usr/include/g++-3/std/bastring.h
initfile.o: /usr/include/g++-3/std/straits.h
initfile.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
initfile.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
initfile.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
initfile.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
initfile.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
initfile.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
initfile.o: /usr/include/g++-3/stl_uninitialized.h
initfile.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
initfile.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
initfile.o: /usr/include/_G_config.h /usr/include/gconv.h
initfile.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
initfile.o: /usr/include/libio.h /usr/include/limits.h
initfile.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
initfile.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
initfile.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
initfile.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
initfile.o: /usr/include/sys/types.h /usr/include/time.h
initfile.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
initfile.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
initfile.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
initfile.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
initfile.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
initfile.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
initfile.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
initfile.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
initfile.o: view.h
insult.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
insult.o: insult.h liblinux.h libutil.h message.h mon-util.h stuff.h
insult.o: /usr/include/alloca.h /usr/include/asm/errno.h
insult.o: /usr/include/assert.h /usr/include/bits/confname.h
insult.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
insult.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
insult.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
insult.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
insult.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
insult.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
insult.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
insult.o: /usr/include/bits/time.h /usr/include/bits/types.h
insult.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
insult.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
insult.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
insult.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
insult.o: /usr/include/features.h /usr/include/g++-3/alloc.h
insult.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
insult.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
insult.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
insult.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
insult.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
insult.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
insult.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
insult.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
insult.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
insult.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
insult.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
insult.o: /usr/include/g++-3/stl_uninitialized.h
insult.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
insult.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
insult.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
insult.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
insult.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
insult.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
insult.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
insult.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
insult.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
insult.o: /usr/include/wchar.h /usr/include/xlocale.h
insult.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
insult.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
insult.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
insult.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
insult.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
insult.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
insult.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
invent.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
invent.o: invent.h itemname.h items.h liblinux.h libutil.h message.h
invent.o: shopping.h stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
invent.o: /usr/include/assert.h /usr/include/bits/confname.h
invent.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
invent.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
invent.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
invent.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
invent.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
invent.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
invent.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
invent.o: /usr/include/bits/time.h /usr/include/bits/types.h
invent.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
invent.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
invent.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
invent.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
invent.o: /usr/include/features.h /usr/include/g++-3/alloc.h
invent.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
invent.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
invent.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
invent.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
invent.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
invent.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
invent.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
invent.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
invent.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
invent.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
invent.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
invent.o: /usr/include/g++-3/stl_uninitialized.h
invent.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
invent.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
invent.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
invent.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
invent.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
invent.o: /usr/include/stdlib.h /usr/include/string.h
invent.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
invent.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
invent.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
invent.o: /usr/include/wchar.h /usr/include/xlocale.h
invent.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
invent.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
invent.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
invent.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
invent.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
invent.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
invent.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
itemname.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
itemname.o: itemname.h liblinux.h libutil.h message.h mon-util.h randart.h
itemname.o: skills2.h stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
itemname.o: /usr/include/assert.h /usr/include/bits/confname.h
itemname.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
itemname.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
itemname.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
itemname.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
itemname.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
itemname.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
itemname.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
itemname.o: /usr/include/bits/time.h /usr/include/bits/types.h
itemname.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
itemname.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
itemname.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
itemname.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
itemname.o: /usr/include/features.h /usr/include/g++-3/alloc.h
itemname.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
itemname.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
itemname.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
itemname.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
itemname.o: /usr/include/g++-3/std/bastring.h
itemname.o: /usr/include/g++-3/std/straits.h
itemname.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
itemname.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
itemname.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
itemname.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
itemname.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
itemname.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
itemname.o: /usr/include/g++-3/stl_uninitialized.h
itemname.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
itemname.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
itemname.o: /usr/include/_G_config.h /usr/include/gconv.h
itemname.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
itemname.o: /usr/include/libio.h /usr/include/limits.h
itemname.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
itemname.o: /usr/include/stdlib.h /usr/include/string.h
itemname.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
itemname.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
itemname.o: /usr/include/sys/types.h /usr/include/time.h
itemname.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
itemname.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
itemname.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
itemname.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
itemname.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
itemname.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
itemname.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
itemname.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
itemname.o: view.h wpn-misc.h
items.o: AppHdr.h beam.h debug.h defines.h delay.h effects.h enum.h externs.h
items.o: fight.h FixAry.h FixVec.h invent.h itemname.h items.h item_use.h
items.o: it_use2.h liblinux.h libutil.h message.h misc.h monplace.h
items.o: mon-util.h mutation.h player.h randart.h religion.h shopping.h
items.o: skills.h spl-cast.h stuff.h /usr/include/alloca.h
items.o: /usr/include/asm/errno.h /usr/include/assert.h
items.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
items.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
items.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
items.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
items.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
items.o: /usr/include/bits/sched.h /usr/include/bits/select.h
items.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
items.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
items.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
items.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
items.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
items.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
items.o: /usr/include/fcntl.h /usr/include/features.h
items.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
items.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
items.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
items.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
items.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
items.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
items.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
items.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
items.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
items.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
items.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
items.o: /usr/include/g++-3/stl_relops.h
items.o: /usr/include/g++-3/stl_uninitialized.h
items.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
items.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
items.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
items.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
items.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
items.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
items.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
items.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
items.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
items.o: /usr/include/wchar.h /usr/include/xlocale.h
items.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
items.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
items.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
items.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
items.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
items.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
items.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
item_use.o: AppHdr.h beam.h debug.h defines.h delay.h describe.h direct.h
item_use.o: effects.h enum.h externs.h fight.h FixAry.h FixVec.h food.h
item_use.o: invent.h itemname.h items.h item_use.h it_use2.h it_use3.h
item_use.o: liblinux.h libutil.h message.h misc.h monplace.h monstuff.h
item_use.o: mon-util.h mstuff2.h ouch.h player.h randart.h religion.h
item_use.o: skills2.h skills.h spells1.h spells2.h spells3.h spl-book.h
item_use.o: spl-cast.h stuff.h transfor.h /usr/include/alloca.h
item_use.o: /usr/include/asm/errno.h /usr/include/assert.h
item_use.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
item_use.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
item_use.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
item_use.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
item_use.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
item_use.o: /usr/include/bits/sched.h /usr/include/bits/select.h
item_use.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
item_use.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
item_use.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
item_use.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
item_use.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
item_use.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
item_use.o: /usr/include/fcntl.h /usr/include/features.h
item_use.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
item_use.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
item_use.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
item_use.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
item_use.o: /usr/include/g++-3/std/bastring.cc
item_use.o: /usr/include/g++-3/std/bastring.h
item_use.o: /usr/include/g++-3/std/straits.h
item_use.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
item_use.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
item_use.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
item_use.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
item_use.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
item_use.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
item_use.o: /usr/include/g++-3/stl_uninitialized.h
item_use.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
item_use.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
item_use.o: /usr/include/_G_config.h /usr/include/gconv.h
item_use.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
item_use.o: /usr/include/libio.h /usr/include/limits.h
item_use.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
item_use.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
item_use.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
item_use.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
item_use.o: /usr/include/sys/types.h /usr/include/time.h
item_use.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
item_use.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
item_use.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
item_use.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
item_use.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
item_use.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
item_use.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
item_use.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
item_use.o: view.h wpn-misc.h
it_use2.o: AppHdr.h beam.h debug.h defines.h effects.h enum.h externs.h
it_use2.o: FixAry.h FixVec.h food.h itemname.h it_use2.h liblinux.h libutil.h
it_use2.o: message.h misc.h mutation.h player.h randart.h religion.h
it_use2.o: skills2.h spells2.h spl-cast.h stuff.h /usr/include/alloca.h
it_use2.o: /usr/include/asm/errno.h /usr/include/assert.h
it_use2.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
it_use2.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
it_use2.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
it_use2.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
it_use2.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
it_use2.o: /usr/include/bits/sched.h /usr/include/bits/select.h
it_use2.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
it_use2.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
it_use2.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
it_use2.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
it_use2.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
it_use2.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
it_use2.o: /usr/include/fcntl.h /usr/include/features.h
it_use2.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
it_use2.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
it_use2.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
it_use2.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
it_use2.o: /usr/include/g++-3/std/bastring.cc
it_use2.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
it_use2.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
it_use2.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
it_use2.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
it_use2.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
it_use2.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
it_use2.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
it_use2.o: /usr/include/g++-3/stl_uninitialized.h
it_use2.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
it_use2.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
it_use2.o: /usr/include/_G_config.h /usr/include/gconv.h
it_use2.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
it_use2.o: /usr/include/libio.h /usr/include/limits.h
it_use2.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
it_use2.o: /usr/include/stdlib.h /usr/include/string.h
it_use2.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
it_use2.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
it_use2.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
it_use2.o: /usr/include/wchar.h /usr/include/xlocale.h
it_use2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
it_use2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
it_use2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
it_use2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
it_use2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
it_use2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
it_use2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
it_use3.o: AppHdr.h beam.h debug.h decks.h defines.h direct.h effects.h
it_use3.o: enum.h externs.h fight.h FixAry.h FixVec.h itemname.h items.h
it_use3.o: it_use2.h it_use3.h liblinux.h libutil.h message.h misc.h
it_use3.o: monplace.h monstuff.h player.h randart.h skills2.h skills.h
it_use3.o: spells1.h spells2.h spl-book.h spl-cast.h spl-util.h stuff.h
it_use3.o: /usr/include/alloca.h /usr/include/asm/errno.h
it_use3.o: /usr/include/assert.h /usr/include/bits/confname.h
it_use3.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
it_use3.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
it_use3.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
it_use3.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
it_use3.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
it_use3.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
it_use3.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
it_use3.o: /usr/include/bits/time.h /usr/include/bits/types.h
it_use3.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
it_use3.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
it_use3.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
it_use3.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
it_use3.o: /usr/include/features.h /usr/include/g++-3/alloc.h
it_use3.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
it_use3.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
it_use3.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
it_use3.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
it_use3.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
it_use3.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
it_use3.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
it_use3.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
it_use3.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
it_use3.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
it_use3.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
it_use3.o: /usr/include/g++-3/stl_uninitialized.h
it_use3.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
it_use3.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
it_use3.o: /usr/include/_G_config.h /usr/include/gconv.h
it_use3.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
it_use3.o: /usr/include/libio.h /usr/include/limits.h
it_use3.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
it_use3.o: /usr/include/stdlib.h /usr/include/string.h
it_use3.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
it_use3.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
it_use3.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
it_use3.o: /usr/include/wchar.h /usr/include/xlocale.h
it_use3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
it_use3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
it_use3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
it_use3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
it_use3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
it_use3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
it_use3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
it_use3.o: wpn-misc.h
lev-pand.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
lev-pand.o: lev-pand.h liblinux.h libutil.h message.h mon-pick.h monplace.h
lev-pand.o: stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
lev-pand.o: /usr/include/assert.h /usr/include/bits/confname.h
lev-pand.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
lev-pand.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
lev-pand.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
lev-pand.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
lev-pand.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
lev-pand.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
lev-pand.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
lev-pand.o: /usr/include/bits/time.h /usr/include/bits/types.h
lev-pand.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
lev-pand.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
lev-pand.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
lev-pand.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
lev-pand.o: /usr/include/features.h /usr/include/g++-3/alloc.h
lev-pand.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
lev-pand.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
lev-pand.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
lev-pand.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
lev-pand.o: /usr/include/g++-3/std/bastring.h
lev-pand.o: /usr/include/g++-3/std/straits.h
lev-pand.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
lev-pand.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
lev-pand.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
lev-pand.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
lev-pand.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
lev-pand.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
lev-pand.o: /usr/include/g++-3/stl_uninitialized.h
lev-pand.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
lev-pand.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
lev-pand.o: /usr/include/_G_config.h /usr/include/gconv.h
lev-pand.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
lev-pand.o: /usr/include/libio.h /usr/include/limits.h
lev-pand.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
lev-pand.o: /usr/include/stdlib.h /usr/include/string.h
lev-pand.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
lev-pand.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
lev-pand.o: /usr/include/sys/types.h /usr/include/time.h
lev-pand.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
lev-pand.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
lev-pand.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
lev-pand.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
lev-pand.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
lev-pand.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
lev-pand.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
lev-pand.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
liblinux.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
liblinux.o: liblinux.h libutil.h message.h /usr/include/alloca.h
liblinux.o: /usr/include/asm/errno.h /usr/include/asm/sigcontext.h
liblinux.o: /usr/include/assert.h /usr/include/bits/confname.h
liblinux.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
liblinux.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
liblinux.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
liblinux.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
liblinux.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
liblinux.o: /usr/include/bits/select.h /usr/include/bits/sigaction.h
liblinux.o: /usr/include/bits/sigcontext.h /usr/include/bits/siginfo.h
liblinux.o: /usr/include/bits/signum.h /usr/include/bits/sigset.h
liblinux.o: /usr/include/bits/sigstack.h /usr/include/bits/sigthread.h
liblinux.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
liblinux.o: /usr/include/bits/termios.h /usr/include/bits/time.h
liblinux.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
liblinux.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
liblinux.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
liblinux.o: /usr/include/ctype.h /usr/include/curses.h /usr/include/endian.h
liblinux.o: /usr/include/errno.h /usr/include/fcntl.h /usr/include/features.h
liblinux.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
liblinux.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
liblinux.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
liblinux.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
liblinux.o: /usr/include/g++-3/std/bastring.cc
liblinux.o: /usr/include/g++-3/std/bastring.h
liblinux.o: /usr/include/g++-3/std/straits.h
liblinux.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
liblinux.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
liblinux.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
liblinux.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
liblinux.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
liblinux.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
liblinux.o: /usr/include/g++-3/stl_uninitialized.h
liblinux.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
liblinux.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
liblinux.o: /usr/include/_G_config.h /usr/include/gconv.h
liblinux.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
liblinux.o: /usr/include/libio.h /usr/include/limits.h
liblinux.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
liblinux.o: /usr/include/ncurses/curses.h /usr/include/ncurses/ncurses_dll.h
liblinux.o: /usr/include/ncurses/unctrl.h /usr/include/signal.h
liblinux.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
liblinux.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
liblinux.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
liblinux.o: /usr/include/sys/ttydefaults.h /usr/include/sys/types.h
liblinux.o: /usr/include/sys/ucontext.h /usr/include/termios.h
liblinux.o: /usr/include/time.h /usr/include/ucontext.h /usr/include/unistd.h
liblinux.o: /usr/include/wchar.h /usr/include/xlocale.h
liblinux.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
liblinux.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
liblinux.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
liblinux.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
liblinux.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
liblinux.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
liblinux.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
libutil.o: AppHdr.h liblinux.h libutil.h /usr/include/alloca.h
libutil.o: /usr/include/asm/errno.h /usr/include/assert.h
libutil.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
libutil.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
libutil.o: /usr/include/bits/fcntl.h /usr/include/bits/posix_opt.h
libutil.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
libutil.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
libutil.o: /usr/include/bits/stat.h /usr/include/bits/time.h
libutil.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
libutil.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
libutil.o: /usr/include/bits/wordsize.h /usr/include/ctype.h
libutil.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
libutil.o: /usr/include/features.h /usr/include/g++-3/alloc.h
libutil.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
libutil.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
libutil.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
libutil.o: /usr/include/g++-3/std/bastring.cc
libutil.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
libutil.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_config.h
libutil.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_relops.h
libutil.o: /usr/include/g++-3/streambuf.h /usr/include/g++-3/string
libutil.o: /usr/include/_G_config.h /usr/include/gconv.h
libutil.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
libutil.o: /usr/include/libio.h /usr/include/linux/errno.h
libutil.o: /usr/include/stdlib.h /usr/include/string.h
libutil.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
libutil.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
libutil.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
libutil.o: /usr/include/wchar.h /usr/include/xlocale.h
libutil.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
libutil.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
libw32c.o: AppHdr.h defines.h liblinux.h libutil.h /usr/include/alloca.h
libw32c.o: /usr/include/asm/errno.h /usr/include/assert.h
libw32c.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
libw32c.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
libw32c.o: /usr/include/bits/fcntl.h /usr/include/bits/posix_opt.h
libw32c.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
libw32c.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
libw32c.o: /usr/include/bits/stat.h /usr/include/bits/time.h
libw32c.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
libw32c.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
libw32c.o: /usr/include/bits/wordsize.h /usr/include/ctype.h
libw32c.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
libw32c.o: /usr/include/features.h /usr/include/g++-3/alloc.h
libw32c.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
libw32c.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
libw32c.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
libw32c.o: /usr/include/g++-3/std/bastring.cc
libw32c.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
libw32c.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_config.h
libw32c.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_relops.h
libw32c.o: /usr/include/g++-3/streambuf.h /usr/include/g++-3/string
libw32c.o: /usr/include/_G_config.h /usr/include/gconv.h
libw32c.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
libw32c.o: /usr/include/libio.h /usr/include/linux/errno.h
libw32c.o: /usr/include/stdlib.h /usr/include/string.h
libw32c.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
libw32c.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
libw32c.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
libw32c.o: /usr/include/wchar.h /usr/include/xlocale.h
libw32c.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
libw32c.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h version.h
macro.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
macro.o: liblinux.h libmac.h libutil.h macro.h message.h
macro.o: /usr/include/alloca.h /usr/include/asm/errno.h /usr/include/assert.h
macro.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
macro.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
macro.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
macro.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
macro.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
macro.o: /usr/include/bits/sched.h /usr/include/bits/select.h
macro.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
macro.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
macro.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
macro.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
macro.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
macro.o: /usr/include/ctype.h /usr/include/curses.h /usr/include/endian.h
macro.o: /usr/include/errno.h /usr/include/fcntl.h /usr/include/features.h
macro.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
macro.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
macro.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
macro.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
macro.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
macro.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
macro.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
macro.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
macro.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
macro.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
macro.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
macro.o: /usr/include/g++-3/stl_relops.h
macro.o: /usr/include/g++-3/stl_uninitialized.h
macro.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
macro.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
macro.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
macro.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
macro.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
macro.o: /usr/include/ncurses/curses.h /usr/include/ncurses/ncurses_dll.h
macro.o: /usr/include/ncurses/unctrl.h /usr/include/stdio.h
macro.o: /usr/include/stdlib.h /usr/include/string.h /usr/include/sys/cdefs.h
macro.o: /usr/include/sys/select.h /usr/include/sys/stat.h
macro.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
macro.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
macro.o: /usr/include/xlocale.h
macro.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
macro.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
macro.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
macro.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
macro.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
macro.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
macro.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
maps.o: AppHdr.h debug.h enum.h FixVec.h liblinux.h libutil.h maps.h
maps.o: monplace.h stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
maps.o: /usr/include/assert.h /usr/include/bits/confname.h
maps.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
maps.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
maps.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
maps.o: /usr/include/bits/sched.h /usr/include/bits/select.h
maps.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
maps.o: /usr/include/bits/time.h /usr/include/bits/types.h
maps.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
maps.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
maps.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
maps.o: /usr/include/fcntl.h /usr/include/features.h
maps.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
maps.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
maps.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
maps.o: /usr/include/g++-3/iterator /usr/include/g++-3/std/bastring.cc
maps.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
maps.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_config.h
maps.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_relops.h
maps.o: /usr/include/g++-3/streambuf.h /usr/include/g++-3/string
maps.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
maps.o: /usr/include/gnu/stubs.h /usr/include/libio.h
maps.o: /usr/include/linux/errno.h /usr/include/stdlib.h
maps.o: /usr/include/string.h /usr/include/sys/cdefs.h
maps.o: /usr/include/sys/select.h /usr/include/sys/stat.h
maps.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
maps.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
maps.o: /usr/include/xlocale.h
maps.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
maps.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
message.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
message.o: liblinux.h libutil.h message.h religion.h stuff.h
message.o: /usr/include/alloca.h /usr/include/asm/errno.h
message.o: /usr/include/assert.h /usr/include/bits/confname.h
message.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
message.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
message.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
message.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
message.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
message.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
message.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
message.o: /usr/include/bits/time.h /usr/include/bits/types.h
message.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
message.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
message.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
message.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
message.o: /usr/include/features.h /usr/include/g++-3/alloc.h
message.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
message.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
message.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
message.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
message.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
message.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
message.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
message.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
message.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
message.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
message.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
message.o: /usr/include/g++-3/stl_uninitialized.h
message.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
message.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
message.o: /usr/include/_G_config.h /usr/include/gconv.h
message.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
message.o: /usr/include/libio.h /usr/include/limits.h
message.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
message.o: /usr/include/stdlib.h /usr/include/string.h
message.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
message.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
message.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
message.o: /usr/include/wchar.h /usr/include/xlocale.h
message.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
message.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
message.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
message.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
message.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
message.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
message.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
misc.o: AppHdr.h cloud.h debug.h defines.h enum.h externs.h fight.h files.h
misc.o: FixAry.h FixVec.h food.h items.h it_use2.h lev-pand.h liblinux.h
misc.o: libutil.h message.h misc.h monplace.h monstuff.h mon-util.h ouch.h
misc.o: player.h shopping.h skills2.h skills.h spells3.h spl-cast.h stuff.h
misc.o: transfor.h /usr/include/alloca.h /usr/include/asm/errno.h
misc.o: /usr/include/assert.h /usr/include/bits/confname.h
misc.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
misc.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
misc.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
misc.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
misc.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
misc.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
misc.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
misc.o: /usr/include/bits/time.h /usr/include/bits/types.h
misc.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
misc.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
misc.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
misc.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
misc.o: /usr/include/features.h /usr/include/g++-3/alloc.h
misc.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
misc.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
misc.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
misc.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
misc.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
misc.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
misc.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
misc.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
misc.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
misc.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
misc.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
misc.o: /usr/include/g++-3/stl_uninitialized.h
misc.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
misc.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
misc.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
misc.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
misc.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
misc.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
misc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
misc.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
misc.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
misc.o: /usr/include/wchar.h /usr/include/xlocale.h
misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
mon-pick.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
mon-pick.o: liblinux.h libutil.h message.h mon-pick.h /usr/include/alloca.h
mon-pick.o: /usr/include/asm/errno.h /usr/include/assert.h
mon-pick.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
mon-pick.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
mon-pick.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
mon-pick.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
mon-pick.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
mon-pick.o: /usr/include/bits/sched.h /usr/include/bits/select.h
mon-pick.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
mon-pick.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
mon-pick.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
mon-pick.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
mon-pick.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
mon-pick.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
mon-pick.o: /usr/include/fcntl.h /usr/include/features.h
mon-pick.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
mon-pick.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
mon-pick.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
mon-pick.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
mon-pick.o: /usr/include/g++-3/std/bastring.cc
mon-pick.o: /usr/include/g++-3/std/bastring.h
mon-pick.o: /usr/include/g++-3/std/straits.h
mon-pick.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
mon-pick.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
mon-pick.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
mon-pick.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
mon-pick.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
mon-pick.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
mon-pick.o: /usr/include/g++-3/stl_uninitialized.h
mon-pick.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
mon-pick.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
mon-pick.o: /usr/include/_G_config.h /usr/include/gconv.h
mon-pick.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
mon-pick.o: /usr/include/libio.h /usr/include/limits.h
mon-pick.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
mon-pick.o: /usr/include/stdlib.h /usr/include/string.h
mon-pick.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
mon-pick.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
mon-pick.o: /usr/include/sys/types.h /usr/include/time.h
mon-pick.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
mon-pick.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
mon-pick.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
mon-pick.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
mon-pick.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
mon-pick.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
mon-pick.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
mon-pick.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
monplace.o: AppHdr.h debug.h defines.h dungeon.h enum.h externs.h FixAry.h
monplace.o: FixVec.h liblinux.h libutil.h message.h misc.h mon-pick.h
monplace.o: monplace.h monstuff.h mon-util.h player.h spells4.h stuff.h
monplace.o: /usr/include/alloca.h /usr/include/asm/errno.h
monplace.o: /usr/include/assert.h /usr/include/bits/confname.h
monplace.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
monplace.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
monplace.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
monplace.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
monplace.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
monplace.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
monplace.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
monplace.o: /usr/include/bits/time.h /usr/include/bits/types.h
monplace.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
monplace.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
monplace.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
monplace.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
monplace.o: /usr/include/features.h /usr/include/g++-3/alloc.h
monplace.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
monplace.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
monplace.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
monplace.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
monplace.o: /usr/include/g++-3/std/bastring.h
monplace.o: /usr/include/g++-3/std/straits.h
monplace.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
monplace.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
monplace.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
monplace.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
monplace.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
monplace.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
monplace.o: /usr/include/g++-3/stl_uninitialized.h
monplace.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
monplace.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
monplace.o: /usr/include/_G_config.h /usr/include/gconv.h
monplace.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
monplace.o: /usr/include/libio.h /usr/include/limits.h
monplace.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
monplace.o: /usr/include/stdlib.h /usr/include/string.h
monplace.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
monplace.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
monplace.o: /usr/include/sys/types.h /usr/include/time.h
monplace.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
monplace.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
monplace.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
monplace.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
monplace.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
monplace.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
monplace.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
monplace.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
monspeak.o: AppHdr.h beam.h debug.h defines.h enum.h externs.h fight.h
monspeak.o: FixAry.h FixVec.h insult.h itemname.h liblinux.h libutil.h
monspeak.o: message.h misc.h monplace.h monspeak.h monstuff.h mon-util.h
monspeak.o: mstuff2.h player.h spells2.h spells4.h stuff.h
monspeak.o: /usr/include/alloca.h /usr/include/asm/errno.h
monspeak.o: /usr/include/assert.h /usr/include/bits/confname.h
monspeak.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
monspeak.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
monspeak.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
monspeak.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
monspeak.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
monspeak.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
monspeak.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
monspeak.o: /usr/include/bits/time.h /usr/include/bits/types.h
monspeak.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
monspeak.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
monspeak.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
monspeak.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
monspeak.o: /usr/include/features.h /usr/include/g++-3/alloc.h
monspeak.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
monspeak.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
monspeak.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
monspeak.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
monspeak.o: /usr/include/g++-3/std/bastring.h
monspeak.o: /usr/include/g++-3/std/straits.h
monspeak.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
monspeak.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
monspeak.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
monspeak.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
monspeak.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
monspeak.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
monspeak.o: /usr/include/g++-3/stl_uninitialized.h
monspeak.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
monspeak.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
monspeak.o: /usr/include/_G_config.h /usr/include/gconv.h
monspeak.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
monspeak.o: /usr/include/libio.h /usr/include/limits.h
monspeak.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
monspeak.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
monspeak.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
monspeak.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
monspeak.o: /usr/include/sys/types.h /usr/include/time.h
monspeak.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
monspeak.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
monspeak.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
monspeak.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
monspeak.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
monspeak.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
monspeak.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
monspeak.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
monspeak.o: view.h
monstuff.o: AppHdr.h beam.h cloud.h debug.h defines.h enum.h externs.h
monstuff.o: fight.h FixAry.h FixVec.h itemname.h items.h liblinux.h libutil.h
monstuff.o: message.h misc.h monplace.h monspeak.h monstuff.h mon-util.h
monstuff.o: mstuff2.h player.h randart.h spells2.h spells4.h stuff.h
monstuff.o: /usr/include/alloca.h /usr/include/asm/errno.h
monstuff.o: /usr/include/assert.h /usr/include/bits/confname.h
monstuff.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
monstuff.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
monstuff.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
monstuff.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
monstuff.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
monstuff.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
monstuff.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
monstuff.o: /usr/include/bits/time.h /usr/include/bits/types.h
monstuff.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
monstuff.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
monstuff.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
monstuff.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
monstuff.o: /usr/include/features.h /usr/include/g++-3/alloc.h
monstuff.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
monstuff.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
monstuff.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
monstuff.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
monstuff.o: /usr/include/g++-3/std/bastring.h
monstuff.o: /usr/include/g++-3/std/straits.h
monstuff.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
monstuff.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
monstuff.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
monstuff.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
monstuff.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
monstuff.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
monstuff.o: /usr/include/g++-3/stl_uninitialized.h
monstuff.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
monstuff.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
monstuff.o: /usr/include/_G_config.h /usr/include/gconv.h
monstuff.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
monstuff.o: /usr/include/libio.h /usr/include/limits.h
monstuff.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
monstuff.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
monstuff.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
monstuff.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
monstuff.o: /usr/include/sys/types.h /usr/include/time.h
monstuff.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
monstuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
monstuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
monstuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
monstuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
monstuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
monstuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
monstuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
monstuff.o: view.h
mon-util.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
mon-util.o: itemname.h liblinux.h libutil.h message.h mon-data.h mon-spll.h
mon-util.o: monstuff.h mon-util.h player.h randart.h stuff.h
mon-util.o: /usr/include/alloca.h /usr/include/asm/errno.h
mon-util.o: /usr/include/assert.h /usr/include/bits/confname.h
mon-util.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
mon-util.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
mon-util.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
mon-util.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
mon-util.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
mon-util.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
mon-util.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
mon-util.o: /usr/include/bits/time.h /usr/include/bits/types.h
mon-util.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
mon-util.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
mon-util.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
mon-util.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
mon-util.o: /usr/include/features.h /usr/include/g++-3/alloc.h
mon-util.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
mon-util.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
mon-util.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
mon-util.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
mon-util.o: /usr/include/g++-3/std/bastring.h
mon-util.o: /usr/include/g++-3/std/straits.h
mon-util.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
mon-util.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
mon-util.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
mon-util.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
mon-util.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
mon-util.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
mon-util.o: /usr/include/g++-3/stl_uninitialized.h
mon-util.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
mon-util.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
mon-util.o: /usr/include/_G_config.h /usr/include/gconv.h
mon-util.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
mon-util.o: /usr/include/libio.h /usr/include/limits.h
mon-util.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
mon-util.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
mon-util.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
mon-util.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
mon-util.o: /usr/include/sys/types.h /usr/include/time.h
mon-util.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
mon-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
mon-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
mon-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
mon-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
mon-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
mon-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
mon-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
mon-util.o: view.h
mstuff2.o: AppHdr.h beam.h debug.h defines.h effects.h enum.h externs.h
mstuff2.o: fight.h FixAry.h FixVec.h itemname.h items.h liblinux.h libutil.h
mstuff2.o: message.h misc.h monplace.h monstuff.h mon-util.h mstuff2.h
mstuff2.o: player.h spells2.h spells4.h spl-cast.h stuff.h
mstuff2.o: /usr/include/alloca.h /usr/include/asm/errno.h
mstuff2.o: /usr/include/assert.h /usr/include/bits/confname.h
mstuff2.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
mstuff2.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
mstuff2.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
mstuff2.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
mstuff2.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
mstuff2.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
mstuff2.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
mstuff2.o: /usr/include/bits/time.h /usr/include/bits/types.h
mstuff2.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
mstuff2.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
mstuff2.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
mstuff2.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
mstuff2.o: /usr/include/features.h /usr/include/g++-3/alloc.h
mstuff2.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
mstuff2.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
mstuff2.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
mstuff2.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
mstuff2.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
mstuff2.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
mstuff2.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
mstuff2.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
mstuff2.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
mstuff2.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
mstuff2.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
mstuff2.o: /usr/include/g++-3/stl_uninitialized.h
mstuff2.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
mstuff2.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
mstuff2.o: /usr/include/_G_config.h /usr/include/gconv.h
mstuff2.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
mstuff2.o: /usr/include/libio.h /usr/include/limits.h
mstuff2.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
mstuff2.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
mstuff2.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
mstuff2.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
mstuff2.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
mstuff2.o: /usr/include/wchar.h /usr/include/xlocale.h
mstuff2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
mstuff2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
mstuff2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
mstuff2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
mstuff2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
mstuff2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
mstuff2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
mstuff2.o: wpn-misc.h
mutation.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
mutation.o: liblinux.h libutil.h message.h mutation.h player.h skills2.h
mutation.o: stuff.h transfor.h /usr/include/alloca.h /usr/include/asm/errno.h
mutation.o: /usr/include/assert.h /usr/include/bits/confname.h
mutation.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
mutation.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
mutation.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
mutation.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
mutation.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
mutation.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
mutation.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
mutation.o: /usr/include/bits/time.h /usr/include/bits/types.h
mutation.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
mutation.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
mutation.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
mutation.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
mutation.o: /usr/include/features.h /usr/include/g++-3/alloc.h
mutation.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
mutation.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
mutation.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
mutation.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
mutation.o: /usr/include/g++-3/std/bastring.h
mutation.o: /usr/include/g++-3/std/straits.h
mutation.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
mutation.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
mutation.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
mutation.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
mutation.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
mutation.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
mutation.o: /usr/include/g++-3/stl_uninitialized.h
mutation.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
mutation.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
mutation.o: /usr/include/_G_config.h /usr/include/gconv.h
mutation.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
mutation.o: /usr/include/libio.h /usr/include/limits.h
mutation.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
mutation.o: /usr/include/stdlib.h /usr/include/string.h
mutation.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
mutation.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
mutation.o: /usr/include/sys/types.h /usr/include/time.h
mutation.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
mutation.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
mutation.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
mutation.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
mutation.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
mutation.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
mutation.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
mutation.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
mutation.o: view.h
newgame.o: AppHdr.h debug.h defines.h enum.h externs.h fight.h files.h
newgame.o: FixAry.h FixVec.h itemname.h items.h liblinux.h libutil.h
newgame.o: message.h newgame.h player.h randart.h skills2.h skills.h stuff.h
newgame.o: /usr/include/alloca.h /usr/include/asm/errno.h
newgame.o: /usr/include/assert.h /usr/include/bits/confname.h
newgame.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
newgame.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
newgame.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
newgame.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
newgame.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
newgame.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
newgame.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
newgame.o: /usr/include/bits/time.h /usr/include/bits/types.h
newgame.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
newgame.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
newgame.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
newgame.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
newgame.o: /usr/include/features.h /usr/include/g++-3/alloc.h
newgame.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
newgame.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
newgame.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
newgame.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
newgame.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
newgame.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
newgame.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
newgame.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
newgame.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
newgame.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
newgame.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
newgame.o: /usr/include/g++-3/stl_uninitialized.h
newgame.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
newgame.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
newgame.o: /usr/include/_G_config.h /usr/include/gconv.h
newgame.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
newgame.o: /usr/include/libio.h /usr/include/limits.h
newgame.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
newgame.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
newgame.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
newgame.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
newgame.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
newgame.o: /usr/include/wchar.h /usr/include/xlocale.h
newgame.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
newgame.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
newgame.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
newgame.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
newgame.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
newgame.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
newgame.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
newgame.o: version.h wpn-misc.h
ouch.o: AppHdr.h chardump.h debug.h defines.h delay.h enum.h externs.h
ouch.o: files.h FixAry.h FixVec.h hiscores.h invent.h itemname.h items.h
ouch.o: liblinux.h libutil.h message.h mon-util.h ouch.h player.h randart.h
ouch.o: religion.h shopping.h skills2.h stuff.h /usr/include/alloca.h
ouch.o: /usr/include/asm/errno.h /usr/include/assert.h
ouch.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
ouch.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
ouch.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
ouch.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
ouch.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
ouch.o: /usr/include/bits/sched.h /usr/include/bits/select.h
ouch.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
ouch.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
ouch.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
ouch.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
ouch.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
ouch.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
ouch.o: /usr/include/fcntl.h /usr/include/features.h
ouch.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
ouch.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
ouch.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
ouch.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
ouch.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
ouch.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
ouch.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
ouch.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
ouch.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
ouch.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
ouch.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
ouch.o: /usr/include/g++-3/stl_relops.h
ouch.o: /usr/include/g++-3/stl_uninitialized.h
ouch.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
ouch.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
ouch.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
ouch.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
ouch.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
ouch.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
ouch.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
ouch.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
ouch.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
ouch.o: /usr/include/wchar.h /usr/include/xlocale.h
ouch.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
ouch.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
ouch.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
ouch.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
ouch.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
ouch.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
ouch.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
output.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
output.o: itemname.h liblinux.h libutil.h message.h ouch.h output.h player.h
output.o: /usr/include/alloca.h /usr/include/asm/errno.h
output.o: /usr/include/assert.h /usr/include/bits/confname.h
output.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
output.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
output.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
output.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
output.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
output.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
output.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
output.o: /usr/include/bits/time.h /usr/include/bits/types.h
output.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
output.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
output.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
output.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
output.o: /usr/include/features.h /usr/include/g++-3/alloc.h
output.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
output.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
output.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
output.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
output.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
output.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
output.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
output.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
output.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
output.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
output.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
output.o: /usr/include/g++-3/stl_uninitialized.h
output.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
output.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
output.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
output.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
output.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
output.o: /usr/include/stdlib.h /usr/include/string.h
output.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
output.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
output.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
output.o: /usr/include/wchar.h /usr/include/xlocale.h
output.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
output.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
output.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
output.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
output.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
output.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
output.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
overmap.o: AppHdr.h debug.h defines.h enum.h externs.h files.h FixAry.h
overmap.o: FixVec.h liblinux.h libutil.h message.h overmap.h religion.h
overmap.o: stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
overmap.o: /usr/include/assert.h /usr/include/bits/confname.h
overmap.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
overmap.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
overmap.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
overmap.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
overmap.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
overmap.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
overmap.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
overmap.o: /usr/include/bits/time.h /usr/include/bits/types.h
overmap.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
overmap.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
overmap.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
overmap.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
overmap.o: /usr/include/features.h /usr/include/g++-3/alloc.h
overmap.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
overmap.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
overmap.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
overmap.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
overmap.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
overmap.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
overmap.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
overmap.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
overmap.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
overmap.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
overmap.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
overmap.o: /usr/include/g++-3/stl_uninitialized.h
overmap.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
overmap.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
overmap.o: /usr/include/_G_config.h /usr/include/gconv.h
overmap.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
overmap.o: /usr/include/libio.h /usr/include/limits.h
overmap.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
overmap.o: /usr/include/stdlib.h /usr/include/string.h
overmap.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
overmap.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
overmap.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
overmap.o: /usr/include/wchar.h /usr/include/xlocale.h
overmap.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
overmap.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
overmap.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
overmap.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
overmap.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
overmap.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
overmap.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
player.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
player.o: itemname.h liblinux.h libutil.h message.h misc.h mon-util.h
player.o: mutation.h output.h player.h randart.h religion.h skills2.h
player.o: spells4.h spl-util.h stuff.h /usr/include/alloca.h
player.o: /usr/include/asm/errno.h /usr/include/assert.h
player.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
player.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
player.o: /usr/include/bits/fcntl.h /usr/include/bits/huge_val.h
player.o: /usr/include/bits/local_lim.h /usr/include/bits/mathcalls.h
player.o: /usr/include/bits/mathdef.h /usr/include/bits/nan.h
player.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
player.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
player.o: /usr/include/bits/sched.h /usr/include/bits/select.h
player.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
player.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
player.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
player.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
player.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
player.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
player.o: /usr/include/fcntl.h /usr/include/features.h
player.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
player.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
player.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
player.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
player.o: /usr/include/g++-3/std/bastring.cc
player.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
player.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
player.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
player.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
player.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
player.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
player.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
player.o: /usr/include/g++-3/stl_uninitialized.h
player.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
player.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
player.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
player.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
player.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
player.o: /usr/include/math.h /usr/include/stdio.h /usr/include/stdlib.h
player.o: /usr/include/string.h /usr/include/sys/cdefs.h
player.o: /usr/include/sys/select.h /usr/include/sys/stat.h
player.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
player.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
player.o: /usr/include/xlocale.h
player.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
player.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
player.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
player.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
player.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
player.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
player.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
player.o: wpn-misc.h
randart.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
randart.o: itemname.h liblinux.h libutil.h message.h randart.h stuff.h
randart.o: unrand.h /usr/include/alloca.h /usr/include/asm/errno.h
randart.o: /usr/include/assert.h /usr/include/bits/confname.h
randart.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
randart.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
randart.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
randart.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
randart.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
randart.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
randart.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
randart.o: /usr/include/bits/time.h /usr/include/bits/types.h
randart.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
randart.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
randart.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
randart.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
randart.o: /usr/include/features.h /usr/include/g++-3/alloc.h
randart.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
randart.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
randart.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
randart.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
randart.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
randart.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
randart.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
randart.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
randart.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
randart.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
randart.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
randart.o: /usr/include/g++-3/stl_uninitialized.h
randart.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
randart.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
randart.o: /usr/include/_G_config.h /usr/include/gconv.h
randart.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
randart.o: /usr/include/libio.h /usr/include/limits.h
randart.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
randart.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
randart.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
randart.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
randart.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
randart.o: /usr/include/wchar.h /usr/include/xlocale.h
randart.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
randart.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
randart.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
randart.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
randart.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
randart.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
randart.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
randart.o: wpn-misc.h
religion.o: AppHdr.h beam.h debug.h decks.h defines.h describe.h direct.h
religion.o: dungeon.h effects.h enum.h externs.h FixAry.h FixVec.h food.h
religion.o: itemname.h items.h it_use2.h liblinux.h libutil.h message.h
religion.o: misc.h monplace.h mutation.h newgame.h ouch.h player.h religion.h
religion.o: shopping.h spells1.h spells2.h spells3.h spl-cast.h stuff.h
religion.o: /usr/include/alloca.h /usr/include/asm/errno.h
religion.o: /usr/include/assert.h /usr/include/bits/confname.h
religion.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
religion.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
religion.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
religion.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
religion.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
religion.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
religion.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
religion.o: /usr/include/bits/time.h /usr/include/bits/types.h
religion.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
religion.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
religion.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
religion.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
religion.o: /usr/include/features.h /usr/include/g++-3/alloc.h
religion.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
religion.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
religion.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
religion.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
religion.o: /usr/include/g++-3/std/bastring.h
religion.o: /usr/include/g++-3/std/straits.h
religion.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
religion.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
religion.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
religion.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
religion.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
religion.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
religion.o: /usr/include/g++-3/stl_uninitialized.h
religion.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
religion.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
religion.o: /usr/include/_G_config.h /usr/include/gconv.h
religion.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
religion.o: /usr/include/libio.h /usr/include/limits.h
religion.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
religion.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
religion.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
religion.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
religion.o: /usr/include/sys/types.h /usr/include/time.h
religion.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
religion.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
religion.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
religion.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
religion.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
religion.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
religion.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
religion.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
shopping.o: AppHdr.h debug.h defines.h describe.h enum.h externs.h FixAry.h
shopping.o: FixVec.h invent.h itemname.h items.h liblinux.h libutil.h
shopping.o: message.h player.h randart.h shopping.h spl-book.h stuff.h
shopping.o: /usr/include/alloca.h /usr/include/asm/errno.h
shopping.o: /usr/include/assert.h /usr/include/bits/confname.h
shopping.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
shopping.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
shopping.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
shopping.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
shopping.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
shopping.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
shopping.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
shopping.o: /usr/include/bits/time.h /usr/include/bits/types.h
shopping.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
shopping.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
shopping.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
shopping.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
shopping.o: /usr/include/features.h /usr/include/g++-3/alloc.h
shopping.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
shopping.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
shopping.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
shopping.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
shopping.o: /usr/include/g++-3/std/bastring.h
shopping.o: /usr/include/g++-3/std/straits.h
shopping.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
shopping.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
shopping.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
shopping.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
shopping.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
shopping.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
shopping.o: /usr/include/g++-3/stl_uninitialized.h
shopping.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
shopping.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
shopping.o: /usr/include/_G_config.h /usr/include/gconv.h
shopping.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
shopping.o: /usr/include/libio.h /usr/include/limits.h
shopping.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
shopping.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
shopping.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
shopping.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
shopping.o: /usr/include/sys/types.h /usr/include/time.h
shopping.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
shopping.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
shopping.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
shopping.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
shopping.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
shopping.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
shopping.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
shopping.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
skills2.o: AppHdr.h debug.h defines.h enum.h externs.h fight.h FixAry.h
skills2.o: FixVec.h liblinux.h libutil.h message.h player.h randart.h
skills2.o: skills2.h stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
skills2.o: /usr/include/assert.h /usr/include/bits/confname.h
skills2.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
skills2.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
skills2.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
skills2.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
skills2.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
skills2.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
skills2.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
skills2.o: /usr/include/bits/time.h /usr/include/bits/types.h
skills2.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
skills2.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
skills2.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
skills2.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
skills2.o: /usr/include/features.h /usr/include/g++-3/alloc.h
skills2.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
skills2.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
skills2.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
skills2.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
skills2.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
skills2.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
skills2.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
skills2.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
skills2.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
skills2.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
skills2.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
skills2.o: /usr/include/g++-3/stl_uninitialized.h
skills2.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
skills2.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
skills2.o: /usr/include/_G_config.h /usr/include/gconv.h
skills2.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
skills2.o: /usr/include/libio.h /usr/include/limits.h
skills2.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
skills2.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
skills2.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
skills2.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
skills2.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
skills2.o: /usr/include/wchar.h /usr/include/xlocale.h
skills2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
skills2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
skills2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
skills2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
skills2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
skills2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
skills2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
skills2.o: wpn-misc.h
skills.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
skills.o: liblinux.h libutil.h message.h player.h skills2.h skills.h stuff.h
skills.o: /usr/include/alloca.h /usr/include/asm/errno.h
skills.o: /usr/include/assert.h /usr/include/bits/confname.h
skills.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
skills.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
skills.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
skills.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
skills.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
skills.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
skills.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
skills.o: /usr/include/bits/time.h /usr/include/bits/types.h
skills.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
skills.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
skills.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
skills.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
skills.o: /usr/include/features.h /usr/include/g++-3/alloc.h
skills.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
skills.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
skills.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
skills.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
skills.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
skills.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
skills.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
skills.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
skills.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
skills.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
skills.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
skills.o: /usr/include/g++-3/stl_uninitialized.h
skills.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
skills.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
skills.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
skills.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
skills.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
skills.o: /usr/include/stdlib.h /usr/include/string.h
skills.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
skills.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
skills.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
skills.o: /usr/include/wchar.h /usr/include/xlocale.h
skills.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
skills.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
skills.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
skills.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
skills.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
skills.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
skills.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
spells1.o: abyss.h AppHdr.h beam.h cloud.h debug.h defines.h direct.h enum.h
spells1.o: externs.h fight.h FixAry.h FixVec.h invent.h itemname.h it_use2.h
spells1.o: liblinux.h libutil.h message.h misc.h monplace.h monstuff.h
spells1.o: mon-util.h player.h skills2.h spells1.h spells3.h spells4.h
spells1.o: spl-util.h stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
spells1.o: /usr/include/assert.h /usr/include/bits/confname.h
spells1.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
spells1.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
spells1.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
spells1.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
spells1.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
spells1.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
spells1.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
spells1.o: /usr/include/bits/time.h /usr/include/bits/types.h
spells1.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
spells1.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
spells1.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
spells1.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
spells1.o: /usr/include/features.h /usr/include/g++-3/alloc.h
spells1.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
spells1.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
spells1.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
spells1.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
spells1.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
spells1.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
spells1.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
spells1.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
spells1.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
spells1.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
spells1.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
spells1.o: /usr/include/g++-3/stl_uninitialized.h
spells1.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
spells1.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
spells1.o: /usr/include/_G_config.h /usr/include/gconv.h
spells1.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
spells1.o: /usr/include/libio.h /usr/include/limits.h
spells1.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
spells1.o: /usr/include/stdlib.h /usr/include/string.h
spells1.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
spells1.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
spells1.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
spells1.o: /usr/include/wchar.h /usr/include/xlocale.h
spells1.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
spells1.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
spells1.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
spells1.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
spells1.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
spells1.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
spells1.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
spells1.o: wpn-misc.h
spells2.o: AppHdr.h beam.h cloud.h debug.h defines.h direct.h effects.h
spells2.o: enum.h externs.h fight.h FixAry.h FixVec.h itemname.h items.h
spells2.o: liblinux.h libutil.h message.h misc.h monplace.h monstuff.h
spells2.o: mon-util.h ouch.h player.h randart.h spells2.h spells4.h
spells2.o: spl-cast.h stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
spells2.o: /usr/include/assert.h /usr/include/bits/confname.h
spells2.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
spells2.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
spells2.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
spells2.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
spells2.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
spells2.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
spells2.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
spells2.o: /usr/include/bits/time.h /usr/include/bits/types.h
spells2.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
spells2.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
spells2.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
spells2.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
spells2.o: /usr/include/features.h /usr/include/g++-3/alloc.h
spells2.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
spells2.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
spells2.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
spells2.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
spells2.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
spells2.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
spells2.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
spells2.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
spells2.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
spells2.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
spells2.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
spells2.o: /usr/include/g++-3/stl_uninitialized.h
spells2.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
spells2.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
spells2.o: /usr/include/_G_config.h /usr/include/gconv.h
spells2.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
spells2.o: /usr/include/libio.h /usr/include/limits.h
spells2.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
spells2.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
spells2.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
spells2.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
spells2.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
spells2.o: /usr/include/wchar.h /usr/include/xlocale.h
spells2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
spells2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
spells2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
spells2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
spells2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
spells2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
spells2.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
spells2.o: wpn-misc.h
spells3.o: abyss.h AppHdr.h beam.h cloud.h debug.h defines.h delay.h direct.h
spells3.o: enum.h externs.h fight.h FixAry.h FixVec.h itemname.h items.h
spells3.o: it_use2.h liblinux.h libutil.h message.h misc.h mon-pick.h
spells3.o: monplace.h monstuff.h mon-util.h player.h randart.h spells1.h
spells3.o: spells3.h spl-cast.h spl-util.h stuff.h /usr/include/alloca.h
spells3.o: /usr/include/asm/errno.h /usr/include/assert.h
spells3.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
spells3.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
spells3.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
spells3.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
spells3.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
spells3.o: /usr/include/bits/sched.h /usr/include/bits/select.h
spells3.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
spells3.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
spells3.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
spells3.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
spells3.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
spells3.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
spells3.o: /usr/include/fcntl.h /usr/include/features.h
spells3.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
spells3.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
spells3.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
spells3.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
spells3.o: /usr/include/g++-3/std/bastring.cc
spells3.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
spells3.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
spells3.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
spells3.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
spells3.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
spells3.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
spells3.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
spells3.o: /usr/include/g++-3/stl_uninitialized.h
spells3.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
spells3.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
spells3.o: /usr/include/_G_config.h /usr/include/gconv.h
spells3.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
spells3.o: /usr/include/libio.h /usr/include/limits.h
spells3.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
spells3.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
spells3.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
spells3.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
spells3.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
spells3.o: /usr/include/wchar.h /usr/include/xlocale.h
spells3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
spells3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
spells3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
spells3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
spells3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
spells3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
spells3.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
spells3.o: wpn-misc.h
spells4.o: abyss.h AppHdr.h beam.h cloud.h debug.h defines.h delay.h
spells4.o: describe.h direct.h dungeon.h effects.h enum.h externs.h fight.h
spells4.o: FixAry.h FixVec.h itemname.h items.h it_use2.h liblinux.h
spells4.o: libutil.h message.h misc.h monplace.h monstuff.h mon-util.h
spells4.o: mstuff2.h ouch.h player.h randart.h religion.h spells1.h spells4.h
spells4.o: spl-cast.h spl-util.h stuff.h /usr/include/alloca.h
spells4.o: /usr/include/asm/errno.h /usr/include/assert.h
spells4.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
spells4.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
spells4.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
spells4.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
spells4.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
spells4.o: /usr/include/bits/sched.h /usr/include/bits/select.h
spells4.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
spells4.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
spells4.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
spells4.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
spells4.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
spells4.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
spells4.o: /usr/include/fcntl.h /usr/include/features.h
spells4.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
spells4.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
spells4.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
spells4.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
spells4.o: /usr/include/g++-3/std/bastring.cc
spells4.o: /usr/include/g++-3/std/bastring.h /usr/include/g++-3/std/straits.h
spells4.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
spells4.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
spells4.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
spells4.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
spells4.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
spells4.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
spells4.o: /usr/include/g++-3/stl_uninitialized.h
spells4.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
spells4.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
spells4.o: /usr/include/_G_config.h /usr/include/gconv.h
spells4.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
spells4.o: /usr/include/libio.h /usr/include/limits.h
spells4.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
spells4.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
spells4.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
spells4.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
spells4.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
spells4.o: /usr/include/wchar.h /usr/include/xlocale.h
spells4.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
spells4.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
spells4.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
spells4.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
spells4.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
spells4.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
spells4.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
spl-book.o: AppHdr.h debug.h defines.h delay.h enum.h externs.h FixAry.h
spl-book.o: FixVec.h invent.h itemname.h items.h it_use3.h liblinux.h
spl-book.o: libutil.h message.h player.h religion.h spl-book.h spl-cast.h
spl-book.o: spl-util.h stuff.h /usr/include/alloca.h /usr/include/asm/errno.h
spl-book.o: /usr/include/assert.h /usr/include/bits/confname.h
spl-book.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
spl-book.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
spl-book.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
spl-book.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
spl-book.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
spl-book.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
spl-book.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
spl-book.o: /usr/include/bits/time.h /usr/include/bits/types.h
spl-book.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
spl-book.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
spl-book.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
spl-book.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
spl-book.o: /usr/include/features.h /usr/include/g++-3/alloc.h
spl-book.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
spl-book.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
spl-book.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
spl-book.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
spl-book.o: /usr/include/g++-3/std/bastring.h
spl-book.o: /usr/include/g++-3/std/straits.h
spl-book.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
spl-book.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
spl-book.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
spl-book.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
spl-book.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
spl-book.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
spl-book.o: /usr/include/g++-3/stl_uninitialized.h
spl-book.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
spl-book.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
spl-book.o: /usr/include/_G_config.h /usr/include/gconv.h
spl-book.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
spl-book.o: /usr/include/libio.h /usr/include/limits.h
spl-book.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
spl-book.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
spl-book.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
spl-book.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
spl-book.o: /usr/include/sys/types.h /usr/include/time.h
spl-book.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
spl-book.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
spl-book.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
spl-book.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
spl-book.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
spl-book.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
spl-book.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
spl-book.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
spl-cast.o: AppHdr.h beam.h cloud.h debug.h defines.h direct.h effects.h
spl-cast.o: enum.h externs.h fight.h FixAry.h FixVec.h food.h itemname.h
spl-cast.o: it_use2.h liblinux.h libutil.h message.h monplace.h monstuff.h
spl-cast.o: mutation.h ouch.h player.h religion.h skills.h spells1.h
spl-cast.o: spells2.h spells3.h spells4.h spl-cast.h spl-util.h stuff.h
spl-cast.o: transfor.h /usr/include/alloca.h /usr/include/asm/errno.h
spl-cast.o: /usr/include/assert.h /usr/include/bits/confname.h
spl-cast.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
spl-cast.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
spl-cast.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
spl-cast.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
spl-cast.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
spl-cast.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
spl-cast.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
spl-cast.o: /usr/include/bits/time.h /usr/include/bits/types.h
spl-cast.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
spl-cast.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
spl-cast.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
spl-cast.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
spl-cast.o: /usr/include/features.h /usr/include/g++-3/alloc.h
spl-cast.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
spl-cast.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
spl-cast.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
spl-cast.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
spl-cast.o: /usr/include/g++-3/std/bastring.h
spl-cast.o: /usr/include/g++-3/std/straits.h
spl-cast.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
spl-cast.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
spl-cast.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
spl-cast.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
spl-cast.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
spl-cast.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
spl-cast.o: /usr/include/g++-3/stl_uninitialized.h
spl-cast.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
spl-cast.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
spl-cast.o: /usr/include/_G_config.h /usr/include/gconv.h
spl-cast.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
spl-cast.o: /usr/include/libio.h /usr/include/limits.h
spl-cast.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
spl-cast.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
spl-cast.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
spl-cast.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
spl-cast.o: /usr/include/sys/types.h /usr/include/time.h
spl-cast.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
spl-cast.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
spl-cast.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
spl-cast.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
spl-cast.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
spl-cast.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
spl-cast.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
spl-cast.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
spl-cast.o: view.h
spl-util.o: AppHdr.h debug.h defines.h direct.h enum.h externs.h FixAry.h
spl-util.o: FixVec.h itemname.h liblinux.h libutil.h message.h player.h
spl-util.o: spl-book.h spl-data.h spl-util.h stuff.h /usr/include/alloca.h
spl-util.o: /usr/include/asm/errno.h /usr/include/assert.h
spl-util.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
spl-util.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
spl-util.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
spl-util.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
spl-util.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
spl-util.o: /usr/include/bits/sched.h /usr/include/bits/select.h
spl-util.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
spl-util.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
spl-util.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
spl-util.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
spl-util.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
spl-util.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
spl-util.o: /usr/include/fcntl.h /usr/include/features.h
spl-util.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
spl-util.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
spl-util.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
spl-util.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
spl-util.o: /usr/include/g++-3/std/bastring.cc
spl-util.o: /usr/include/g++-3/std/bastring.h
spl-util.o: /usr/include/g++-3/std/straits.h
spl-util.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
spl-util.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
spl-util.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
spl-util.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
spl-util.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
spl-util.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
spl-util.o: /usr/include/g++-3/stl_uninitialized.h
spl-util.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
spl-util.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
spl-util.o: /usr/include/_G_config.h /usr/include/gconv.h
spl-util.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
spl-util.o: /usr/include/libio.h /usr/include/limits.h
spl-util.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
spl-util.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
spl-util.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
spl-util.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
spl-util.o: /usr/include/sys/types.h /usr/include/time.h
spl-util.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
spl-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
spl-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
spl-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
spl-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
spl-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
spl-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
spl-util.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
spl-util.o: view.h
stuff.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
stuff.o: liblinux.h libutil.h message.h misc.h output.h skills2.h stuff.h
stuff.o: /usr/include/alloca.h /usr/include/asm/errno.h /usr/include/assert.h
stuff.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
stuff.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
stuff.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
stuff.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
stuff.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
stuff.o: /usr/include/bits/sched.h /usr/include/bits/select.h
stuff.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
stuff.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
stuff.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
stuff.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
stuff.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
stuff.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
stuff.o: /usr/include/fcntl.h /usr/include/features.h
stuff.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
stuff.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
stuff.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
stuff.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
stuff.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
stuff.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
stuff.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
stuff.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
stuff.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
stuff.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
stuff.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
stuff.o: /usr/include/g++-3/stl_relops.h
stuff.o: /usr/include/g++-3/stl_uninitialized.h
stuff.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
stuff.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
stuff.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
stuff.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
stuff.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
stuff.o: /usr/include/stdlib.h /usr/include/string.h /usr/include/sys/cdefs.h
stuff.o: /usr/include/sys/select.h /usr/include/sys/stat.h
stuff.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
stuff.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
stuff.o: /usr/include/xlocale.h
stuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
stuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
stuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
stuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
stuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
stuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
stuff.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
tags.o: AppHdr.h debug.h defines.h enum.h externs.h files.h FixAry.h FixVec.h
tags.o: itemname.h liblinux.h libutil.h message.h monstuff.h mon-util.h
tags.o: randart.h skills.h stuff.h tags.h /usr/include/alloca.h
tags.o: /usr/include/asm/errno.h /usr/include/assert.h
tags.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
tags.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
tags.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
tags.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
tags.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
tags.o: /usr/include/bits/sched.h /usr/include/bits/select.h
tags.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
tags.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
tags.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
tags.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
tags.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
tags.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
tags.o: /usr/include/fcntl.h /usr/include/features.h
tags.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
tags.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
tags.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
tags.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
tags.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
tags.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
tags.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
tags.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
tags.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
tags.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
tags.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
tags.o: /usr/include/g++-3/stl_relops.h
tags.o: /usr/include/g++-3/stl_uninitialized.h
tags.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
tags.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
tags.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
tags.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
tags.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
tags.o: /usr/include/stdio.h /usr/include/stdlib.h /usr/include/string.h
tags.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
tags.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
tags.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/unistd.h
tags.o: /usr/include/wchar.h /usr/include/xlocale.h
tags.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
tags.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
tags.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
tags.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
tags.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
tags.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
tags.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
transfor.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
transfor.o: itemname.h items.h it_use2.h liblinux.h libutil.h message.h
transfor.o: misc.h player.h skills2.h stuff.h transfor.h
transfor.o: /usr/include/alloca.h /usr/include/asm/errno.h
transfor.o: /usr/include/assert.h /usr/include/bits/confname.h
transfor.o: /usr/include/bits/endian.h /usr/include/bits/environments.h
transfor.o: /usr/include/bits/errno.h /usr/include/bits/fcntl.h
transfor.o: /usr/include/bits/local_lim.h /usr/include/bits/posix1_lim.h
transfor.o: /usr/include/bits/posix2_lim.h /usr/include/bits/posix_opt.h
transfor.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/sched.h
transfor.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
transfor.o: /usr/include/bits/stat.h /usr/include/bits/stdio_lim.h
transfor.o: /usr/include/bits/time.h /usr/include/bits/types.h
transfor.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
transfor.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
transfor.o: /usr/include/bits/xopen_lim.h /usr/include/ctype.h
transfor.o: /usr/include/endian.h /usr/include/errno.h /usr/include/fcntl.h
transfor.o: /usr/include/features.h /usr/include/g++-3/alloc.h
transfor.o: /usr/include/g++-3/cassert /usr/include/g++-3/cctype
transfor.o: /usr/include/g++-3/cstddef /usr/include/g++-3/cstring
transfor.o: /usr/include/g++-3/iostream.h /usr/include/g++-3/iterator
transfor.o: /usr/include/g++-3/queue /usr/include/g++-3/std/bastring.cc
transfor.o: /usr/include/g++-3/std/bastring.h
transfor.o: /usr/include/g++-3/std/straits.h
transfor.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
transfor.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
transfor.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
transfor.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
transfor.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
transfor.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
transfor.o: /usr/include/g++-3/stl_uninitialized.h
transfor.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
transfor.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
transfor.o: /usr/include/_G_config.h /usr/include/gconv.h
transfor.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
transfor.o: /usr/include/libio.h /usr/include/limits.h
transfor.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
transfor.o: /usr/include/stdlib.h /usr/include/string.h
transfor.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
transfor.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
transfor.o: /usr/include/sys/types.h /usr/include/time.h
transfor.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
transfor.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
transfor.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
transfor.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
transfor.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
transfor.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
transfor.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
transfor.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
view.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
view.o: insult.h liblinux.h libutil.h message.h monstuff.h mon-util.h
view.o: overmap.h player.h spells4.h stuff.h /usr/include/alloca.h
view.o: /usr/include/asm/errno.h /usr/include/assert.h
view.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
view.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
view.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
view.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
view.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
view.o: /usr/include/bits/sched.h /usr/include/bits/select.h
view.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
view.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
view.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
view.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
view.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
view.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
view.o: /usr/include/fcntl.h /usr/include/features.h
view.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
view.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
view.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
view.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
view.o: /usr/include/g++-3/std/bastring.cc /usr/include/g++-3/std/bastring.h
view.o: /usr/include/g++-3/std/straits.h /usr/include/g++-3/stl_algobase.h
view.o: /usr/include/g++-3/stl_alloc.h /usr/include/g++-3/stl_bvector.h
view.o: /usr/include/g++-3/stl_config.h /usr/include/g++-3/stl_construct.h
view.o: /usr/include/g++-3/stl_deque.h /usr/include/g++-3/stl_function.h
view.o: /usr/include/g++-3/stl_heap.h /usr/include/g++-3/stl_iterator.h
view.o: /usr/include/g++-3/stl_pair.h /usr/include/g++-3/stl_queue.h
view.o: /usr/include/g++-3/stl_relops.h
view.o: /usr/include/g++-3/stl_uninitialized.h
view.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
view.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
view.o: /usr/include/_G_config.h /usr/include/gconv.h /usr/include/getopt.h
view.o: /usr/include/gnu/stubs.h /usr/include/libio.h /usr/include/limits.h
view.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
view.o: /usr/include/stdlib.h /usr/include/string.h /usr/include/sys/cdefs.h
view.o: /usr/include/sys/select.h /usr/include/sys/stat.h
view.o: /usr/include/sys/sysmacros.h /usr/include/sys/types.h
view.o: /usr/include/time.h /usr/include/unistd.h /usr/include/wchar.h
view.o: /usr/include/xlocale.h
view.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
view.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
view.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
view.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
view.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
view.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
view.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h view.h
wpn-misc.o: AppHdr.h debug.h defines.h enum.h externs.h FixAry.h FixVec.h
wpn-misc.o: liblinux.h libutil.h message.h /usr/include/alloca.h
wpn-misc.o: /usr/include/asm/errno.h /usr/include/assert.h
wpn-misc.o: /usr/include/bits/confname.h /usr/include/bits/endian.h
wpn-misc.o: /usr/include/bits/environments.h /usr/include/bits/errno.h
wpn-misc.o: /usr/include/bits/fcntl.h /usr/include/bits/local_lim.h
wpn-misc.o: /usr/include/bits/posix1_lim.h /usr/include/bits/posix2_lim.h
wpn-misc.o: /usr/include/bits/posix_opt.h /usr/include/bits/pthreadtypes.h
wpn-misc.o: /usr/include/bits/sched.h /usr/include/bits/select.h
wpn-misc.o: /usr/include/bits/sigset.h /usr/include/bits/stat.h
wpn-misc.o: /usr/include/bits/stdio_lim.h /usr/include/bits/time.h
wpn-misc.o: /usr/include/bits/types.h /usr/include/bits/waitflags.h
wpn-misc.o: /usr/include/bits/waitstatus.h /usr/include/bits/wchar.h
wpn-misc.o: /usr/include/bits/wordsize.h /usr/include/bits/xopen_lim.h
wpn-misc.o: /usr/include/ctype.h /usr/include/endian.h /usr/include/errno.h
wpn-misc.o: /usr/include/fcntl.h /usr/include/features.h
wpn-misc.o: /usr/include/g++-3/alloc.h /usr/include/g++-3/cassert
wpn-misc.o: /usr/include/g++-3/cctype /usr/include/g++-3/cstddef
wpn-misc.o: /usr/include/g++-3/cstring /usr/include/g++-3/iostream.h
wpn-misc.o: /usr/include/g++-3/iterator /usr/include/g++-3/queue
wpn-misc.o: /usr/include/g++-3/std/bastring.cc
wpn-misc.o: /usr/include/g++-3/std/bastring.h
wpn-misc.o: /usr/include/g++-3/std/straits.h
wpn-misc.o: /usr/include/g++-3/stl_algobase.h /usr/include/g++-3/stl_alloc.h
wpn-misc.o: /usr/include/g++-3/stl_bvector.h /usr/include/g++-3/stl_config.h
wpn-misc.o: /usr/include/g++-3/stl_construct.h /usr/include/g++-3/stl_deque.h
wpn-misc.o: /usr/include/g++-3/stl_function.h /usr/include/g++-3/stl_heap.h
wpn-misc.o: /usr/include/g++-3/stl_iterator.h /usr/include/g++-3/stl_pair.h
wpn-misc.o: /usr/include/g++-3/stl_queue.h /usr/include/g++-3/stl_relops.h
wpn-misc.o: /usr/include/g++-3/stl_uninitialized.h
wpn-misc.o: /usr/include/g++-3/stl_vector.h /usr/include/g++-3/streambuf.h
wpn-misc.o: /usr/include/g++-3/string /usr/include/g++-3/type_traits.h
wpn-misc.o: /usr/include/_G_config.h /usr/include/gconv.h
wpn-misc.o: /usr/include/getopt.h /usr/include/gnu/stubs.h
wpn-misc.o: /usr/include/libio.h /usr/include/limits.h
wpn-misc.o: /usr/include/linux/errno.h /usr/include/linux/limits.h
wpn-misc.o: /usr/include/stdlib.h /usr/include/string.h
wpn-misc.o: /usr/include/sys/cdefs.h /usr/include/sys/select.h
wpn-misc.o: /usr/include/sys/stat.h /usr/include/sys/sysmacros.h
wpn-misc.o: /usr/include/sys/types.h /usr/include/time.h
wpn-misc.o: /usr/include/unistd.h /usr/include/wchar.h /usr/include/xlocale.h
wpn-misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/exception
wpn-misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/limits.h
wpn-misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new
wpn-misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/new.h
wpn-misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stdarg.h
wpn-misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/stddef.h
wpn-misc.o: /usr/lib/gcc-lib/i386-redhat-linux/2.96/include/syslimits.h
wpn-misc.o: wpn-misc.h
