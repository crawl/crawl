#
# Dungeon Crawl Stone Soup
# GNU Makefile
#
# largely written by Steven Noonan <steven@uplinklabs.net>
#    (if something breaks, blame him.)
#

# Typical targets:
#    make
#    make wizard
#    make debug
#    make install
#    make wizard install
#    make debug install
#    -- note, unlike most programs, you need to specify build type when
#       installing even if you just built it.
# Typical parameters:
#    TILES         -- set to anything to enable tiles build
#
#    CROSSHOST     -- target system, eg, i386-pc-msdosdjgpp or i586-mingw32msvc
#
#    prefix        -- installation base.  Specify eg. /usr/local on Unix systems.
#    DESTDIR       -- installation staging area (the dir you intend to pack)
#    DATADIR       -- place to hold immutable files.  Can be either relative to
#                     "prefix" or absolute.
#    SAVEDIR       -- place to hold writeable data (saves, database, morgue
#                     dumps).  Can be relative to "prefix" or absolute.
#       Layout examples:
#         prefix=~/crawl DATADIR=data/ SAVEDIR=saves/
#                  -- everything under ~/crawl
#         prefix=/usr/local
#                  -- a typical multiuser installation
#
#    V             -- set to anything to enable verbose build
#
#    USE_ICC       -- set to use Intel's compiler
#    ASSERTS       -- set to enable assertion checks (implied in debug mode)
#
#
# Requirements:
#    For tile builds, you need pkg-config.
#    You also need libpng, sdl, sdl-image and libfreetype -- if you got your
#    source from git, you can 'git submodule update' to fetch them; you can also
#    ask for a package with convenience libraries instead -- we'll try to provide
#    them somewhere in the near future.

GAME = crawl

ASSERTS = yes

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


ifdef USE_ICC

# If you have a Core 2 processor, this _really_ makes things fly:
#CFOPTIMIZE := -O2 -parallel -xT

# Optionally enable the 'ipo' feature, to facilitate inlining
# across object file bounds.
#CFOPTIMIZE_L := -ipo

# Some very good optimization flags.
CFOPTIMIZE := -O2 -parallel

else

CFOPTIMIZE := -O2

endif # USE_ICC

CFOTHERS := -fno-strict-aliasing -pipe $(EXTERNAL_FLAGS)
CFOTHERS_L := -fsigned-char
CFWARN := -Wall
CFWARN_L := -Wundef

DEFINES := $(EXTERNAL_DEFINES)

#
# The GCC and GXX variables are set later.
#
AR = ar
RANLIB = ranlib
CC = $(GCC)
CXX = $(GXX)
RM = rm -f
COPY = cp
COPY_R = cp -r
STRIP = strip
WINDRES = windres
CHMOD = chmod 2>/dev/null
CHOWN = chown 2>/dev/null

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
LIBPNG := contrib/install/lib/libpng.a
LIBSDLIMAGE := contrib/install/lib/libSDL_image.a
LIBFREETYPE := contrib/install/lib/libfreetype.a
LIBSQLITE := contrib/install/lib/libsqlite3.a
ifdef USE_LUAJIT
LIBLUA := contrib/install/lib/libluajit.a
else
LIBLUA := contrib/install/lib/liblua.a
endif
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
	NO_AUTO_OPT = YesPlease
	NEED_STATIC = YesPlease
	CONFIGURE_FLAGS += --host=$(CROSSHOST)
	BUILD_LUA = yes
	BUILD_SQLITE = yes

	# If needed, override uname_S so we get the appropriate
	# things compiled.
	ifneq (,$(findstring mingw,$(CROSSHOST)))
		uname_S=MINGW32
	endif
	ifneq (,$(findstring djgpp,$(CROSSHOST)))
		uname_S=DOS
	endif

endif
ifneq (,$(findstring MINGW,$(uname_S)))
	GAME = crawl.exe
	bin_prefix = .
	WIN32 = Yes
	NO_RDYNAMIC = YesPlease
	NO_NCURSES = YesPlease
	NEED_LIBW32C = YesPlease
	BUILD_PCRE = YesPlease
	ifdef TILES
		EXTRA_LIBS += -lmingw32 -lgdi32 -lwinmm contrib/install/lib/libSDLmain.a -mwindows
		BUILD_FREETYPE = YesPlease
		BUILD_SDL = YesPlease
		BUILD_SDLIMAGE = YesPlease
		BUILD_LIBPNG = YesPlease
		BUILD_ZLIB = YesPlease
	endif
endif
ifeq ($(uname_S),DOS)
	GAME = crawl.exe
	bin_prefix = .
	NO_NCURSES = yes
	NO_UNICODE = yes
	NEED_LIBDOS = yes
	BUILD_PCRE = yes
	NO_RDYNAMIC = yes
endif
ifeq ($(uname_S),Darwin)
	NEED_APPKIT = YesPlease
	LIBNCURSES_IS_UNICODE = Yes
	NO_PKGCONFIG = Yes
	ifndef NO_APPLE_GCC
		APPLE_GCC = YesPlease
	endif
	BUILD_SQLITE = YesPlease
	ifdef TILES
		EXTRA_LIBS += -framework AppKit -framework AudioUnit -framework Carbon -framework IOKit -framework OpenGL contrib/install/lib/libSDLmain.a
		BUILD_FREETYPE = YesPlease
		BUILD_SDL = YesPlease
		BUILD_SDLIMAGE = YesPlease
		BUILD_LIBPNG = YesPlease
		BUILD_ZLIB = YesPlease
	endif
endif
ifdef USE_LUAJIT
	BUILD_LUA := YesPlease
endif
ifdef USE_ICC
	NO_RDYNAMIC := YesPlease
endif
ifneq (,$(findstring CYGWIN,$(uname_S)))
	GAME = crawl.exe
	NO_RDYNAMIC = YesPlease
	BUILD_PCRE = YesPlease
endif
ifeq (,$(findstring .exe,$(GAME)))
	DEFINES += -DUSE_TAR
else
	DEFINES += -DUSE_ZIP
endif

ifdef BUILD_ALL
	BUILD_FREETYPE = YesPlease
	BUILD_PCRE = YesPlease
	BUILD_SDL = YesPlease
	BUILD_SDLIMAGE = YesPlease
	BUILD_SQLITE = YesPlease
	BUILD_LUA = YesPlease
	BUILD_LIBPNG = YesPlease
	BUILD_ZLIB = YesPlease
endif

#
# Set up object file dependencies for $(GAME) target.
#
include makefile.obj

# Just a quick hack to make it clean up
# tiles-specific object files too.
ifneq (,$(findstring clean,$(MAKECMDGOALS)))
TILES := YesPlease
endif

# Works for Mac OS X and Linux.
OBJECTS += crash-u.o

ifdef WIN32
EXTRA_OBJECTS += icon.o
endif

ifdef TILES
OBJECTS += libgui.o tile2.o tilereg.o tilepick.o tilesdl.o tilefont.o tiletex.o tilemcache.o tilebuf.o uiwrapper.o glwrapper.o cgcontext.o

ifdef SDL
OBJECTS += uiwrapper-sdl.o cgcontext-sdl.o
endif

ifdef GL
OBJECTS += glwrapper-ogl.o
endif

ifdef FT
OBJECTS += fontwrapper-ft.o
endif

else
ifdef NEED_LIBW32C
OBJECTS += libw32c.o
else
ifdef NEED_LIBDOS
OBJECTS += libdos.o
else
OBJECTS += libunix.o
endif
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

ifdef USE_MERGE_BASE
MERGE_BASE := $(shell git merge-base HEAD $(USE_MERGE_BASE))
endif

# Permissions to set on the game executable.
MCHMOD := 2755

# Permissions to set on the save directory.
MCHMOD_SAVEDIR := 775
MCHMOD_LOGS := 664

# The user:group to install the game as.
INSTALL_UGRP := games:games

chroot_prefix :=
prefix        :=

ifeq ($(patsubst %/local,%,$(patsubst %/,%,$(prefix))),/usr)
FHS := yes
endif

ifeq (,$(bin_prefix))
ifneq ($(patsubst %/,%,$(prefix)),/usr)
bin_prefix    := bin
else
bin_prefix    := games
endif
endif

# If you're installing Crawl for multiple users, you *must* set this to a
# valid path before building Crawl. This is not necessary if you are building
# Crawl for a single user.
# If you're installing to /usr or /usr/local, we have sane defaults.

# SAVEDIR := saves/
# DATADIR := data/
ifneq (,$(FHS))
DATADIR       := share/crawl
SAVEDIR       := /var/games/crawl
endif


INCLUDES_L += -Icontrib/install/include
LIBS += -Lcontrib/install/lib

INCLUDES_L += -Iutil -I.

ifdef APPLE_GCC

# Compatibility level for Mac OS X
#
SDK_VER := 10.4
GCC_VER := 4.0

ARCHS := $(shell arch)
export ARCHS

CF_ARCHS := $(patsubst %,-arch %,$(ARCHS))

# Mac OS X 10.4 adds a 'u' on the end of the SDK name. Everything
# else is much easier to predict the name of.
ifeq ($(SDK_VER),10.4)
SDKROOT := /Developer/SDKs/MacOSX$(SDK_VER)u.sdk
else
SDKROOT := /Developer/SDKs/MacOSX$(SDK_VER).sdk
endif

CC = $(GCC) $(CF_ARCHS) -isysroot $(SDKROOT) -mmacosx-version-min=$(SDK_VER)
CXX = $(GXX) $(CF_ARCHS) -isysroot $(SDKROOT) -mmacosx-version-min=$(SDK_VER)

ifdef USE_ICC
CC += -gcc-name=gcc-$(GCC_VER) -gxx-name=g++-$(GCC_VER)
CXX += -gcc-name=gcc-$(GCC_VER) -gxx-name=g++-$(GCC_VER)
endif

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
STRIP := $(CROSSHOST)-strip
WINDRES := $(CROSSHOST)-windres

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

ifndef BUILD_LUA
  ifneq (,$(wildcard /usr/include/lua5.1))
    INCLUDES_L += -I/usr/include/lua5.1
    LIBS += -llua5.1
  else
    ifneq (,$(wildcard /usr/include/lua.h))
      LIBS += -llua
    else
      BUILD_LUA = yes
    endif
  endif
endif

ifndef BUILD_SQLITE
  ifneq ($(shell grep -q sqlite3_prepare_v2 /usr/include/sqlite3.h 2>/dev/null && echo yes),yes)
    BUILD_SQLITE = yes
  else
    LIBS += -lsqlite3
  endif
endif

RLTILES = rltiles

#
# Tiles build stuff
#
ifdef TILES

DEFINES_L += -DUSE_TILE
INCLUDES_L += -I$(RLTILES)

ifdef BUILD_SDL
INCLUDES_L += -Icontrib/install/include/SDL
endif
ifdef BUILD_FREETYPE
INCLUDES_L += -Icontrib/install/include/freetype2
endif

ifdef SDL
DEFINES_L += -DUSE_SDL
endif

ifdef GL
DEFINES_L += -DUSE_GL
endif

ifdef FT
DEFINES_L += -DUSE_FT
endif

# Okay, we have to assume we're on something else that
# uses standard UNIX-like methods for finding libs.
#
# For instance, on Linux and most other UNIX-likes,
# the app pkg-config can provide the appropriate
# CFLAGS and LDFLAGS.
#

ifndef NO_PKGCONFIG
ifneq ($(shell which pkg-config 2> /dev/null),)
PKGCONFIG = YesPlease
endif
endif

ifdef PKGCONFIG

# If pkg-config is available, it's the surest way to find where
# the contributing libraries are located.
#

FREETYPE_INCLUDE := $(shell pkg-config freetype2 --cflags-only-I)
FREETYPE_CFLAGS  := $(shell pkg-config freetype2 --cflags-only-other)
FREETYPE_LDFLAGS := $(shell pkg-config freetype2 --libs-only-L) $(shell pkg-config freetype2 --libs-only-l)

SDL_INCLUDE := $(shell pkg-config sdl --cflags-only-I)
SDL_CFLAGS  := $(shell pkg-config sdl --cflags-only-other)
SDL_LDFLAGS := $(shell pkg-config sdl --libs-only-L) $(shell pkg-config sdl --libs-only-l)

LIBS += -lSDL_image $(SDL_LDFLAGS) $(FREETYPE_LDFLAGS)

endif # pkg-config

ifneq ($(uname_S),Darwin)
ifeq (,$(findstring MINGW,$(uname_S)))
LIBS += -lGL -lGLU
else
LIBS += -lopengl32 -lglu32
endif
endif

DEFINES_L += $(PNG_CFLAGS) $(FREETYPE_CFLAGS) $(SDL_CFLAGS)
INCLUDES_L += $(PNG_INCLUDE) $(FREETYPE_INCLUDE) $(SDL_INCLUDE)

ifdef PROPORTIONAL_FONT
DEFINES += -DPROPORTIONAL_FONT=\"$(PROPORTIONAL_FONT)\"
endif
ifdef MONOSPACED_FONT
DEFINES += -DMONOSPACED_FONT=\"$(MONOSPACED_FONT)\"
endif

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
ifneq (,$(findstring debug,$(MAKECMDGOALS)))
	FULLDEBUG=YesPlease
	WIZARD=YesPlease
	DEBUG=YesPlease
	NO_OPTIMIZE=YesPlease
endif

# Wizard
# Optimized, with wizard mode.
ifneq (,$(findstring wizard,$(MAKECMDGOALS)))
	WIZARD=YesPlease
	DEBUG=YesPlease
endif

# Profile
# Optimized, with full debugging.
ifneq (,$(findstring profile,$(MAKECMDGOALS)))
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
CFOTHERS := -ggdb $(CFOTHERS)
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

ifdef ASSERTS
DEFINES += -DASSERTS
endif

# Cygwin has a panic attack if we do this...
ifndef NO_OPTIMIZE
ifneq ($(GCC_GTE_4_0_0),0)
CFWARN_L += -Wuninitialized
else
CFWARN_L += -Wno-uninitialized
endif
endif

ifneq ($(strip $(chroot_prefix)),)
	USE_CHROOT=YesPlease
endif

ifdef USE_DGAMELAUNCH
	CFOTHERS_L += -DDGAMELAUNCH
endif

ifdef USE_CHROOT
	prefix_fp := $(abspath $(strip $(DESTDIR)$(chroot_prefix))/$(strip $(prefix)))
else
	prefix_fp := $(abspath $(strip $(DESTDIR)$(prefix)))
endif

ifneq ($(strip $(SAVEDIR)),)
  ifeq ($(filter /%,$(SAVEDIR)),)
    ifneq ($(prefix),)
	override SAVEDIR := $(strip $(prefix))/$(strip $(SAVEDIR))
    endif
  endif
  CFOTHERS_L += -DSAVE_DIR_PATH=\"$(abspath $(SAVEDIR))\"
  savedir_fp := $(abspath $(strip $(DESTDIR))$(strip $(SAVEDIR)))
endif

ifneq ($(strip $(DATADIR)),)
  ifeq ($(filter /%,$(DATADIR)),)
    #relative DATADIR
    ifneq ($(prefix),)
	override DATADIR := $(strip $(prefix))/$(strip $(DATADIR))
    endif
  endif
  CFOTHERS_L += -DDATA_DIR_PATH=\"$(abspath $(DATADIR))/\"
else
  ifneq ($(prefix),)
	DATADIR := $(strip $(prefix))/$(strip $(DATADIR))
  endif
endif
datadir_fp := $(abspath $(strip $(DESTDIR))$(strip $(DATADIR)))

ifndef NO_NCURSES

NC_LIB = ncurses
NC_PREFIX = /usr
NC_INCLUDE = $(NC_PREFIX)/include/ncurses

# Usually, it can be autodetected for you:
ifndef NO_UNICODE
ifneq ($(shell ls $(NC_PREFIX)/include/ncursesw 2> /dev/null),)
NC_INCLUDE = $(NC_PREFIX)/include/ncursesw
USE_UNICODE = YesPlease
endif
endif

# If you have USE_UNICODE set, and have a preferred Unicode
# (UTF-8) locale you want Crawl to use, you can set it here. The
# default is en_US.UTF-8. If you'd prefer that Crawl use the locale
# as set in your environment LC_* variables, use UNICODE_LOCALE = .
UNICODE_LOCALE =

INCLUDES_L += -I$(NC_INCLUDE)

ifdef USE_UNICODE
# Include path for (n)curses with Unicode support.

# Your ncurses library may include Unicode support, and you may not have a
# separate libncursesw; in that case, change this line accordingly.
NC_LIB  = ncursesw
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
NC_LIB = ncurses
endif

endif

ifndef TILES
LIBS += -L$(NC_PREFIX)/lib -l$(NC_LIB)
endif

endif

ifdef BUILD_PCRE
DEFINES += -DREGEX_PCRE
LIBS += -lpcre
endif

ifdef USE_ICC
NO_INLINE_DEPGEN := YesPlease
GCC := icc
GXX := icpc
AR  := xiar
RANLIB := true
LIBS += -lguide -lpthread
CFWARN := -wd383,810,869,981,1418 -we14,193,304
CFWARN_L :=
endif

LDFLAGS += $(CFOPTIMIZE) $(CFOPTIMIZE_L)

ifdef REPORT
CFOTHERS += -ftime-report
endif

CFLAGS   := $(CFOPTIMIZE) $(CFOTHERS) $(CFWARN)
CFLAGS_L := $(CFOPTIMIZE_L) $(DEFINES_L) $(CFWARN_L) $(INCLUDES_L) $(CFOTHERS_L)
ALL_CFLAGS := $(CFLAGS) $(CFLAGS_L)
YACC_CFLAGS  := $(ALL_CFLAGS) -w -DYYENABLE_NLS=0 -DYYLTYPE_IS_TRIVIAL=0

UTIL = util/

EXTRA_OBJECTS += $(UTIL)levcomp.tab.o $(UTIL)levcomp.lex.o

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
        QUIET_WINDRES  = @echo '   ' WINDRES $@;
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

ifdef BUILD_PCRE
CONTRIBS += pcre
CONTRIB_LIBS += $(LIBPCRE)
endif
ifdef BUILD_FREETYPE
CONTRIBS += freetype
CONTRIB_LIBS += $(LIBFREETYPE)
endif
ifdef BUILD_SDLIMAGE
CONTRIBS += sdl-image
CONTRIB_LIBS += $(LIBSDLIMAGE)
endif
ifdef BUILD_SDL
CONTRIBS += sdl
CONTRIB_LIBS += $(LIBSDL)
endif
ifdef BUILD_LIBPNG
CONTRIBS += libpng
CONTRIB_LIBS += $(LIBPNG)
endif
ifdef BUILD_ZLIB
CONTRIBS += zlib
CONTRIB_LIBS += $(LIBZ)
endif
ifdef BUILD_LUA
ifdef USE_LUAJIT
CONTRIBS += luajit/src
CFOTHER_L += -DUSE_LUAJIT
else
CONTRIBS += lua/src
endif
CONTRIB_LIBS += $(LIBLUA)
endif
ifdef BUILD_SQLITE
CONTRIBS += sqlite
CONTRIB_LIBS += $(LIBSQLITE)
endif

EXTRA_OBJECTS += version.o

LIBS += $(CONTRIB_LIBS) $(EXTRA_LIBS)

DOC_BASE        := ../docs
DOC_TEMPLATES   := $(DOC_BASE)/template
GENERATED_DOCS  := $(DOC_BASE)/aptitudes.txt
GENERATED_FILES := art-data.h $(RLTILES)/dc-unrand.txt

GAME_DEPENDS  := $(DESTTILEFILES) $(OBJECTS) $(EXTRA_OBJECTS) $(CONTRIB_LIBS)
SRC_PKG_BASE  := stone_soup
SRC_VERSION   := $(shell git describe --tags --long 2>/dev/null || cat util/release_ver)

# when making release builds, use just the bare tag
SRC_VERSION_SHORT := $(shell git describe --tags 2>/dev/null || cat util/release_ver)
ifneq (,$(SRC_VERSION_SHORT))
  ifeq (,$(findstring -,$(SRC_VERSION_SHORT)))
    SRC_VERSION := $(SRC_VERSION_SHORT)
  endif
endif

PKG_SRC_DIR   := $(SRC_PKG_BASE)-$(SRC_VERSION)
SRC_PKG_TAR   := $(PKG_SRC_DIR).tar.bz2
SRC_PKG_TAR_NODEPS := $(PKG_SRC_DIR)-nodeps.tar.bz2
SRC_PKG_ZIP   := $(PKG_SRC_DIR).zip

.PHONY: all test install clean clean-contrib distclean debug profile wizard package-source source

all: $(GENERATED_FILES) $(GAME) $(GENERATED_DOCS)

test:
	./$(GAME) -test > /dev/null


ifeq (,$(findstring clean,$(MAKECMDGOALS)))

#
# CFLAGS difference check
#
# Check for flag changes between the previous build and the current one,
# because any CFLAGS change could result in an inconsistent build if the
# person building it isn't careful.
#
# This should eliminate an annoying need to use 'make clean' every time.
#

TRACK_CFLAGS = $(subst ','\'',$(CC) $(CXX) $(ALL_CFLAGS))           # (stray ' for highlights)

.cflags: .force-cflags
	@FLAGS='$(TRACK_CFLAGS)'; \
    if test x"$$FLAGS" != x"`cat .cflags 2>/dev/null`" ; then \
        echo "    * rebuilding crawl: new build flags or prefix"; \
        echo "$$FLAGS" > .cflags; \
    fi

.PHONY: .force-cflags

##########################################################################
# Dependencies

DEPS := $(shell ls $(OBJECTS:.o=.d) 2> /dev/null)

ifneq ($(DEPS),)
-include $(DEPS)
endif

endif

depend: $(OBJECTS:.o=.d)

# This information is included in crash reports, and is printed with
# "crawl -version"
compflag.h: $(OBJECTS:.o=.cc)
	$(QUIET_GEN)util/gen-cflg.pl compflag.h "$(ALL_CFLAGS)" "$(LDFLAGS)"

build.h: $(OBJECTS:.o=.cc)
	$(QUIET_GEN)util/gen_ver.pl $@ $(MERGE_BASE)

version.cc: build.h compflag.h

##########################################################################
# Documentation
#
$(DOC_BASE)/aptitudes.txt: $(DOC_TEMPLATES)/apt-tmpl.txt player.cc skills2.cc \
						   util/gen-apt.pl
	$(QUIET_GEN)./util/gen-apt.pl $@ $^

##########################################################################
# The level compiler
#

$(UTIL)levcomp.tab.o: $(CONTRIB_LIBS)
$(UTIL)levcomp.lex.o: $(CONTRIB_LIBS)

ifndef NO_YACC

prebuildyacc:	$(UTIL)levcomp.tab.cc $(UTIL)levcomp.tab.h $(UTIL)levcomp.lex.cc
		$(QUIET_COPY)$(COPY) $^ prebuilt/

$(UTIL)levcomp.tab.cc: $(UTIL)levcomp.ypp
		+@$(MAKE) -C $(UTIL) levcomp.tab.cc

$(UTIL)levcomp.lex.cc: $(UTIL)levcomp.lpp $(UTIL)levcomp.tab.cc
		+@$(MAKE) -C $(UTIL) levcomp.lex.cc

$(UTIL)levcomp.tab.h: $(UTIL)levcomp.tab.cc

else

prebuildyacc:
		@echo "**** yacc is not installed, aborting."; false

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
install: all
ifeq ($(DESTDIR)$(prefix),)
	@echo Neither "DESTDIR" nor "prefix" defined -- nowhere to install to, aborting.
	@exit 1
endif
	[ -d $(prefix_fp)/$(bin_prefix) ] || mkdir -p $(prefix_fp)/$(bin_prefix)
	$(COPY) $(GAME) $(prefix_fp)/$(bin_prefix)/
	$(STRIP) -s $(prefix_fp)/$(bin_prefix)/$(GAME)
	$(CHOWN) $(INSTALL_UGRP) $(prefix_fp)/$(bin_prefix)/$(GAME) || true
	$(CHMOD) $(MCHMOD) $(prefix_fp)/$(bin_prefix)/$(GAME) || true
	mkdir -p $(datadir_fp)/dat/des
	mkdir -p $(datadir_fp)/dat/clua
	mkdir -p $(datadir_fp)/dat/lua
	mkdir -p $(datadir_fp)/dat/database
	mkdir -p $(datadir_fp)/dat/descript
	mkdir -p $(datadir_fp)/docs/develop
	mkdir -p $(datadir_fp)/docs/develop/levels
	mkdir -p $(datadir_fp)/docs/license
	mkdir -p $(datadir_fp)/settings
	$(COPY_R) dat/des/* $(datadir_fp)/dat/des/
	$(COPY)   dat/clua/*.lua $(datadir_fp)/dat/clua/
	$(COPY)   dat/lua/*.lua $(datadir_fp)/dat/lua/
	$(COPY)   dat/database/*.txt $(datadir_fp)/dat/database/
	$(COPY)   dat/descript/*.txt $(datadir_fp)/dat/descript/
	$(COPY) ../docs/*.txt $(datadir_fp)/docs/
	$(COPY) ../docs/develop/*.txt $(datadir_fp)/docs/develop/
	$(COPY) ../docs/develop/levels/*.txt $(datadir_fp)/docs/develop/levels/
	$(COPY) ../docs/license/*.txt $(datadir_fp)/docs/license/
	$(COPY) ../settings/* $(datadir_fp)/settings/
ifdef TILES
	mkdir -p $(datadir_fp)/dat/tiles
	$(COPY) dat/tiles/*.png $(datadir_fp)/dat/tiles/
ifndef PROPORTIONAL_FONT
	$(COPY) dat/tiles/Vera.ttf $(datadir_fp)/dat/tiles/
endif
ifndef MONOSPACED_FONT
	$(COPY) dat/tiles/VeraMono.ttf $(datadir_fp)/dat/tiles/
endif
endif
ifeq ($(USE_DGAMELAUNCH),)
	$(CHOWN) -R $(INSTALL_UGRP) $(datadir_fp) || true
endif
ifneq ($(SAVEDIR),)
	mkdir -p $(savedir_fp)/saves
ifneq ($(patsubst /var/%,%,$(savedir_fp)),$(savedir_fp))
# Only if we're being installed for real.  Installations to a staging dir
# which are then packaged would trample existing files; these need to be
# handled by packagers themselves.
	touch $(savedir_fp)/saves/logfile
	touch $(savedir_fp)/saves/scores
endif
	mkdir -p $(savedir_fp)/morgue
ifeq ($(USE_DGAMELAUNCH),)
	$(CHOWN) -R $(INSTALL_UGRP) $(savedir_fp) || true
	$(CHMOD) $(MCHMOD_SAVEDIR) $(savedir_fp) || true
	$(CHMOD) $(MCHMOD_SAVEDIR) $(savedir_fp)/saves || true
	$(CHMOD) $(MCHMOD_LOGS) $(savedir_fp)/saves/logfile || true
	$(CHMOD) $(MCHMOD_LOGS) $(savedir_fp)/saves/scores || true
	$(CHMOD) $(MCHMOD_SAVEDIR) $(savedir_fp)/morgue || true
endif
endif

clean:
	+$(MAKE) -C $(UTIL) clean
	+$(MAKE) -C $(RLTILES) clean
	$(RM) $(GAME) $(GAME).exe $(GENERATED_FILES) $(EXTRA_OBJECTS) libw32c.o libunix.o $(OBJECTS) $(OBJECTS:.o=.d) *.ixx build.h compflag.h .contrib-libs .cflags

clean-contrib:
	+$(MAKE) -C contrib clean

distclean: clean clean-contrib clean-rltiles
	$(RM) -r morgue saves
	$(RM) scores $(GAME) core $(DEPENDENCY_MKF)

$(GAME): $(GAME_DEPENDS)
	$(QUIET_LINK)$(CXX) $(LDFLAGS) $(EXTRA_OBJECTS) $(OBJECTS) -o $(GAME) $(LIBS)

debug: all
profile: all
wizard: all

# [ds] Note we don't use the standard CFLAGS here; that's intentional, most
# flex/bison combos I've tried don't produce code that passes the warnings
# test.

$(UTIL)%.o: $(UTIL)%.cc .cflags
	$(QUIET_CXX)$(CXX) $(YACC_CFLAGS) -Wp,-MD,$(UTIL)$*.d -c $< -o $(UTIL)$*.o

ifdef PCH
%.h.gch: %.h
	$(QUIET_PCH)$(CXX) $(ALL_CFLAGS) -c $< -o $@

CC_DEP := AppHdr.h.gch
endif

$(OBJECTS:%.o=%.cc): $(CC_DEP) $(TILEDEFHDRS) $(CONTRIB_LIBS)

ifdef NO_INLINE_DEPGEN
$(OBJECTS): $(OBJECTS:%.o=%.d)
endif

%.d: %.cc .cflags $(GENERATED_FILES)
	$(QUIET_DEPEND)$(CXX) -MM $(ALL_CFLAGS) -MT $*.o $< > $*.d

%.o: %.m .cflags $(GENERATED_FILES)
ifdef NO_INLINE_DEPGEN
	$(QUIET_CC)$(CC) $(ALL_CFLAGS) -c $< -o $@
else
	$(QUIET_CC)$(CC) $(ALL_CFLAGS) -Wp,-MD,$*.d,-MT,$@ -c $< -o $@
endif

%.o: %.cc .cflags $(GENERATED_FILES)
ifdef NO_INLINE_DEPGEN
	$(QUIET_CXX)$(CXX) $(ALL_CFLAGS) -c $< -o $@
else
	$(QUIET_CXX)$(CXX) $(ALL_CFLAGS) -Wp,-MD,$*.d,-MT,$@ -c $< -o $@
endif

icon.o: util/crawl.rc util/crawl.ico
	$(QUIET_WINDRES)$(WINDRES) util/crawl.rc icon.o

#
# Contribs
#

$(CONTRIB_LIBS): .contrib-libs
	@:

.contrib-libs: .cflags
ifneq (,$(CONTRIBS))
	  +@$(MAKE) -C contrib $(CONTRIBS)
endif
	@touch $@

$(foreach t,$(CONTRIB_LIBS),$(if $(wildcard $t),,$(shell rm -f .contrib-libs)))

#############################################################################
# Build unrandart data
$(GENERATED_FILES): art-data.txt util/art-data.pl art-func.h
	util/art-data.pl

#############################################################################
# RLTiles
#

.PHONY: rltile-build
rltile-build: .contrib-libs $(GENERATED_FILES)
	+$(MAKE) -C $(RLTILES) all

$(TILEDEFSRCS): rltile-build
$(TILEDEFHDRS): rltile-build
$(ORIGTILEFILES): rltile-build

dat/tiles/%.png: $(RLTILES)/%.png
	$(QUIET_COPY)$(COPY) $< $@

clean-rltiles:
	$(RM) $(DESTTILEFILES)
	+$(MAKE) -C $(RLTILES) distclean

#############################################################################
# Packaging a source tarball for release
#

# To package, you *must* have lex and yacc to generate the intermediates.
BSRC = build/crawl-ref/source/
package-source: prebuildyacc
	+@$(MAKE) source

source: removeold
	@git branch >/dev/null 2>/dev/null || (echo You can package source only from git. && false)
	rm -rf build
	mkdir build
	(cd ../..;git ls-files| \
		grep -v -f crawl-ref/source/misc/src-pkg-excludes.lst| \
		tar cf - -T -)|tar xf - -C build
	for x in lua pcre sqlite libpng freetype sdl sdl-image zlib; \
	  do \
	   mkdir -p $(BSRC)contrib/$$x; \
	   (cd contrib/$$x;git ls-files|tar cf - -T -)| \
		tar xf - -C $(BSRC)contrib/$$x; \
	  done
	find build -name .gitignore -execdir rm -f '{}' +
	(git describe --tags --long $(MERGE_BASE) 2> /dev/null || \
	  git describe --tags $(MERGE_BASE) 2> /dev/null) \
	  > $(BSRC)util/release_ver
	cd build && mv crawl-ref $(PKG_SRC_DIR)
	cd build && tar cfj ../../../$(SRC_PKG_TAR) $(PKG_SRC_DIR)
	@if which zip >/dev/null; then \
	  echo "cd build && zip -rq ../../../$(SRC_PKG_ZIP) $(PKG_SRC_DIR)"; \
	  cd build && zip -rq ../../../$(SRC_PKG_ZIP) $(PKG_SRC_DIR); \
	else \
	  echo "**** No ZIP installed -- not creating the zipball."; \
	fi
	cd build && rm -rf $(PKG_SRC_DIR)/source/contrib \
		 && tar cjf ../../../$(SRC_PKG_TAR_NODEPS) $(PKG_SRC_DIR)
	rm -rf build

removeold:
	if [ -f ../../$(SRC_PKG_TAR) ]; then $(RM) ../../$(SRC_PKG_TAR); fi
	if [ -f ../../$(SRC_PKG_ZIP) ]; then $(RM) ../../$(SRC_PKG_ZIP); fi
