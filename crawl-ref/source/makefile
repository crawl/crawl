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
# These are good optimizations for a Pentium M or better:
#CFOPTIMIZE := -O2 -march=i686 -mtune=i686 -mmmx -msse -msse2 -mfpmath=sse -funroll-all-loops
# These are really good options for the Intel C++ compiler:
#CFOPTIMIZE := -O2 -parallel
CFOPTIMIZE := -O2
CFOTHERS := -fno-strict-aliasing -pipe
CFOTHERS_L := -fsigned-char
CFWARN := -Wall

#
# The GCC and GXX variables are set later.
#
AR = ar rcu
CC = $(GCC)
CXX = $(GXX)
DELETE = rm -f
COPY = cp

export AR
export CC
export CXX
export CFLAGS

#
# Platform Detection
#
# Use the variable OSNAME if you need to add something that's
# platform-specific.
# The valid OSNAME values are currently:
#    'WIN32': alias for MinGW and Cygwin
#    'MacOS': alias for Darwin (Mac OS X)
# and non-translated values from other platforms (values from
# 'uname -s' call). So Linux is simply 'Linux', etc.
#
UNAME_S := $(shell uname -s | cut -d'_' -f1)
OSNAME =
ifeq ($(UNAME_S),MINGW32)
OSNAME = WIN32
endif
ifeq ($(UNAME_S),CYGWIN)
OSNAME = WIN32
endif
ifeq ($(UNAME_S),Darwin)
OSNAME = MacOS
endif
ifeq ($(OSNAME),)
OSNAME = $(UNAME_S)
endif

ifeq ($(OSNAME),WIN32)
GAME = crawl.exe
endif

TILES = n
HURRY = n

#
# Set up object file dependencies for $(GAME) target.
#
include makefile.obj

# Works for Mac OS X and Linux.
OBJECTS += crash-u.o

ifneq ($(TILES),n)
OBJECTS += libgui.o tile2.o tilereg.o tilepick.o tilesdl.o tilefont.o tiletex.o tilemcache.o tilebuf.o
else
ifneq ($(UNAME_S),MINGW32)
OBJECTS += libunix.o
else
OBJECTS += libw32c.o
endif
endif

# To get stack trace symbols.
# Note that MinGW doesn't support -rdynamic.
ifeq ($(OSNAME),WIN32)
LDFLAGS :=
else
LDFLAGS := -rdynamic
endif

# Change this to y (case-sensitive!) if you want to use Unicode glyphs
# in the map, and you have libncursesw available.
UNICODE_GLYPHS = n
# Usually, it can be autodetected for you:
ifneq ($(shell ls /usr/include/ncursesw 2> /dev/null),)
UNICODE_GLYPHS = y
endif


# If you're using UNICODE_GLYPHS=y, and have a preferred Unicode
# (UTF-8) locale you want Crawl to use, you can set it here. The
# default is en_US.UTF-8. If you'd prefer that Crawl use the locale
# as set in your environment LC_* variables, use UNICODE_LOCALE = .
UNICODE_LOCALE =

# If you have lex and yacc, set DOYACC to y (lowercase y).
DOYACC := y

# Permissions to set on the game executable.
MCHMOD := 2755

# Permissions to set on the save directory.
MCHMOD_SAVEDIR := 775

# The user:group to install the game as.
INSTALL_UGRP := games:games

INSTALLDIR   := /usr/games/crawl

# If you're installing Crawl for multiple users, you *must* set this to a
# valid path before building Crawl. This is not necessary if you are building
# Crawl for a single user.

# SAVEDIR := /usr/games/crawl/saves/
# DATADIR := /usr/games/crawl/data/

PCH := n

# libpcre
PCRESRC := util/pcre/
PCRELIB := pcre
PCRELIBA := lib$(PCRELIB).a
FPCRELIBA := $(PCRESRC)$(PCRELIBA)

# LUA
LUASRC := util/lua/src/
LUALIB := lua
LUALIBA := lib$(LUALIB).a
FLUALIBA := $(LUASRC)$(LUALIBA)

# SQLite
SQLSRC   := util/sqlite/
SQLLIB   := sqlite3
SQLLIBA  := lib$(SQLLIB).a
FSQLLIBA := $(SQLSRC)$(SQLLIBA)

LIBDBM         := $(FSQLLIBA)
EXTRA_INCLUDES += -I$(SQLSRC)
EXTRA_DEPENDS  += $(FSQLLIBA)

LIB := -L$(LUASRC) -l$(LUALIB) $(LIBDBM)

INCLUDES_L += -Iutil -I. -I$(LUASRC) $(EXTRA_INCLUDES)

ifeq ($(OSNAME),MacOS)
APPLE_GCC=y
else
APPLE_GCC=n
endif
export APPLE_GCC

GCC_VER=

ifneq ($(APPLE_GCC),n)

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

ifneq ($(GCC_VER),)
GCC := gcc-$(GCC_VER)
GXX := g++-$(GCC_VER)
else
GCC := gcc
GXX := g++
endif

RLTILES = rltiles

#
# Tiles build stuff
#
ifneq ($(TILES),n)

DEFINES_L += -DUSE_TILE
INCLUDES_L += -I$(RLTILES)

ifeq ($(OSNAME),MacOS)
# Handle the tile build dependencies for Mac a bit
# differently than other platforms.

LIB += -framework OpenGL -framework AppKit
endif

ifneq ($(APPLE_GCC),n)

# SDL has a goof in the stock SDL_config.h which checks whether
# __MACOSX__ is defined. It's not one of Apple's stock markers,
# so we define it ourselves here.
#
DEFINES += -D__MACOSX__

EXTRA_OBJECTS += SDLMain.o

# If the /Library/Frameworks folder exists, add a -F flag
#
ifneq ($(shell ls -a /Library/Frameworks 2> /dev/null),)
LIB += -F/Library/Frameworks/
endif

# Check if our dependencies exist as frameworks. If so, use their
# headers and link to them.
#
ifneq ($(shell ls /Library/Frameworks/SDL.framework 2> /dev/null),)
INCLUDES_L += -I/Library/Frameworks/SDL.framework/Headers
LIB += -framework SDL
endif
ifneq ($(shell ls /Library/Frameworks/SDL_image.framework 2> /dev/null),)
INCLUDES_L += -I/Library/Frameworks/SDL_image.framework/Headers
LIB += -framework SDL_image
endif
ifneq ($(shell ls /Library/Frameworks/Freetype2.framework 2> /dev/null),)
INCLUDES_L += -I/Library/Frameworks/Freetype2.framework/Headers
LIB += -framework Freetype2
endif
ifneq ($(shell ls /Library/Frameworks/libpng.framework 2> /dev/null),)
INCLUDES_L += -I/Library/Frameworks/libpng.framework/Headers
LIB += -framework libpng
endif

else # MacOS

# Okay, we have to assume we're on something else that
# uses standard UNIX-like methods for finding libs.
#
# For instance, on Linux and most other UNIX-likes,
# the app pkg-config can provide the appropriate
# CFLAGS and LDFLAGS.
#

ifneq ($(shell which pkg-config 2> /dev/null),)

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
PNG_LDFLAGS := -L/opt/libpng/lib -lpng

FREETYPE_INCLUDE := -I/opt/freetype/include
FREETYPE_CFLAGS  :=
FREETYPE_LDFLAGS := -L/opt/freetype/lib -lfreetype

ifneq ($(which sdl-config 2> /dev/null),)

# sdl-config is our second best bet...
#
SDL_INCLUDE := $(shell sdl-config --cflags | tr ' ' '\n' | grep ^[-]I | tr '\n' ' ')
SDL_CFLAGS  := $(shell sdl-config --cflags | tr ' ' '\n' | grep -v ^[-]I | tr '\n' ' ')
SDL_LDFLAGS := $(shell sdl-config --libs)

else # sdl-config

# Fine, we have to make some REALLY ugly assumptions...
#
SDL_INCLUDE := -I/opt/sdl/include/SDL
SDL_CFLAGS  :=
SDL_LDFLAGS := -L/opt/sdl/lib -lSDL -lSDLmain

endif # sdl-config
endif # pkg-config

LIB += $(PNG_LDFLAGS) $(FREETYPE_LDFLAGS) $(SDL_LDFLAGS) -lSDL_image
ifneq ($(OSNAME),MacOS)
LIB += -lGL -lGLU
endif

DEFINES_L += $(PNG_CFLAGS) $(FREETYPE_CFLAGS) $(SDL_CFLAGS)
INCLUDES_L += $(PNG_INCLUDE) $(FREETYPE_INCLUDE) $(SDL_INCLUDE)

endif # MacOS

endif # TILES

CFWARN_L := -D_FORTIFY_SOURCE=0 -Wno-array-bounds -Wno-parentheses -Wwrite-strings -Wshadow -pedantic
CFOTHERS_L = $(EXTERNAL_FLAGS) $(EXTRA_FLAGS) $(DEFINES) -DCLUA_BINDINGS $(SDL_CFLAGS)

#
# Figure out the build settings for this type of build
#

# Crawl defaults
# Optimized, no debugging.
FULLDEBUG=n
WIZARD=n
DEBUG=n
OPTIMIZE=y

# Debug
# No optimization, full debugging.
ifeq ($(MAKECMDGOALS),debug)
FULLDEBUG=y
WIZARD=y
DEBUG=y
OPTIMIZE=n
endif

# Wizard
# Optimized, with wizard mode.
ifeq ($(MAKECMDGOALS),wizard)
FULLDEBUG=n
WIZARD=y
DEBUG=y
OPTIMIZE=y
endif

# Profile
# Optimized, with full debugging.
ifeq ($(MAKECMDGOALS),profile)
FULLDEBUG=y
WIZARD=y
DEBUG=y
OPTIMIZE=y
endif

ifneq ($(HURRY),n)
OPTIMIZE=n
endif

ifeq ($(FULLDEBUG),y)
DEFINES += -DFULLDEBUG
endif
ifeq ($(DEBUG),y)
EXTRA_FLAGS += -ggdb
DEFINES += -DDEBUG
endif
ifeq ($(WIZARD),y)
DEFINES += -DWIZARD
endif
ifeq ($(OPTIMIZE),n)
CFOPTIMIZE  := -O0
endif
ifneq ($(PCH),n)
CFWARN_L += -Winvalid-pch
endif

# Cygwin has a panic attack if we do this...
ifneq ($(OPTIMIZE),n)
ifneq ($(UNAME_S),CYGWIN)
CFWARN_L += -Wuninitialized
else
CFWARN_L += -Wno-uninitialized
endif
endif

ifneq ($(strip $(SAVEDIR)),)
CFOTHERS_L += '-DSAVE_DIR_PATH="$(strip $(SAVEDIR))"'
endif

ifneq ($(strip $(DATADIR)),)
CFOTHERS_L += '-DDATA_DIR_PATH="$(strip $(DATADIR))"'
endif

ifeq ($(strip $(UNICODE_GLYPHS)),y)
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
ifeq ($(shell uname),Darwin)
LIBCURS = ncurses
endif

else
# Include path for curses or ncurses (non-Unicode).
INCLUDES_L += -I/usr/include/ncurses
LIBCURS  = ncurses
endif

ifeq ($(OSNAME),WIN32)
ifneq ($(TILES),n)
CFOTHERS_L += -DWIN32TILES
else
CFOTHERS_L += -DWIN32CONSOLE
endif
endif

USE_PCRE := n
ifeq ($(OSNAME),WIN32)
USE_PCRE := y
endif

ifneq ($(USE_PCRE),n)
INCLUDES_L += -I$(PCRESRC)
DEFINES += -DREGEX_PCRE
LIB += $(FPCRELIBA)
endif

ifneq ($(UNAME_S),MINGW32)
LIB += -l$(LIBCURS)
endif

USE_ICC := n

ifneq ($(USE_ICC),n)
GCC := icc
GXX := icpc
AR  := xiar rcu
LIB += -lguide -lpthread
CFWARN := -wd383,810,869,981,1418 -we14,193,304
CFWARN_L :=
endif

CFLAGS   := $(CFOPTIMIZE) $(CFOTHERS) $(CFWARN)
CFLAGS_L := $(CFOPTIMIZE_L) $(DEFINES_L) $(CFWARN_L) $(INCLUDES_L) $(CFOTHERS_L)
YCFLAGS  := -w -DYYENABLE_NLS=0 -DYYLTYPE_IS_TRIVIAL=0

UTIL = util/

OBJECTS := $(UTIL)levcomp.tab.o $(UTIL)levcomp.lex.o $(OBJECTS)

LEX := $(shell which flex 2> /dev/null)
YACC := $(shell which bison 2> /dev/null)

ifeq ($(strip $(LEX)),)
DOYACC := n
endif
ifeq ($(strip $(YACC)),)
DOYACC := n
endif

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
        QUIET_CC       = @echo '   ' CC $@;
        QUIET_CXX      = @echo '   ' CXX $@;
        QUIET_PCH      = @echo '   ' PCH $@;
        QUIET_LINK     = @echo '   ' LINK $@;
        QUIET_COPY     = @echo '   ' COPY $@;
        QUIET_DEPEND   = @echo '   ' DEPEND $@;
        export V
endif
endif

ifneq ($(TILES),n)
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

EXTRA_DEPENDS += $(DESTTILEFILES)
OBJECTS += $(TILEDEFOBJS)
endif

GAME_DEPENDS := $(LUASRC)$(LUALIBA) $(EXTRA_DEPENDS) $(OBJECTS) $(EXTRA_OBJECTS)
SRC_PKG_BASE := stone_soup
SRC_VERSION  := $(shell egrep 'VER_NUM  *".*"' version.h | cut -d'"' -f2)
PKG_SRC_DIR  := $(SRC_PKG_BASE)-$(SRC_VERSION)-src
SRC_PKG_TAR  := $(PKG_SRC_DIR).tbz2
SRC_PKG_ZIP  := $(PKG_SRC_DIR).zip

ifneq ($(USE_PCRE),n)
GAME_DEPENDS += $(FPCRELIBA)
endif

PKG_TIDY_LIST := $(UTIL)*.o $(LEVCOMP) *.o \
		$(UTIL)*.tab.cc $(UTIL)*.tab.h $(UTIL)*.lex.cc *.ixx
PKG_EXCLUDES  := $(PWD)/misc/src-pkg-excludes.lst

##########################################################################

all: $(GAME)

test: $(GAME)
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

%.dep: $(OBJECTS:.o=.cc)
	$(QUIET_DEPEND)fastdep $(DEFINES) $(INCLUDES) *.h $(OBJECTS:.o=.cc) > $@
endif

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPENDENCY_MKF)
endif
endif

##########################################################################
# The level compiler
#

ifeq ($(strip $(DOYACC)),y)

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
	[ -d $(INSTALLDIR) ] || mkdir -p $(INSTALLDIR)
	$(COPY) $(GAME) $(INSTALLDIR)
	chown $(INSTALL_UGRP) $(INSTALLDIR)/$(GAME)
	chmod $(MCHMOD) $(INSTALLDIR)/$(GAME)
	mkdir -p $(DATADIR)/dat
	mkdir -p $(DATADIR)/dat/lua
	mkdir -p $(DATADIR)/dat/clua
	mkdir -p $(DATADIR)/dat/descript
	mkdir -p $(DATADIR)/dat/database
	$(COPY) dat/*.des $(DATADIR)/dat
	$(COPY) dat/lua/*.lua $(DATADIR)/dat/lua
	$(COPY) dat/clua/*.lua $(DATADIR)/dat/clua
	$(COPY) dat/descript/*.txt $(DATADIR)/dat/descript
	$(COPY) dat/database/*.txt $(DATADIR)/dat/database
	mkdir -p $(DATADIR)/settings
	$(COPY) ../settings/* $(DATADIR)/settings/
	mkdir -p $(DATADIR)/docs
	$(COPY) ../docs/*.txt $(DATADIR)/docs
	chown -R $(INSTALL_UGRP) $(DATADIR)
ifneq ($(SAVEDIR),)
	mkdir -p $(SAVEDIR)
	chown $(INSTALL_UGRP) $(SAVEDIR)
	chmod $(MCHMOD_SAVEDIR) $(SAVEDIR)
endif

clean:
	+$(MAKE) -C $(UTIL) clean
	$(DELETE) *.o
	$(DELETE) *.ixx

clean-lua:
	+$(MAKE) -C $(LUASRC) clean

clean-sql:
	+$(MAKE) -C $(SQLSRC) clean

distclean: clean clean-lua clean-sql clean-rltiles
	$(DELETE) bones.*
	$(DELETE) morgue.txt
	$(DELETE) scores
	$(DELETE) $(GAME)
	$(DELETE) *.sav
	$(DELETE) core
	$(DELETE) *.0*
	$(DELETE) *.lab
	$(DELETE) $(DEPENDENCY_MKF)

$(GAME): $(GAME_DEPENDS)
	$(QUIET_LINK)$(CXX) $(LDFLAGS) $(EXTRA_OBJECTS) $(OBJECTS) -o $(GAME) $(LIB)

.PHONY : debug profile wizard

debug: $(GAME)
profile: $(GAME)
wizard: $(GAME)

# [ds] Note we don't use the standard CFLAGS here; that's intentional, most
# flex/bison combos I've tried don't produce code that passes the warnings
# test.

$(UTIL)%.o: $(UTIL)%.cc
	$(QUIET_CXX)$(CXX) $(CFLAGS) $(CFLAGS_L) $(YCFLAGS) -o $@ -c $<

ifneq ($(PCH),n)
%.h.gch: %.h
	$(QUIET_PCH)$(CXX) $(CFLAGS) -c $< -o $@

CC_DEP := AppHdr.h.gch
endif

$(OBJECTS:%.o=%.cc): $(CC_DEP) $(TILEDEFHDRS)

%.o: %.m
	$(QUIET_CXX)$(CC) $(CFLAGS) $(CFLAGS_L) -c $< -o $@

%.o: %.cc
	$(QUIET_CXX)$(CXX) $(CFLAGS) $(CFLAGS_L) -c $< -o $@

#############################################################################
# Build PCRE

$(FPCRELIBA):
	+@$(MAKE) -C $(PCRESRC)

#############################################################################
# Build Lua

$(FLUALIBA):
	+@$(MAKE) -C $(LUASRC) crawl_unix

#############################################################################
# Build SQLite

$(FSQLLIBA):
	+@$(MAKE) -C $(SQLSRC)

#############################################################################
# Build unrandart data
art-data.h: art-data.txt util/art-data.pl art-func.h
	util/art-data.pl

#############################################################################
# RLTiles
#

.PHONY: rltile-build
rltile-build:
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
ifeq ($(DOYACC),y)
package-source: distclean prebuildyacc pkgtidy depend removeold vlink \
				pkgtarbz2 pkgzip

pkgtidy:
	$(DELETE) $(PKG_TIDY_LIST)

removeold:
	if [ -f ../../$(SRC_PKG_TAR) ]; then $(DELETE) ../../$(SRC_PKG_TAR); fi
	if [ -f ../../$(SRC_PKG_ZIP) ]; then $(DELETE) ../../$(SRC_PKG_ZIP); fi

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
