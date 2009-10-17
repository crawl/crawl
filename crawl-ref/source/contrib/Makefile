uname_S := $(shell uname -s)
PREFIX:=$(shell pwd)/install

all: lua sqlite sdl sdl-image freetype libpng pcre zlib

.PHONY: lua sqlite sdl sdl-image freetype libpng pcre zlib

# Only clean submodules we actually posses.  Some makefiles don't have
# 'distclean' target, so try both 'clean' and 'distclean', and discard
# any errors.
clean:
	rm -rf $(PREFIX)
	@for a in freetype libpng lua sdl sdl-image sqlite pcre zlib; do \
		if [ -f $$a/Makefile ] || [ -f $$a/makefile ] ; then \
			make -k -C $$a clean distclean 2> /dev/null; \
		fi \
	done

#
# Check for GNU Make >=3.80
#
eval_available :=
$(eval eval_available := T)
ifneq ($(eval_available),T)
ifneq (,$(findstring MINGW,$(uname_S)))
  $(error FreeType needs GNU Make 3.80 or later. Get this from http://prdownloads.sourceforge.net/mingw/make-3.81-MSYS-1.0.11-2.tar.bz2)
else
  $(error FreeType's build system needs GNU Make 3.80 or later)
endif
endif


#
# FreeType 2
#
freetype/objs/.libs/libfreetype.a: $(PREFIX)/lib/libz.a
	cd freetype && ./configure --prefix=$(PREFIX) --with-zlib=$(PREFIX) --enable-static --disable-shared
	@+$(MAKE) -C freetype

$(PREFIX)/lib/libfreetype.a: freetype/objs/.libs/libfreetype.a
	@$(MAKE) -C freetype install

freetype: $(PREFIX)/lib/libfreetype.a


#
# Lua
#
lua/src/liblua.a:
	@+$(MAKE) -C lua/src crawl_unix

$(PREFIX)/lib/liblua.a: lua/src/liblua.a
	@$(MAKE) -C lua/src install prefix=$(PREFIX)

lua: $(PREFIX)/lib/liblua.a


#
# SQLite
#
sqlite/libsqlite3.a:
	@+$(MAKE) -C sqlite

$(PREFIX)/lib/libsqlite3.a: sqlite/libsqlite3.a
	@$(MAKE) -C sqlite install prefix=$(PREFIX)

sqlite: $(PREFIX)/lib/libsqlite3.a


#
# PCRE
#
pcre/libpcre.a:
	@+$(MAKE) -C pcre

$(PREFIX)/lib/libpcre.a: pcre/libpcre.a
	@$(MAKE) -C pcre install prefix=$(PREFIX)

pcre: $(PREFIX)/lib/libpcre.a


#
# libpng
#
libpng/.libs/libpng.a: $(PREFIX)/lib/libz.a
	cd libpng && ./configure --prefix=$(PREFIX) --enable-static --disable-shared LDFLAGS="-L$(PREFIX)/lib"
	@+$(MAKE) -C libpng

$(PREFIX)/lib/libpng.a: libpng/.libs/libpng.a
	@$(MAKE) -C libpng install

libpng: $(PREFIX)/lib/libpng.a


#
# SDL
#
sdl/build/.libs/libSDL.a:
	cd sdl && ./configure --prefix=$(PREFIX) --enable-static --disable-shared
	@+$(MAKE) -C sdl

$(PREFIX)/lib/libSDL.a: sdl/build/.libs/libSDL.a
	@$(MAKE) -C sdl install
ifneq (,$(findstring MINGW,$(uname_S)))
#
# Very hackish fix for MinGW
	sed 's/-mwindows/-lwinmm -lpng -lz -mwindows/' install/bin/sdl-config > install/bin/sdl-config.fixed
	mv install/bin/sdl-config.fixed install/bin/sdl-config
	chmod 755 install/bin/sdl-config
endif

sdl: $(PREFIX)/lib/libSDL.a


#
# SDL_image
#
sdl-image/.libs/libSDL_image.a: $(PREFIX)/lib/libpng.a
	cd sdl-image && ./configure --prefix=$(PREFIX) --with-sdl-prefix=$(PREFIX) --enable-static --disable-shared --enable-bmp --enable-png LDFLAGS="-L$(PREFIX)/lib -lpng -lz" CPPFLAGS="-I$(PREFIX)/include"
	@+$(MAKE) -C sdl-image

$(PREFIX)/lib/libSDL_image.a: sdl-image/.libs/libSDL_image.a
	@$(MAKE) -C sdl-image install

sdl-image: $(PREFIX)/lib/libSDL_image.a


#
# zlib
#
zlib/libz.a:
	cd zlib && ./configure --prefix=$(PREFIX)
	@+$(MAKE) -C zlib

$(PREFIX)/lib/libz.a: zlib/libz.a
	@$(MAKE) -C zlib install

zlib: $(PREFIX)/lib/libz.a
