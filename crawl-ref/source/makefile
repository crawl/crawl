#
# Dungeon Crawl Stone Soup
# GNU Makefile
#
# largely written by Steven Noonan <steven@uplinklabs.net>
#    (if something breaks, blame him.)
#

GAME = crawl

#
# Compiler Flags
#
# The compiler flag variables are separated into their individual
# purposes, making it easier to deal with the various tools involved
# in a compile.
#
# These are also divided into global vs. local flags. So for instance,
# CFOPTIMIZE affects Crawl, Lua, and SQLite, while CFOPTIMIZE_L only
# affects Crawl.
#
# The variables are as follows:
# CFOPTIMIZE(_L) - Optimization flags
# CFWARN(_L) - Warning flags
# CFOTHERS(_L) - Anything else
#

# These are really good options for the Intel C++ compiler:
#CFOPTIMIZE := -O2 -parallel

CFOPTIMIZE := -O2
CFOTHERS := -fno-strict-aliasing -pipe $(EXTERNAL_FLAGS)
CFOTHERS_L := -fsigned-char
CFWARN := -Wall
MAKEFLAGS := --no-print-directory

#
# The GCC and GXX variables are set later.
#
AR = ar
RANLIB = ranlib
CC = $(GCC)
CXX = $(GXX)
RM = rm -f
COPY = cp

export AR
export RANLIB
export RM
export CC
export CXX
export CFLAGS
export MAKEFLAGS
export CONFIGURE_FLAGS
export uname_S

LIBPCRE := contrib/install/lib/libpcre.a
LIBSDL := contrib/install/lib/libSDL.a
LIBSDLIMAGE := contrib/install/lib/libSDL_image.a
LIBFREETYPE := contrib/install/lib/libfreetype.a
LIBSQLITE := contrib/install/lib/libsqlite3.a
LIBLUA := contrib/install/lib/liblua.a
LIBZ := contrib/install/lib/libz.a

#
# Platform Detection
#
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')
uname_O := $(shell sh -c 'uname -o 2>/dev/null || echo not')
uname_R := $(shell sh -c 'uname -r 2>/dev/null || echo not')
uname_P := $(shell sh -c 'uname -p 2>/dev/null || echo not')
uname_V := $(shell sh -c 'uname -v 2>/dev/null || echo not')

ifdef CROSSHOST
	NO_PKGCONFIG = YesPlease
	NO_SDLCONFIG = YesPlease
	NO_AUTO_OPT = YesPlease
	NEED_STATIC = YesPlease
	CONFIGURE_FLAGS += --host=$(CROSSHOST)
	SDLCONFIG := $(shell pwd)/contrib/install/bin/sdl-config

	# If needed, override uname_S so we get the appropriate
	# things compiled.
	ifneq (,$(findstring mingw,$(CROSSHOST)))
		uname_S=MINGW32
	endif

endif
ifneq (,$(findstring MINGW,$(uname_S)))
	GAME = crawl.exe
	WIN32 = Yes
	NO_RDYNAMIC = YesPlease
	NO_NCURSES = YesPlease
	NEEDS_LIBW32C = YesPlease
	NEEDS_PCRE = YesPlease
	ifdef TILES
		NEEDS_FREETYPE = YesPlease
		NEEDS_SDL = YesPlease
		NEEDS_SDLIMAGE = YesPlease
		NEEDS_LIBPNG = YesPlease
		NEEDS_ZLIB = YesPlease
	endif
endif
ifneq (,$(findstring CYGWIN,$(uname_S)))
	GAME = crawl.exe
	NO_RDYNAMIC = YesPlease
	NEEDS_PCRE = YesPlease
endif
ifeq ($(uname_S),Darwin)
	ifndef NO_APPLE_GCC
		APPLE_GCC = YesPlease
	endif
	NEED_APPKIT = YesPlease
	LIBNCURSES_IS_UNICODE = Yes
endif

#
# Set up object file dependencies for $(GAME) target.
#
include makefile.obj

# Works for Mac OS X and Linux.
OBJECTS += crash-u.o

ifdef TILES
OBJECTS += libgui.o tile2.o tilereg.o tilepick.o tilesdl.o tilefont.o tiletex.o tilemcache.o tilebuf.o
else
ifdef NEEDS_LIBW32C
OBJECTS += libw32c.o
else
OBJECTS += libunix.o
endif
endif

# To get stack trace symbols.
# Note that MinGW doesn't support -rdynamic.
ifndef NO_RDYNAMIC
LDFLAGS := -rdynamic
endif
ifdef NEED_STATIC
LDFLAGS += -static
endif

# Permissions to set on the game executable.
MCHMOD := 2755

# Permissions to set on the save directory.
MCHMOD_SAVEDIR := 775

# The user:group to install the game as.
INSTALL_UGRP := games:games

prefix   := /usr/games/crawl

# If you're installing Crawl for multiple users, you *must* set this to a
# valid path before building Crawl. This is not necessary if you are building
# Crawl for a single user.

# SAVEDIR := saves/
# DATADIR := data/

INCLUDES_L += -Icontrib/install/include
LIB += -Lcontrib/install/lib

INCLUDES_L += -Iutil -I.

ifdef APPLE_GCC

# Compatibility level for Mac OS X
#
SDK_VER := 10.4
GCC_VER := 4.0

# FIXME: The '-arch i386' is only here until
#   SDL 1.2.14 is available
#
ARCHS := -arch i386

# Mac OS X 10.4 adds a 'u' on the end of the SDK name. Everything
# else is much easier to predict the name of.
ifeq ($(SDK_VER),10.4)
SDKROOT := /Developer/SDKs/MacOSX$(SDK_VER)u.sdk
else
SDKROOT := /Developer/SDKs/MacOSX$(SDK_VER).sdk
endif

CC = $(GCC) $(ARCHS) -isysroot $(SDKROOT) -mmacosx-version-min=$(SDK_VER)
CXX = $(GXX) $(ARCHS) -isysroot $(SDKROOT) -mmacosx-version-min=$(SDK_VER)

endif # MacOS

ifndef CROSSHOST

ifneq ($(GCC_VER),)
# We do this in a separate variable because if we
# specify GCC_VER on the make command-line, the
# variable is immutable, and we can't add the dash.
GCC_VER_SUFFIX:=-$(GCC_VER)
endif

# Attempt to use a full compiler name, to make
# distcc builds work nicely.
LMACH := $(shell gcc -dumpmachine)-
ifeq ($(LMACH),-)
LMACH :=
endif
ifeq ($(shell which $(LMACH)gcc$(GCC_VER_SUFFIX) 2> /dev/null),)
LMACH :=
endif

GCC := $(LMACH)gcc$(GCC_VER_SUFFIX)
GXX := $(LMACH)g++$(GCC_VER_SUFFIX)

else

# Cross-compiling is a weird case.
GCC := $(CROSSHOST)-gcc
GXX := $(CROSSHOST)-g++
AR := $(CROSSHOST)-ar
RANLIB := $(CROSSHOST)-ranlib

endif
GCC_GTE_4_0_0 := $(shell util/gcc-gte.pl $(GCC) 4.0.0)
GCC_GTE_4_3_0 := $(shell util/gcc-gte.pl $(GCC) 4.3.0)

# Define this to automatically generate code optimized for your machine
# (GCC only as of now).
#
# NOTE: Don't use this with a release build, since the generated code
# won't work for all machines.
ifdef HURRY
NO_AUTO_OPT = YesPlease
endif

ifdef AUTO_OPT
ifndef NO_AUTO_OPT
CFOPTIMIZE += $(shell util/gcc-opt.pl $(GCC))
endif
endif

RLTILES = rltiles

#
# Tiles build stuff
#
ifdef TILES

ifeq ($(uname_S),Darwin)
ifndef UNSUPPORTED_BUILD
$(warning Using this makefile to build a tiles build on Mac OS X is NOT OFFICIALLY SUPPORTED.)
$(warning You should have no expectation of stability if you choose to continue anyway!)
$(warning Please use the Xcode project instead.)
$(error To build anyway, add the flag UNSUPPORTED_BUILD=y to the command-line)
endif
endif

DEFINES_L += -DUSE_TILE
INCLUDES_L += -I$(RLTILES)

# Okay, we have to assume we're on something else that
# uses standard UNIX-like methods for finding libs.
#
# For instance, on Linux and most other UNIX-likes,
# the app pkg-config can provide the appropriate
# CFLAGS and LDFLAGS.
#

ifneq (,$(findstring MINGW,$(uname_S)))
CONTRIB_DEPENDS += $(LIBSDL) $(LIBSDLIMAGE) $(LIBFREETYPE)
endif

ifndef NO_PKGCONFIG
ifneq ($(shell which pkg-config 2> /dev/null),)
PKGCONFIG = YesPlease
endif
endif

ifdef PKGCONFIG

# If pkg-config is available, it's the surest way to find where
# the contributing libraries are located.
#

PNG_INCLUDE := $(shell pkg-config libpng --cflags-only-I)
PNG_CFLAGS  := $(shell pkg-config libpng --cflags-only-other)
PNG_LDFLAGS := $(shell pkg-config libpng --libs-only-L) $(shell pkg-config libpng --libs-only-l)

FREETYPE_INCLUDE := $(shell pkg-config freetype2 --cflags-only-I)
FREETYPE_CFLAGS  := $(shell pkg-config freetype2 --cflags-only-other)
FREETYPE_LDFLAGS := $(shell pkg-config freetype2 --libs-only-L) $(shell pkg-config freetype2 --libs-only-l)

SDL_INCLUDE := $(shell pkg-config sdl --cflags-only-I)
SDL_CFLAGS  := $(shell pkg-config sdl --cflags-only-other)
SDL_LDFLAGS := $(shell pkg-config sdl --libs-only-L) $(shell pkg-config sdl --libs-only-l)

else # pkg-config

# Ugh, pkg-config isn't available. We're probably on MinGW, Cygwin,
# or some other stupid platform. We've got to assume that the
# libraries are either in the standard paths or are in /opt/<lib>/.
#

PNG_INCLUDE := -I/opt/libpng/include
PNG_CFLAGS  :=
PNG_LDFLAGS := -L/opt/libpng/lib -lpng -lz

FREETYPE_INCLUDE := -I/opt/freetype/include -Icontrib/install/include/freetype2
FREETYPE_CFLAGS  :=
FREETYPE_LDFLAGS := -L/opt/freetype/lib -lfreetype

ifndef NO_SDLCONFIG
SDLCONFIG := $(which sdl-config)
ifeq ($(SDLCONFIG),)
SDLCONFIG := $(shell pwd)/contrib/install/bin/sdl-config
endif
endif

ifneq ($(SDLCONFIG),)

# sdl-config is our second best bet...
#
SDL_INCLUDE := $(shell $(SDLCONFIG) --cflags | tr ' ' '\n' | grep ^[-]I | tr '\n' ' ')
SDL_CFLAGS  := $(shell $(SDLCONFIG) --cflags | tr ' ' '\n' | grep -v ^[-]I | tr '\n' ' ')
SDL_LDFLAGS := $(shell $(SDLCONFIG) --libs)

else # sdl-config

# Fine, we have to make some REALLY ugly assumptions...
#
SDL_INCLUDE := -I/opt/sdl/include/SDL
SDL_CFLAGS  :=
SDL_LDFLAGS := -L/opt/sdl/lib -lSDL -lSDLmain

endif # sdl-config
endif # pkg-config

LIB += $(SDL_LDFLAGS) -lSDL_image $(PNG_LDFLAGS) $(FREETYPE_LDFLAGS) -lz
ifneq ($(OSNAME),MacOS)
ifeq (,$(findstring MINGW,$(uname_S)))
LIB += -lGL -lGLU
else
LIB += -lopengl32 -lglu32
endif
endif

DEFINES_L += $(PNG_CFLAGS) $(FREETYPE_CFLAGS) $(SDL_CFLAGS)
INCLUDES_L += $(PNG_INCLUDE) $(FREETYPE_INCLUDE) $(SDL_INCLUDE)

endif # TILES

ifeq ($(GCC_GTE_4_3_0),1)
CFWARN_L += -Wno-array-bounds
endif

CFWARN_L += -Wno-parentheses -Wwrite-strings -Wshadow -pedantic -D_FORTIFY_SOURCE=0
CFOTHERS_L = $(EXTERNAL_FLAGS_L) $(EXTRA_FLAGS) $(DEFINES) $(SDL_CFLAGS)

ifndef NO_LUA_BINDINGS
CFOTHERS_L += -DCLUA_BINDINGS
endif

#
# Figure out the build settings for this type of build
#

# Debug
# No optimization, full debugging.
ifeq ($(MAKECMDGOALS),debug)
	FULLDEBUG=YesPlease
	WIZARD=YesPlease
	DEBUG=YesPlease
	NO_OPTIMIZE=YesPlease
endif

# Wizard
# Optimized, with wizard mode.
ifeq ($(MAKECMDGOALS),wizard)
	WIZARD=YesPlease
	DEBUG=YesPlease
endif

# Profile
# Optimized, with full debugging.
ifeq ($(MAKECMDGOALS),profile)
	FULLDEBUG=YesPlease
	WIZARD=YesPlease
	DEBUG=YesPlease
endif

ifdef HURRY
	NO_OPTIMIZE=YesPlease
endif

ifdef FULLDEBUG
DEFINES += -DFULLDEBUG
endif
ifdef DEBUG
EXTRA_FLAGS += -ggdb
DEFINES += -DDEBUG
endif
ifdef WIZARD
DEFINES += -DWIZARD
endif
ifdef NO_OPTIMIZE
CFOPTIMIZE  := -O0
endif
ifdef PCH
CFWARN_L += -Winvalid-pch
endif

# Cygwin has a panic attack if we do this...
ifndef NO_OPTIMIZE
ifneq ($(GCC_GTE_4_0_0),0)
CFWARN_L += -Wuninitialized
else
CFWARN_L += -Wno-uninitialized
endif
endif

ifneq ($(strip $(SAVEDIR)),)
CFOTHERS_L += '-DSAVE_DIR_PATH="$(strip $(prefix))/$(strip $(SAVEDIR))"'
endif

ifneq ($(strip $(DATADIR)),)
CFOTHERS_L += '-DDATA_DIR_PATH="$(strip $(prefix))/$(strip $(DATADIR))"'
endif

ifndef NO_NCURSES
# Usually, it can be autodetected for you:
ifneq ($(shell ls /usr/include/ncursesw 2> /dev/null),)
USE_UNICODE = YesPlease
endif

# If you have USE_UNICODE set, and have a preferred Unicode
# (UTF-8) locale you want Crawl to use, you can set it here. The
# default is en_US.UTF-8. If you'd prefer that Crawl use the locale
# as set in your environment LC_* variables, use UNICODE_LOCALE = .
UNICODE_LOCALE =

ifdef USE_UNICODE
# Include path for (n)curses with Unicode support.
INCLUDES_L += -I/usr/include/ncursesw

# Your ncurses library may include Unicode support, and you may not have a
# separate libncursesw; in that case, change this line accordingly.
LIBCURS  = ncursesw
CFOTHERS_L += -DUNICODE_GLYPHS

ifneq ($(strip $(UNICODE_LOCALE)),)
ifneq ($(strip $(UNICODE_LOCALE)),.)
CFOTHERS_L += -DUNICODE_LOCALE=\"$(strip $(UNICODE_LOCALE))\"
else
CFOTHERS_L += -DUNICODE_LOCALE=\"\"
endif
endif

# The standard ncurses library also supports Unicode on Mac OS/Darwin.
ifdef LIBNCURSES_IS_UNICODE
LIBCURS = ncurses
endif

LIB += -l$(LIBCURS)

else
# Include path for curses or ncurses (non-Unicode).
INCLUDES_L += -I/usr/include/ncurses
LIB += -lncurses
endif
endif

ifdef NEEDS_PCRE
DEFINES += -DREGEX_PCRE
LIB += -lpcre
endif

ifdef USE_ICC
GCC := icc
GXX := icpc
AR  := xiar rcu
LIB += -lguide -lpthread
CFWARN := -wd383,810,869,981,1418 -we14,193,304
CFWARN_L :=
endif

ifdef REPORT
CFOTHERS += -ftime-report
endif

CFLAGS   := $(CFOPTIMIZE) $(CFOTHERS) $(CFWARN)
CFLAGS_L := $(CFOPTIMIZE_L) $(DEFINES_L) $(CFWARN_L) $(INCLUDES_L) $(CFOTHERS_L)
YCFLAGS  := -w -DYYENABLE_NLS=0 -DYYLTYPE_IS_TRIVIAL=0

UTIL = util/

OBJECTS := $(UTIL)levcomp.tab.o $(UTIL)levcomp.lex.o $(OBJECTS)

LEX := $(shell which flex 2> /dev/null)
YACC := $(shell which bison 2> /dev/null)

ifeq ($(strip $(LEX)),)
NO_YACC = YesPlease
endif
ifeq ($(strip $(YACC)),)
NO_YACC = YesPlease
endif

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
        QUIET_CC       = @echo '   ' CC $@;
        QUIET_CXX      = @echo '   ' CXX $@;
        QUIET_PCH      = @echo '   ' PCH $@;
        QUIET_LINK     = @echo '   ' LINK $@;
        QUIET_GEN      = @echo '   ' GEN $@;
        QUIET_COPY     = @echo '   ' COPY $@;
        QUIET_DEPEND   = @echo '   ' DEPEND $@;
        export V
endif
endif

ifdef TILES
TILEDEFS = dngn main player gui unrand
TILEDEFPRES = $(TILEDEFS:%=$(RLTILES)/tiledef-%)
TILEDEFTXTS = $(TILEDEFPRES:%=%.txt)
TILEDEFOBJS = $(TILEDEFPRES:%=%.o)
TILEDEFSRCS = $(TILEDEFPRES:%=%.cc)
TILEDEFHDRS = $(TILEDEFPRES:%=%.h)

TILEFILES = \
	main.png \
	player.png \
    dngn.png \
    gui.png
ORIGTILEFILES = $(TILEFILES:%=$(RLTILES)/%)
DESTTILEFILES = $(TILEFILES:%=dat/tiles/%)

OBJECTS += $(TILEDEFOBJS)
endif

ifdef NEEDS_PCRE
CONTRIB_DEPENDS += $(LIBPCRE)
endif
ifdef NEEDS_FREETYPE
CONTRIB_DEPENDS += $(LIBFREETYPE)
endif
ifdef NEEDS_SDL
CONTRIB_DEPENDS += $(LIBSDL)
endif
ifdef NEEDS_SDLIMAGE
CONTRIB_DEPENDS += $(LIBSDLIMAGE)
endif
ifdef NEEDS_LIBPNG
CONTRIB_DEPENDS += $(LIBPNG)
endif
ifdef NEEDS_ZLIB
CONTRIB_DEPENDS += $(LIBZ)
endif

CONTRIB_DEPENDS += $(LIBLUA) $(LIBSQLITE)
LIB += $(LIBLUA) $(LIBSQLITE)

GAME_DEPENDS := $(DESTTILEFILES) $(OBJECTS) $(EXTRA_OBJECTS) $(CONTRIB_DEPENDS)
SRC_PKG_BASE := stone_soup
SRC_VERSION  := $(shell git describe --tags --long)
PKG_SRC_DIR  := $(SRC_PKG_BASE)-$(SRC_VERSION)-src
SRC_PKG_TAR  := $(PKG_SRC_DIR).tbz2
SRC_PKG_ZIP  := $(PKG_SRC_DIR).zip

PKG_TIDY_LIST := $(UTIL)*.o $(LEVCOMP) *.o \
		$(UTIL)*.tab.cc $(UTIL)*.tab.h $(UTIL)*.lex.cc *.ixx
PKG_EXCLUDES  := $(PWD)/misc/src-pkg-excludes.lst

.PHONY: all install clean clean-contrib distclean debug profile wizard

##########################################################################

all: $(GAME)

test: profile
	$(PWD)/$(GAME) -test

##########################################################################
# Dependencies

DEPENDENCY_MKF := makefile.dep

ifeq ($(shell which fastdep 2> /dev/null),)
depend: $(OBJECTS:.o=.cc)
	rm -f $(DEPENDENCY_MKF).tmp
	@for i in $^; do \
		echo '   ' DEP $$i; \
	    $(CXX) -MM $(CFLAGS) $$i >>$(DEPENDENCY_MKF).tmp 2>/dev/null; \
	done
	mv -f $(DEPENDENCY_MKF).tmp $(DEPENDENCY_MKF)
else
depend: $(DEPENDENCY_MKF)

makefile.dep: .contrib-ok build.h compflag.h

%.dep: $(OBJECTS:.o=.cc)
	$(QUIET_DEPEND)fastdep $(DEFINES) $(INCLUDES) $(OBJECTS:.o=.cc) > $@
endif

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPENDENCY_MKF)
endif
endif

.INTERMEDIATE: build.h compflag.h

build.h:
	$(QUIET_GEN)util/gen_ver.pl $@

version.o: build.h compflag.h

##########################################################################
# The level compiler
#

$(UTIL)levcomp.tab.cc: $(CONTRIB_DEPENDS)
$(UTIL)levcomp.lex.cc: $(CONTRIB_DEPENDS)

ifndef NO_YACC

prebuildyacc:	$(UTIL)levcomp.tab.cc $(UTIL)levcomp.tab.h $(UTIL)levcomp.lex.cc
		$(QUIET_COPY)$(COPY) $^ prebuilt/

$(UTIL)levcomp.tab.cc: $(UTIL)levcomp.ypp
		+@$(MAKE) -C $(UTIL) levcomp.tab.cc

$(UTIL)levcomp.lex.cc: $(UTIL)levcomp.lpp $(UTIL)levcomp.tab.cc
		+@$(MAKE) -C $(UTIL) levcomp.lex.cc

else

# Pull the level-compiler stuff up from prebuilt/

$(UTIL)levcomp.tab.cc: prebuilt/levcomp.tab.cc
		$(QUIET_COPY)$(COPY) prebuilt/*.h $(UTIL)
		$(QUIET_COPY)$(COPY) $< $@

$(UTIL)levcomp.lex.cc: prebuilt/levcomp.lex.cc
		$(QUIET_COPY)$(COPY) $< $@

endif

##########################################################################


##########################################################################
# The actual build targets
#
install: $(GAME)
ifeq ($(DATADIR),)
	$(error DATADIR not set! Set DATADIR and run make clean install again)
endif
	[ -d $(prefix) ] || mkdir -p $(prefix)
	$(COPY) $(GAME) $(prefix)
	chown $(INSTALL_UGRP) $(prefix)/$(GAME)
	chmod $(MCHMOD) $(prefix)/$(GAME)
	mkdir -p $(prefix)/$(DATADIR)/dat/lua
	mkdir -p $(prefix)/$(DATADIR)/dat/clua
	mkdir -p $(prefix)/$(DATADIR)/dat/descript
	mkdir -p $(prefix)/$(DATADIR)/dat/database
	$(COPY) dat/*.des $(prefix)/$(DATADIR)/dat
	$(COPY) dat/lua/*.lua $(prefix)/$(DATADIR)/dat/lua
	$(COPY) dat/clua/*.lua $(prefix)/$(DATADIR)/dat/clua
	$(COPY) dat/descript/*.txt $(prefix)/$(DATADIR)/dat/descript
	$(COPY) dat/database/*.txt $(prefix)/$(DATADIR)/dat/database
	mkdir -p $(DATADIR)/settings
	$(COPY) ../settings/* $(prefix)/$(DATADIR)/settings/
	mkdir -p $(DATADIR)/docs
	$(COPY) ../docs/*.txt $(prefix)/$(DATADIR)/docs
	chown -R $(INSTALL_UGRP) $(prefix)/$(DATADIR)
ifneq ($(SAVEDIR),)
	mkdir -p $(prefix)/$(SAVEDIR)
	chown $(INSTALL_UGRP) $(prefix)/$(SAVEDIR)
	chmod $(MCHMOD_SAVEDIR) $(prefix)/$(SAVEDIR)
endif

clean:
	+$(MAKE) -C $(UTIL) clean
	$(RM) *.o *.ixx

clean-contrib:
	+$(MAKE) -C contrib clean

distclean: clean clean-contrib clean-rltiles
	$(RM) bones.* morgue.txt scores $(GAME) *.sav core *.0* *.lab $(DEPENDENCY_MKF)

.contrib-ok:
	@for a in contrib/lua/src/Makefile contrib/sqlite/Makefile contrib/pcre/Makefile; do \
	    if [ ! -f $$a ]; then \
	        echo "The '$$a' file is missing. Have you run 'git submodule update --init'?"; \
			exit 1; \
	    fi \
	done
	@touch .contrib-ok

# This information is included in crash reports, and is printed with
# "crawl -version"
compflag.h:
	$(QUIET_GEN)
	@echo "// Automatically generated by makefile" > compflag.h
	@echo "#ifndef __included_crawl_compiler_flags_h" >> compflag.h
	@echo "#define __included_crawl_compiler_flags_h" >> compflag.h
	@echo -n "#define CRAWL_CFLAGS \"" >> compflag.h
	@echo -n $(CFLAGS) | sed 's/\"/\\"/g' >> compflag.h
	@echo "\"" >> compflag.h
	@echo -n "#define CRAWL_CFLAGS_L \"" >> compflag.h
	@echo -n $(CFLAGS_L) | sed 's/\"/\\"/g' >> compflag.h
	@echo "\"" >> compflag.h
	@echo -n "#define CRAWL_LDFLAGS \"" >> compflag.h
	@echo -n $(LDFLAGS) | sed 's/\"/\\"/g' >> compflag.h
	@echo "\"" >> compflag.h
	@echo "#endif" >> compflag.h

$(GAME): $(GAME_DEPENDS)
	$(QUIET_LINK)$(CXX) $(LDFLAGS) $(EXTRA_OBJECTS) $(OBJECTS) -o $(GAME) $(LIB)

debug: $(GAME)
profile: $(GAME)
wizard: $(GAME)

# [ds] Note we don't use the standard CFLAGS here; that's intentional, most
# flex/bison combos I've tried don't produce code that passes the warnings
# test.

$(UTIL)%.o: $(UTIL)%.cc
	$(QUIET_CXX)$(CXX) $(CFLAGS) $(CFLAGS_L) $(YCFLAGS) -o $@ -c $<

ifdef PCH
%.h.gch: %.h
	$(QUIET_PCH)$(CXX) $(CFLAGS) $(CFLAGS_L) -c $< -o $@

CC_DEP := AppHdr.h.gch
endif

$(OBJECTS:%.o=%.cc): $(CC_DEP) $(TILEDEFHDRS) $(CONTRIB_DEPENDS)

%.o: %.m
	$(QUIET_CXX)$(CC) $(CFLAGS) $(CFLAGS_L) -c $< -o $@

%.o: %.cc
	$(QUIET_CXX)$(CXX) $(CFLAGS) $(CFLAGS_L) -c $< -o $@

#
# Contribs
#
.PHONY: pcre lua sqlite sdl sdl-image freetype

$(LIBPCRE): .contrib-ok
	+@$(MAKE) -C contrib pcre
pcre: $(LIBPCRE)

$(LIBLUA): .contrib-ok
	+@$(MAKE) -C contrib lua
lua: $(LIBLUA)

$(LIBSQLITE): .contrib-ok
	+@$(MAKE) -C contrib sqlite
sqlite: $(LIBSQLITE)

$(LIBFREETYPE): .contrib-ok
	+@$(MAKE) -C contrib freetype
freetype: $(LIBFREETYPE)

$(LIBSDL): .contrib-ok
	+@$(MAKE) -C contrib sdl
sdl: $(LIBSDL)

$(LIBSDLIMAGE): .contrib-ok
	+@$(MAKE) -C contrib sdl-image
sdl-image: $(LIBSDLIMAGE)


#############################################################################
# Build unrandart data
art-data.h: art-data.txt util/art-data.pl art-func.h
	util/art-data.pl

#############################################################################
# RLTiles
#

.PHONY: rltile-build
rltile-build: $(CONTRIB_DEPENDS)
	$(MAKE) -C $(RLTILES) -f makefile.unix all

$(TILEDEFSRCS): rltile-build
$(TILEDEFHDRS): rltile-build
$(ORIGTILEFILES): rltile-build

dat/tiles/%.png: $(RLTILES)/%.png
	$(QUIET_COPY)$(COPY) $< $@

clean-rltiles:
	+$(MAKE) -C $(RLTILES) -f makefile.unix distclean

#############################################################################
# Packaging a source tarball for release
#

# To package, you *must* have lex and yacc to generate the intermediates.
ifdef DOYACC
package-source: distclean prebuildyacc pkgtidy depend removeold vlink \
				pkgtarbz2 pkgzip

pkgtidy:
	$(RM) $(PKG_TIDY_LIST)

removeold:
	if [ -f ../../$(SRC_PKG_TAR) ]; then $(RM) ../../$(SRC_PKG_TAR); fi
	if [ -f ../../$(SRC_PKG_ZIP) ]; then $(RM) ../../$(SRC_PKG_ZIP); fi

# [ds] Existing directory names could produce a bad package!
vlink:
	cd .. && WHERE=$$PWD && cd .. && \
		( [ -e $(PKG_SRC_DIR) ] || ln -sf $$WHERE $(PKG_SRC_DIR) )

pkgtarbz2:
	cd ../.. && tar -ch --bzip2 -f $(SRC_PKG_TAR) \
		-X $(PKG_EXCLUDES) $(PKG_SRC_DIR)

pkgzip:
	cd ../.. && zip -rq $(SRC_PKG_ZIP) $(PKG_SRC_DIR) \
		-x@$(PKG_EXCLUDES)

endif
