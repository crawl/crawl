# vim:set ts=8 sw=4 noexpandtab:
#
# Dungeon Crawl Stone Soup
# GNU Makefile
#
# largely written by Steven Noonan <steven@uplinklabs.net>
#    (if something breaks, blame him.)
#

# Typical targets:
#    make
#    make debug
#    make debug-lite    # Like "make debug", but without all the spew.
#    make install
#    make debug install
#    -- note, unlike most programs, you need to specify build type when
#       installing even if you just built it.
# Typical parameters:
#    TILES         -- set to anything to enable tiles build
#
#    SOUND         -- set to anything to enable sound; note that you will need to
#                     uncomment some lines in sound.h if not building tiles
#
#    CROSSHOST     -- target system, eg, i386-pc-msdosdjgpp or i586-mingw32msvc
#
#    prefix        -- installation base.  Specify eg. /usr/local on Unix systems.
#    DESTDIR       -- installation staging area (the dir you intend to pack)
#    DATADIR       -- place to hold immutable files.  Can be either relative to
#                     "prefix" or absolute.
#    SAVEDIR       -- place to hold writeable data (saves, database, morgue
#                     dumps).  Can be relative to "prefix", absolute or placed
#                     in the user's home dir (~).  Remember to protect the ~
#                     from your shell!
#                     Warning, as of 0.8, directories shared between
#                     multiple system users are no longer supported.
#    SHAREDDIR     -- place to hold writeable data common to multiple
#                     versions of Crawl (scores, the logfile, ghosts).  Will
#                     be placed inside the SAVEDIR if not specified.
#       Layout examples:
#         prefix=~/crawl DATADIR=data/ SAVEDIR=saves/
#                  -- everything under ~/crawl
#         prefix=/usr/local
#                  -- system-wide installation
#
#    V             -- set to anything to enable verbose build
#
#    USE_ICC       -- set to use Intel's compiler
#    LTO           -- set for better optimization but slower compilation,
#                     requires gcc4.6+
#    NO_TRY_LLD    -- if set don't try to detect a working lld linker
#    NO_TRY_GOLD   -- if set don't try to detect a working gold linker
#    NOASSERTS     -- set to disable assertion checks (ignored in debug mode)
#    NOWIZARD      -- set to disable wizard mode.  Use if you have untrusted
#                     remote players without DGL.
#
#    PROPORTIONAL_FONT -- set to a .ttf file you want to use for a proportional
#                         font; if not set, a copy of Bitstream Vera Sans
#                         shipped with Crawl will be used
#    MONOSPACED_FONT   -- monospaced font; Bitstream Vera Mono Sans
#    COPY_FONTS    -- force installing fonts
#
#    WEBTILES      -- set to anything to compile for Webtiles
#    WEBDIR        -- place to hold the Webtiles client data. Can be either
#                     relative to prefix or absolute.
#
#    LINUXDEPLOY   -- path to linuxdeploy tool, required for AppImage builds
#    ANDROID       -- perform an Android build (see docs/develop/android.txt)
#
# For various mac build tricks oriented at release builds see the various
# mac-app-* targets as well as the crawl-universal target. (For normal dev
# circumstances, none of this is needed.)
#
# There are a number of #defines that may be useful for downstream packaging
# or custom server setups. See AppHdr.h for more details. Of note:
#
#    VERSIONED_CACHE_DIR -- use a cache dir per version, rather than a shared
#                           cache dir across versions. This may be useful for
#                           reproducible builds, since the shared cache
#                           directory makes use of mtimes.
#
# To set defines from the command line while building, use EXTERNAL_DEFINES.
# E.g.: make EXTERNAL_DEFINES="-DVERSIONED_CACHE_DIR"
#
# Requirements:
#    For tile builds, you need pkg-config.
#    You also need libpng, sdl2, sdl2-image, sdl2-mixer and libfreetype -- if
#    you got your source from git, you can 'git submodule update' to fetch
#    them; you can also ask for a package with convenience libraries instead --
#    we'll try to provide them somewhere in the near future.

# allow overriding GAME at the command line, intended for downstream packaging
# processes. Not supported for MSYS2/cygwin builds, and will be overridden for
# mac universal builds. This does not count as a change in build flags.
ifndef GAME
	GAME = crawl
endif

# Disable GNU Make implicit rules and variables. Leaving them enabled will slow
# down MinGW and Cygwin builds by a very VERY noticeable degree. Besides, we have
# _explicit_ rules defined for everything. So we don't need them.
MAKEFLAGS += -rR # This only works for recursive makes, i.e. contribs ...
.SUFFIXES:       # ... so zap the suffix list to neutralize most predefined rules, too

.PHONY: all test install clean clean-contrib clean-rltiles clean-android \
        clean-appimage clean-catch2 clean-plug-and-play-tests \
        clean-coverage clean-coverage-full \
        appimage distclean debug debug-lite profile package-source source \
        build-windows package-windows-installer docs greet api api-dev android FORCE \
        monster catch2-tests plug-and-play-tests \
        crawl-universal crawl-arm64-apple-macos11 crawl-x86_64-apple-macos10.7 clean-mac

include Makefile.obj

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

# Which C++ standard to support
STDFLAG = -std=c++11

CFOTHERS := -pipe $(EXTERNAL_FLAGS)
# Build with FORCE_SSE=y to get better seed stability on 32 bit x86 builds. It
# is not recommended to do this unless you are building with contrib lua.
# On any 64bit  builds where the arch supports it, (at least) sse2 is implied.
ifdef FORCE_SSE
CFOTHERS += -mfpmath=sse -msse2
endif

CFWARN :=
CFWARN_L := -Wall -Wformat-security -Wundef

# Exceptions:
#  -Wmissing-field-initializers: need c++14 to set default struct fields
#  -Wimplicit-fallthrough: need c++17 for [[fallthrough]]
#  -Wtype-limits: mostly complains about ASSERT_RANGE, and _Pragma is buggy on
#      old g++ anyway: re-enable when we bump the minimum g++ version
#  -Wuninitialized: added later in this makefile, conditional on NO_OPTIMIZE
CFWARN_L += -Wextra \
	    -Wno-missing-field-initializers \
	    -Wno-implicit-fallthrough \
	    -Wno-type-limits \
	    -Wno-uninitialized \

DEFINES := $(EXTERNAL_DEFINES)

ifndef ANDROID
LDFLAGS :=
endif

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
STRIP = strip -s
SED = sed
WINDRES = windres
CHMOD = chmod 2>/dev/null
CHOWN = chown 2>/dev/null
PNGCRUSH = $(COPY)
PNGCRUSH_LABEL = COPY
ADVPNG = advpng -z -2
PKGCONFIG = pkg-config
DOXYGEN = doxygen
DOXYGEN_SIMPLE_CONF = crawl_simple.doxy
DOXYGEN_ALL_CONF = crawl_all.doxy
DOXYGEN_HTML_GEN = html/
# Prefer python3, if available
PYTHON = python
ifneq (, $(shell which python3 2>&1))
	PYTHON = python3
endif

export AR
export RANLIB
export RM
export CC
export CXX
export CFLAGS
export STDFLAG
export MAKEFLAGS
export CONFIGURE_FLAGS
export uname_S

#
# Platform Detection
#
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')

HOST := $(shell sh -c 'cc -dumpmachine || echo unknown')
ARCH := $(HOST)

ifdef CROSSHOST
	# XX is there a way of integrating this with mac cross-compiling?
	ARCH := $(CROSSHOST)
	ifneq (,$(shell which $(ARCH)-pkg-config 2>/dev/null))
	  PKGCONFIG = $(ARCH)-pkg-config
	else
	  ifneq (,$(wildcard /usr/$(ARCH)/lib/pkgconfig))
	    PKGCONFIG = PKG_CONFIG_LIBDIR=/usr/$(ARCH)/lib/pkgconfig pkg-config
	  else
	    NO_PKGCONFIG = YesPlease
	    BUILD_LUA = yes
	    BUILD_SQLITE = yes
	    BUILD_ZLIB = YesPlease
	  endif
	endif
	NO_AUTO_OPT = YesPlease
	CONFIGURE_FLAGS += --host=$(CROSSHOST)

	# If needed, override uname_S so we get the appropriate
	# things compiled.
	ifneq (,$(findstring mingw,$(CROSSHOST)))
		uname_S=MINGW32
	endif

endif

msys =
ifneq (,$(findstring MINGW,$(uname_S)))
	msys = Yes
endif
ifneq (,$(findstring MSYS,$(uname_S)))
	msys = Yes
endif
ifdef msys
	GAME = crawl.exe
	bin_prefix = .
	WIN32 = Yes
	NO_NCURSES = YesPlease
	NEED_LIBW32C = YesPlease
	BUILD_PCRE = YesPlease
	BUILD_ZLIB = YesPlease
	BUILD_SQLITE = YesPlease
	SOUND = YesPlease
	DEFINES_L += -DWINMM_PLAY_SOUNDS -D__USE_MINGW_ANSI_STDIO
	EXTRA_LIBS += -lwinmm
	ifdef TILES
		EXTRA_LIBS += -lmingw32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion contrib/install/$(ARCH)/lib/libSDL2main.a -mwindows
		BUILD_FREETYPE = YesPlease
		BUILD_SDL2 = YesPlease
		BUILD_SDL2IMAGE = YesPlease
		BUILD_LIBPNG = YesPlease
		COPY_FONTS = yes
	endif
	ifeq ($(shell gcc -v -static -static-libstdc++ 2>&1 | grep 'unrecognized option'),)
		EXTRA_LIBS += -static -static-libgcc -static-libstdc++
	endif
endif
ifeq ($(uname_S),Darwin)
ifdef MAC_TARGET
	# for reasons I really cannot figure out, setting GAME later (when the
	# other MAC_TARGET checks are done) really does not work.
	GAME = crawl-$(MAC_TARGET)
endif

	STRIP := strip -x
	NEED_APPKIT = YesPlease
	LIBNCURSES_IS_UNICODE = Yes
	ifdef TILES
		EXTRA_LIBS += -framework AppKit -framework AudioUnit -framework CoreAudio -framework ForceFeedback -framework Carbon -framework IOKit -framework OpenGL -framework AudioToolbox -framework CoreVideo
		COPY_FONTS = yes
	endif
	ifndef FORCE_PKGCONFIG
		NO_PKGCONFIG = Yes
		# is any of this stuff actually needed if NO_PKGCONFIG is set?
		BUILD_SQLITE = YesPlease
		BUILD_ZLIB = YesPlease
		ifdef TILES
			EXTRA_LIBS += contrib/install/$(ARCH)/lib/libSDL2main.a
			BUILD_FREETYPE = YesPlease
			BUILD_SDL2 = YesPlease
			BUILD_SDL2IMAGE = YesPlease
			ifdef SOUND
				ifndef WIN32
					BUILD_SDL2MIXER = YesPlease
				endif
			endif
			BUILD_LIBPNG = YesPlease
		endif
	endif
endif
ifneq (,$(findstring CYGWIN,$(uname_S)))
	GAME = crawl.exe
	BUILD_PCRE = YesPlease
	# newlib doesn't provide Posix functions with c++11
	STDFLAG = -std=gnu++11
endif

ifdef BUILD_ALL
	ifdef TILES
		BUILD_FREETYPE = YesPlease
		BUILD_SDL2 = YesPlease
		BUILD_SDL2IMAGE = YesPlease
		BUILD_LIBPNG = YesPlease
	endif
	ifdef WEBTILES
		BUILD_LIBPNG = YesPlease
	endif

	BUILD_PCRE = YesPlease
	ifdef SOUND
		ifndef WIN32
			BUILD_SDL2MIXER = YesPlease
		endif
	endif
	BUILD_SQLITE = YesPlease
	BUILD_LUA = YesPlease
	BUILD_ZLIB = YesPlease
endif

# Should be the first rule, but must come after $(GAME) is set.
all: greet check-fonts $(GAME) docs webserver

# XX these are set wrong on mac cross-compiling
LIBPCRE := contrib/install/$(ARCH)/lib/libpcre.a
LIBSDL2 := contrib/install/$(ARCH)/lib/libSDL2.a
LIBPNG := contrib/install/$(ARCH)/lib/libpng.a
LIBSDL2IMAGE := contrib/install/$(ARCH)/lib/libSDL2_image.a
LIBSDL2MIXER := contrib/install/$(ARCH)/lib/libSDL2_mixer.a
LIBFREETYPE := contrib/install/$(ARCH)/lib/libfreetype.a
LIBSQLITE := contrib/install/$(ARCH)/lib/libsqlite3.a
ifdef USE_LUAJIT
LIBLUA := contrib/install/$(ARCH)/lib/libluajit.a
else
LIBLUA := contrib/install/$(ARCH)/lib/liblua.a
endif
LIBZ := contrib/install/$(ARCH)/lib/libz.a

#
# Set up the TILES variant
#
ifdef TILES
TILES_ANY = YesPlease
OBJECTS += $(TILES_OBJECTS) $(GLTILES_OBJECTS)
  ifdef WEBTILES
    OBJECTS += $(error Web and local tiles are exclusive)
  endif
endif

ifdef WEBTILES
TILES_ANY = YesPlease
OBJECTS += $(TILES_OBJECTS) $(WEBTILES_OBJECTS)
ifdef NO_PKGCONFIG
BUILD_LIBPNG = YesPlease # used by build-rltiles sub-make
endif
endif

ifdef ANDROID
TILES_ANY = YesPlease
endif

#
# Check for an Apple-released compiler.
#
ifndef NO_APPLE_PLATFORM
ifeq ($(uname_S),Darwin)
ifneq ($(shell gcc -v 2>&1 | grep Apple),)
APPLE_PLATFORM = YesPlease
endif
endif
endif


ifdef WIN32
EXTRA_OBJECTS += icon.o
else
ifndef ANDROID
ifneq ($(uname_S),Haiku)
EXTRA_LIBS += -pthread
endif
endif
endif

ifndef TILES
ifdef NEED_LIBW32C
OBJECTS += libw32c.o
else
OBJECTS += libunix.o
endif
endif

ifdef USE_MERGE_BASE
MERGE_BASE := $(shell git merge-base HEAD $(USE_MERGE_BASE))
endif

ifdef USE_DGAMELAUNCH
# Permissions to set on the game executable.
MCHMOD := 755

# Permissions to set on the save directory.
MCHMOD_SAVEDIR := 775
MCHMOD_LOGS := 664

# The user:group to install the game as.
INSTALL_UGRP := games:games
endif

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
# If you're installing to /usr, /usr/local or /opt, we have sane defaults.

# SAVEDIR := saves/
# DATADIR := data/
ifneq (,$(FHS))
DATADIR       := share/crawl
SAVEDIR       := ~/.crawl
endif

ifneq ($(patsubst /opt%,%,$(prefix)),$(prefix))
DATADIR := data
SAVEDIR := ~/.crawl
endif


INCLUDES_L += -Iutil -I.


ifdef APPLE_PLATFORM

ifdef MAC_TARGET
	# support mac cross-compiling. As of Apr 2021 on OS X 11, some example
	# values for this that seem to work include:
	#   x86_64-apple-macos10.7
	#   x86_64-apple-macos10.12
	#   arm64-apple-macos11
	# leave MACOSX_MIN_VERSION unset, or there will be many warnings
	CFLAGS_ARCH := -target $(MAC_TARGET)
	# couldn't get setting GAME here to work?
else
ifdef BUILD_OLD_UNIVERSAL
	# XX can be removed?
	# [ds] 10.4 SDK g++-4.0 + x86_64 runs into SDL compile issues.
	CFLAGS_ARCH := -arch i386 -arch ppc -faltivec
	CFLAGS_DEPCC_ARCH := -arch i386
	NO_INLINE_DEPGEN := YesPlease
else
	CFLAGS_ARCH := -arch $(uname_M)
	MACOSX_MIN_VERSION=10.7
endif
endif

# only set this when no -target is passed
ifdef MACOSX_MIN_VERSION
	CFLAGS_ARCH += -mmacosx-version-min=$(MACOSX_MIN_VERSION)
	CFLAGS_DEPCC_ARCH += -mmacosx-version-min=$(MACOSX_MIN_VERSION)
endif
CC = $(GCC) $(CFLAGS_ARCH)
CXX = $(GXX) $(CFLAGS_ARCH) -stdlib=libc++
DEPCC = $(GCC) $(or $(CFLAGS_DEPCC_ARCH),$(CFLAGS_ARCH))
DEPCXX = $(GXX) $(or $(CFLAGS_DEPCC_ARCH),$(CFLAGS_ARCH)) -stdlib=libc++

ifdef USE_ICC
CC += -gcc-name=gcc-$(GCC_VER) -gxx-name=g++-$(GCC_VER)
CXX += -gcc-name=gcc-$(GCC_VER) -gxx-name=g++-$(GCC_VER)
endif

ifdef BUILD_OLD_UNIVERSAL
# XX remove?
ifndef CROSSHOST
# Don't need a universal build of host tools, so use DEPCC.
HOSTCC = $(DEPCC)
HOSTCXX = $(DEPCXX)
export HOSTCC
export HOSTCXX
endif
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
ifeq ($(shell which $(LMACH)gcc$(GCC_VER_SUFFIX) > /dev/null 2> /dev/null && echo "Yes"),)
LMACH :=
endif

ifneq ($(FORCE_CC),)
GCC := $(FORCE_CC)
HOSTCC := $(FORCE_CC)
export HOSTCC
else
GCC := $(LMACH)$(GCC_VER_PREFIX)gcc$(GCC_VER_SUFFIX)
endif

ifneq ($(FORCE_CXX),)
GXX := $(FORCE_CXX)
HOSTCXX := $(FORCE_CXX)
export HOSTCXX
else
GXX := $(LMACH)$(GCC_VER_PREFIX)g++$(GCC_VER_SUFFIX)
endif

else

# Cross-compiling is a weird case.
GCC := $(CROSSHOST)-gcc
GXX := $(CROSSHOST)-g++
AR := $(CROSSHOST)-ar
RANLIB := $(CROSSHOST)-ranlib
STRIP := $(CROSSHOST)-strip
WINDRES := $(CROSSHOST)-windres

endif

ifdef USE_ICC
# If you have a Core 2 processor, this _really_ makes things fly:
#CFOPTIMIZE := -O2 -parallel -xT

# Optionally enable the 'ipo' feature, to facilitate inlining
# across object file bounds.
#CFOPTIMIZE_L := -ipo

# Some very good optimization flags.
  CFOPTIMIZE := -O2 -parallel
else

  ifneq (,$(shell $(GXX) --version|grep 'g++.*4\.2\.'))
    # OS X uses a buggy ancient version of gcc without fixes from even
    # subsequent point releases of 4.2.
    CFOPTIMIZE := -O0
  else
    CFOPTIMIZE := -O2
  endif
endif

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
CFOPTIMIZE += -march=native
endif
endif

ifdef LTO
  ifeq ($(shell $(GXX) -v 2>&1|grep clang),)
    CFOPTIMIZE += -flto=jobserver -fwhole-program
    # FIXME: this check is fragile, and should be done via a full compile test.
    ifeq ($(shell $(GXX) -x c++ /dev/null -fno-fat-lto-objects 2>&1 | grep 'unrecognized command line option'),)
      CFOPTIMIZE += -fno-fat-lto-objects
    endif
  else
    CFOPTIMIZE += -flto
  endif
endif

# To get stack trace symbols.
# Note that Msys, MinGW, ICC, and Cygwin don't support -rdynamic.
ifeq (,$(shell echo 'int main(){return 1;}'|$(GXX) -x c++ - -o /dev/null -rdynamic 2>&1))
  LDFLAGS += -rdynamic
endif

# Okay, we have to assume we're on something else that
# uses standard UNIX-like methods for finding libs.
#
# For instance, on Linux and most other UNIX-likes,
# the app pkg-config can provide the appropriate
# CFLAGS and LDFLAGS.
#

ifndef NO_PKGCONFIG
ifeq ($(shell which $(PKGCONFIG) 2> /dev/null),)
NO_PKGCONFIG = YesPlease
endif
endif

ifdef TILES
  XDG_ORIG := org.develz.Crawl_tiles
  ifndef XDG_NAME
    XDG_NAME := org.develz.Crawl_tiles
  endif
else
  XDG_ORIG := org.develz.Crawl_console
  ifndef XDG_NAME
    XDG_NAME := org.develz.Crawl_console
  endif
endif

ifdef LINUXDEPLOY
  prefix=usr
  DESTDIR=appimage/
  SAVEDIR=~/.crawl
  COPY_FONTS=yes
  export VERSION=$(SRC_VERSION)
ifdef TILES
  appimage_build := tiles
else
  appimage_build := console
endif
  export OUTPUT = stone_soup-$(SRC_VERSION)-linux-$(appimage_build).$(uname_M).AppImage
endif

ifdef ANDROID
  BUILD_LUA=
  BUILD_SQLITE=
  BUILD_ZLIB=
  BUILD_SDL2=
  BUILD_FREETYPE=
  COPY_FONTS = yes
  NO_PKGCONFIG=Y
  GLES=Y
  TILES=Y
  SOUND=Y
  SAVEDIR=
else

ifndef BUILD_LUA
  ifdef NO_PKGCONFIG
    BUILD_LUA = yes
  endif
endif
# You can override the Lua package search with "make LUA_PACKAGE=lua-5-1".
# pkg-config is still required.
ifndef LUA_PACKAGE
  ifndef BUILD_LUA
    ifneq ($(USE_LUAJIT),)
      ifeq ($(shell $(PKGCONFIG) luajit --exists || echo no),)
        LUA_PACKAGE = luajit
      endif
    else
      ifneq ($(shell $(PKGCONFIG) lua5.1 --exists || echo no),)
        ifneq ($(shell $(PKGCONFIG) lua-5.1 --exists || echo no),)
          ifneq ($(shell $(PKGCONFIG) lua51 --exists || echo no),)
            ifeq ($(shell $(PKGCONFIG) lua --exists || echo no),)
              ifeq ($(shell $(PKGCONFIG) lua --modversion | head -c 3),5.1)
                LUA_PACKAGE = lua
              endif
            endif
          else
            LUA_PACKAGE = lua51
          endif
        else
          LUA_PACKAGE = lua-5.1
        endif
      else
        LUA_PACKAGE = lua5.1
      endif
    endif
  endif
endif
ifndef LUA_PACKAGE
  BUILD_LUA = yes
endif

ifndef BUILD_LUA
  ifndef LUA_PACKAGE
    LUA_PACKAGE = lua5.1
  endif
  INCLUDES_L += $(shell $(PKGCONFIG) $(LUA_PACKAGE) --cflags-only-I | sed -e 's/-I/-isystem /')
  CFLAGS_L  += $(shell $(PKGCONFIG) $(LUA_PACKAGE) --cflags-only-other)
  LIBS += $(shell $(PKGCONFIG) $(LUA_PACKAGE) --libs)
endif

ifdef USE_LUAJIT
DEFINES_L += -DUSE_LUAJIT
endif

ifndef BUILD_SQLITE
  ifdef NO_PKGCONFIG
    BUILD_SQLITE = yes
  endif
endif
ifndef BUILD_SQLITE
 ifneq ($(shell $(PKGCONFIG) sqlite3 --exists || echo no),)
   BUILD_SQLITE = yes
 else
   INCLUDES_L += $(shell $(PKGCONFIG) sqlite3 --cflags-only-I | sed -e 's/-I/-isystem /')
   CFLAGS_L += $(shell $(PKGCONFIG) sqlite3 --cflags-only-other)
   LIBS += $(shell $(PKGCONFIG) sqlite3 --libs)
  endif
endif

ifndef BUILD_ZLIB
  LIBS += -lz
else
  LIBS += $(LIBZ)
endif
endif #ANDROID

RLTILES = rltiles

ifdef WEBTILES
DEFINES_L += -DUSE_TILE
DEFINES_L += -DUSE_TILE_WEB
endif

#
# Tiles build stuff
#
ifdef TILES

DEFINES_L += -DUSE_TILE
DEFINES_L += -DUSE_TILE_LOCAL

ifdef BUILD_SDL2
INCLUDES_L += -isystem contrib/install/$(ARCH)/include/SDL2
endif
ifdef BUILD_FREETYPE
INCLUDES_L += -isystem contrib/install/$(ARCH)/include/freetype2
endif

DEFINES_L += -DUSE_SDL -DUSE_GL -DUSE_FT

ifdef GLES
DEFINES_L += -DUSE_GLES
endif

ifndef NO_PKGCONFIG

# If pkg-config is available, it's the surest way to find where
# the contributing libraries are located.
#

ifndef BUILD_FREETYPE
FREETYPE_INCLUDE := $(shell $(PKGCONFIG) freetype2 --cflags-only-I | sed -e 's/-I/-isystem /' )
FREETYPE_CFLAGS  := $(shell $(PKGCONFIG) freetype2 --cflags-only-other)
FREETYPE_LDFLAGS := $(shell $(PKGCONFIG) freetype2 --libs-only-L) $(shell $(PKGCONFIG) freetype2 --libs-only-l)
endif

ifndef BUILD_SDL2
SDL2_INCLUDE := $(shell $(PKGCONFIG) sdl2 --cflags-only-I | sed -e 's/-I/-isystem /')
SDL2_CFLAGS  := $(shell $(PKGCONFIG) sdl2 --cflags-only-other)
SDL2_LDFLAGS := $(shell $(PKGCONFIG) sdl2 --libs-only-L) $(shell $(PKGCONFIG) sdl2 --libs-only-l)
endif

ifdef SOUND
ifndef WIN32
LIBS += -lSDL2_mixer
endif
endif
LIBS += -lSDL2_image $(SDL2_LDFLAGS) $(FREETYPE_LDFLAGS)

endif # pkg-config

ifndef GLES
ifneq ($(uname_S),Darwin)
ifeq ($(msys),Yes)
LIBS += -lopengl32 -lglu32
else
LIBS += -lGL -lGLU
endif
endif
endif

DEFINES_L += $(PNG_CFLAGS) $(FREETYPE_CFLAGS) $(SDL2_CFLAGS)
INCLUDES_L += $(PNG_INCLUDE) $(FREETYPE_INCLUDE) $(SDL2_INCLUDE)

endif # TILES

ifdef SOUND
DEFINES_L += -DUSE_SOUND
endif

# On clang, unknown -Wfoo is merely a warning, thus -Werror.
# For `no-` options, gcc will only emit an error if there are other errors, so
# we need to check positive forms (applies to array-bounds, format-zero-length,
# and tautological-unsigned-enum-zero-compare.)
CFWARN_L += $(shell w=array-bounds;echo|$(GXX) -E -x c++ - -Werror -W$$w >/dev/null 2>&1 && echo -Wno-$$w)
CFWARN_L += $(shell w=format-zero-length;echo|$(GXX) -E -x c++ - -Werror -W$$w >/dev/null 2>&1 && echo -Wno-$$w)
CFWARN_L += $(shell w=-Wmissing-declarations;echo|$(GXX) -E -x c++ - -Werror $$w >/dev/null 2>&1 && echo $$w)
CFWARN_L  += $(shell w=-Wredundant-decls;echo|$(GXX) -E -x c++ - -Werror $$w >/dev/null 2>&1 && echo $$w)
# Avoid warnings about safety checks that might be necessary on other compilers
# where the same enum is assigned a signed underlying type.
CFWARN_L  += $(shell w=tautological-unsigned-enum-zero-compare;echo|$(GXX) -E -x c++ - -Werror -W$$w >/dev/null 2>&1 && echo -Wno-$$w)

CFWARN_L += -Wno-parentheses -Wwrite-strings -Wshadow -pedantic
CFOTHERS_L = $(EXTERNAL_FLAGS_L) $(EXTRA_FLAGS) $(DEFINES) $(SDL2_CFLAGS)

ifndef NO_LUA_BINDINGS
CFOTHERS_L += -DCLUA_BINDINGS
endif

ifdef USE_DGAMELAUNCH
SRC_BRANCH    := $(shell git rev-parse --abbrev-ref HEAD || echo release)
ifneq ($(SRC_BRANCH),$(filter master release stone_soup-%, $(SRC_BRANCH)))
  DEFINES_L += -DEXPERIMENTAL_BRANCH="\"$(SRC_BRANCH)\""
endif
endif

#
# Figure out the build settings for this type of build
#

# Debug
# No optimization, full debugging.
ifneq (,$(filter debug,$(MAKECMDGOALS)))
	FULLDEBUG=YesPlease
	DEBUG=YesPlease
	NO_OPTIMIZE=YesPlease
	COVERAGE=YesPlease
endif

# Debug-Lite
# No optimization, but little/no debugging spew.
ifneq (,$(filter debug-lite,$(MAKECMDGOALS)))
	DEBUG=YesPlease
	NO_OPTIMIZE=YesPlease
	COVERAGE=YesPlease
endif

# Profile
# Optimized, with full debugging.
ifneq (,$(filter profile,$(MAKECMDGOALS)))
	FULLDEBUG=YesPlease
	DEBUG=YesPlease
	COVERAGE=YesPlease
endif

# Unit tests
ifneq (,$(filter catch2-tests,$(MAKECMDGOALS)))
	COVERAGE=YesPlease
	# current catch2 doesn't support c++11
	STDFLAG = -std=c++14
endif

ifdef HURRY
	NO_OPTIMIZE=YesPlease
endif

ifdef COVERAGE
    # GCC
    ifeq ($(shell $(GXX) -v 2>&1|grep clang),)
		CFOPTIMIZE_L = -fprofile-arcs -ftest-coverage
    else
    # Clang
		CFOPTIMIZE_L = -fprofile-instr-generate -fcoverage-mapping
    endif
endif

ifdef FULLDEBUG
DEFINES += -DFULLDEBUG
endif
ifdef DEBUG
CFOTHERS := -ggdb $(CFOTHERS)
DEFINES += -DDEBUG
endif
ifndef NOWIZARD
DEFINES += -DWIZARD
endif
ifdef NO_OPTIMIZE
CFOPTIMIZE  := -O0
endif

ifdef PCH
CFWARN_L += -Winvalid-pch

ifdef PCH_BYTHEBOOK
# Follow the suggestions in ccache(1)
CFOTHERS_L += -fpch-preprocess
export CCACHE_SLOPPINESS = time_macros
# ... for everything but version.cc, which uses __DATE__ and __TIME__
version.o: CCACHE_SLOPPINESS =
else
# Just keep ccache blisfully ignorant of the precompiled header
export CCACHE_CPP2 = yes
endif
endif

ifndef NOASSERTS
DEFINES += -DASSERTS
endif

# Cygwin has a panic attack if we do this...
ifndef NO_OPTIMIZE
CFWARN_L += -Wuninitialized
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
  ifneq ($(filter ~%,$(SAVEDIR))$(ANDROID),)
    CFOTHERS_L += -DSAVE_DIR_PATH=\"$(SAVEDIR)\"
    savedir_fp :=
    shareddir_fp :=
  else
    ifeq ($(filter /%,$(SAVEDIR)),)
      ifneq ($(prefix),)
	override SAVEDIR := $(strip $(prefix))/$(strip $(SAVEDIR))
      endif
    endif
    CFOTHERS_L += -DSAVE_DIR_PATH=\"$(abspath $(SAVEDIR))\"
    savedir_fp := $(abspath $(strip $(DESTDIR))$(strip $(SAVEDIR)))
    shareddir_fp := $(savedir_fp)/saves
  endif
endif

ifneq ($(strip $(SHAREDDIR)),)
  ifneq ($(filter ~%,$(SHAREDDIR)),)
    CFOTHERS_L += -DSHARED_DIR_PATH=\"$(SHAREDDIR)\"
    shareddir_fp :=
  else
    ifeq ($(filter /%,$(SHAREDDIR)),)
      ifneq ($(prefix),)
	override SHAREDDIR := $(strip $(prefix))/$(strip $(SHAREDDIR))
      endif
    endif
    CFOTHERS_L += -DSHARED_DIR_PATH=\"$(abspath $(SHAREDDIR))\"
    shareddir_fp := $(abspath $(strip $(DESTDIR))$(strip $(SHAREDDIR)))
  endif
endif

ifneq ($(strip $(DATADIR)),)
  ifeq ($(filter /%,$(DATADIR)),)
    #relative DATADIR
    ifneq ($(prefix),)
	override DATADIR := $(strip $(prefix))/$(strip $(DATADIR))
    endif
  endif
  CFOTHERS_L += -DDATA_DIR_PATH=\"$(abspath $(DATADIR))/\"
  FONTDIR = $(abspath $(DATADIR))/dat/tiles/
else
  ifneq ($(prefix),)
	DATADIR := $(strip $(prefix))/$(strip $(DATADIR))
        ifeq ($(LINUXDEPLOY),)
	  FONTDIR = $(strip $(DATADIR))/dat/tiles/
        else
	  FONTDIR = dat/tiles/
        endif
  else
    ifneq ($(DESTDIR)$(ANDROID),)
        FONTDIR = dat/tiles/
    else
        FONTDIR = contrib/fonts/
    endif
  endif
endif
datadir_fp := $(abspath $(strip $(DESTDIR))$(strip $(DATADIR)))

ifneq ($(strip $(WEBDIR)),)
  ifeq ($(filter /%,$(WEBDIR)),)
    #relative WEBDIR
    ifneq ($(prefix),)
	override WEBDIR := $(strip $(prefix))/$(strip $(WEBDIR))
    endif
  endif
  CFOTHERS_L += -DWEB_DIR_PATH=\"$(abspath $(WEBDIR))/\"
else
  ifneq ($(prefix),)
	WEBDIR := $(strip $(prefix))/$(strip $(WEBDIR))
  endif
endif
webdir_fp := $(abspath $(strip $(DESTDIR))$(strip $(WEBDIR)))


# Fonts.
ifdef TILES

OUR_PROPORTIONAL_FONT=DejaVuSans.ttf
OUR_MONOSPACED_FONT=DejaVuSansMono.ttf

ifdef PROPORTIONAL_FONT
  DEFINES += -DPROPORTIONAL_FONT=\"$(PROPORTIONAL_FONT)\"
  ifneq (,$(COPY_FONTS))
    INSTALL_FONTS += "$(PROPORTIONAL_FONT)"
  endif
else
  SYS_PROPORTIONAL_FONT = $(shell util/find_font "$(OUR_PROPORTIONAL_FONT)")
  ifneq (,$(SYS_PROPORTIONAL_FONT))
    ifeq (,$(COPY_FONTS))
      DEFINES += -DPROPORTIONAL_FONT=\"$(SYS_PROPORTIONAL_FONT)\"
    else
      DEFINES += -DPROPORTIONAL_FONT=\"$(FONTDIR)$(OUR_PROPORTIONAL_FONT)\"
      INSTALL_FONTS += "$(SYS_PROPORTIONAL_FONT)"
    endif
  else
    DEFINES += -DPROPORTIONAL_FONT=\"$(FONTDIR)$(OUR_PROPORTIONAL_FONT)\"
    INSTALL_FONTS += "contrib/fonts/$(OUR_PROPORTIONAL_FONT)"
  endif
endif

ifdef MONOSPACED_FONT
  DEFINES += -DMONOSPACED_FONT=\"$(MONOSPACED_FONT)\"
  ifneq (,$(COPY_FONTS))
    INSTALL_FONTS += "$(MONOSPACED_FONT)"
  endif
else
  SYS_MONOSPACED_FONT = $(shell util/find_font "$(OUR_MONOSPACED_FONT)")
  ifneq (,$(SYS_MONOSPACED_FONT))
    ifeq (,$(COPY_FONTS))
      DEFINES += -DMONOSPACED_FONT=\"$(SYS_MONOSPACED_FONT)\"
    else
      DEFINES += -DMONOSPACED_FONT=\"$(FONTDIR)$(OUR_MONOSPACED_FONT)\"
      INSTALL_FONTS += "$(SYS_MONOSPACED_FONT)"
    endif
  else
    DEFINES += -DMONOSPACED_FONT=\"$(FONTDIR)$(OUR_MONOSPACED_FONT)\"
    INSTALL_FONTS += "contrib/fonts/$(OUR_MONOSPACED_FONT)"
  endif
endif

endif


ifndef NO_NCURSES

ifndef CROSSHOST
	NC_PREFIX = /usr
else
	NC_PREFIX = /usr/$(ARCH)
endif

# Include path for (n)curses with Unicode support.

# Your ncurses library may include Unicode support, and you may not have a
# separate libncursesw; this is the case on Mac OS/Darwin.
ifdef LIBNCURSES_IS_UNICODE
  NCURSESLIB = ncurses
else
  NCURSESLIB = ncursesw
endif

NC_LIBS := -L$(NC_PREFIX)/lib -l$(NCURSESLIB)
NC_CFLAGS := -isystem $(NC_PREFIX)/include/$(NCURSESLIB)

ifndef NO_PKGCONFIG
  NC_LIBS := $(shell $(PKGCONFIG) --libs $(NCURSESLIB) 2>/dev/null || echo "$(NC_LIBS)")
  NC_CFLAGS := $(subst -I,-isystem ,$(shell $(PKGCONFIG) --cflags $(NCURSESLIB) 2>/dev/null || echo "$(NC_CFLAGS)"))
endif

ifndef TILES
CFOTHERS_L += $(NC_CFLAGS)
LIBS += $(NC_LIBS)
endif

endif

ifdef BUILD_PCRE
DEFINES += -DREGEX_PCRE
else
  ifdef USE_PCRE
  DEFINES += -DREGEX_PCRE
  LIBS += -lpcre
  endif
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

LINKER_LDFLAG=
ifndef NO_TRY_LLD
ifeq (,$(shell echo 'int main(){return 1;}'|$(GXX) -x c++ - -o /dev/null -fuse-ld=lld 2>&1))
 LINKER_LDFLAG = -fuse-ld=lld
endif
endif
ifeq ($(LINKER_LDFLAG),)
ifndef NO_TRY_GOLD
ifeq (,$(shell echo 'int main(){return 1;}'|$(GXX) -x c++ - -o /dev/null -fuse-ld=gold 2>&1))
  LINKER_LDFLAG = -fuse-ld=gold
endif
endif
endif
ifneq ($(LINKER_LDFLAG),)
LDFLAGS += $(LINKER_LDFLAG)
endif

LDFLAGS += $(CFOPTIMIZE) $(CFOPTIMIZE_L) $(EXTERNAL_LDFLAGS)

ifdef REPORT
CFOTHERS += -ftime-report
endif

UTIL = util/

EXTRA_OBJECTS += $(YACC_OBJECTS)

LEX := $(shell which flex 2> /dev/null)
YACC := $(shell which bison 2> /dev/null)

ifeq ($(strip $(LEX)),)
NO_YACC = YesPlease
endif
ifeq ($(strip $(YACC)),)
NO_YACC = YesPlease
endif

ifneq ($(findstring s,$(MAKEFLAGS)),s)
ifndef V
        QUIET_CC          = @echo '   ' CC $@;
        QUIET_CXX         = @echo '   ' CXX $@;
        QUIET_PCH         = @echo '   ' PCH $@;
        QUIET_LINK        = @echo '   ' LINK $@;
        QUIET_GEN         = @echo '   ' GEN $@;
        QUIET_COPY        = @echo '   ' COPY $@;
        QUIET_DEPEND      = @echo '   ' DEPEND $@;
        QUIET_WINDRES     = @echo '   ' WINDRES $@;
        QUIET_HOSTCC      = @echo '   ' HOSTCC $@;
        QUIET_PNGCRUSH    = @echo '   ' $(PNGCRUSH_LABEL) $@;
        QUIET_ADVPNG      = @echo '   ' ADVPNG $@;
        QUIET_PYTHON      = @echo '   ' PYTHON $@;
        export V
endif
endif

TILEIMAGEFILES := floor wall feat main player gui icons
TILEDEFS = $(TILEIMAGEFILES) dngn unrand
TILEDEFPRES = $(TILEDEFS:%=$(RLTILES)/tiledef-%)
TILEDEFOBJS = $(TILEDEFPRES:%=%.o)
TILEDEFSRCS = $(TILEDEFPRES:%=%.cc)
TILEDEFHDRS = $(TILEDEFPRES:%=%.h)

TILEFILES = $(TILEIMAGEFILES:%=%.png)
ORIGTILEFILES = $(TILEFILES:%=$(RLTILES)/%)
DESTTILEFILES = $(TILEFILES:%=dat/tiles/%)

OBJECTS += $(TILEDEFOBJS)

ifdef TILES_ANY
ifndef NO_OPTIMIZE
  ifneq (,$(shell which advpng))
    USE_ADVPNG = y
  else
    ifneq (,$(shell which pngcrush))
      PNGCRUSH = pngcrush -q -m 113
      PNGCRUSH_LABEL = PNGCRUSH
    endif
  endif
endif
endif

ifdef BUILD_PCRE
CONTRIBS += pcre
CONTRIB_LIBS += $(LIBPCRE)
endif
ifdef BUILD_FREETYPE
CONTRIBS += freetype
CONTRIB_LIBS += $(LIBFREETYPE)
endif
ifdef BUILD_LIBPNG
CONTRIBS += libpng
CONTRIB_LIBS := $(LIBPNG) $(CONTRIB_LIBS)
endif
ifdef BUILD_SDL2
CONTRIBS += sdl2
CONTRIB_LIBS := $(LIBSDL2) $(CONTRIB_LIBS)
ifeq ($(uname_S),Linux)
LIBS += -ldl -lrt
endif
endif
ifdef BUILD_SDL2IMAGE
CONTRIBS += sdl2-image
CONTRIB_LIBS := $(LIBSDL2IMAGE) $(CONTRIB_LIBS)
endif
ifdef BUILD_SDL2MIXER
CONTRIBS += sdl2-mixer
CONTRIB_LIBS := $(LIBSDL2MIXER) $(CONTRIB_LIBS)
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

ifdef CONTRIBS
INCLUDES_L += -isystem contrib/install/$(ARCH)/include
LIBS += -Lcontrib/install/$(ARCH)/lib
endif

ifdef APPLE_PLATFORM
# workaround: mac clang hardcodes (as far as I can tell) `-I /usr/local/include`
# into the include path. Homebrew also installs into this directory. So a user
# that has homebrew installed will get includes from there rather than contribs,
# leading to build failures. This workaround relies on that if both -I and
# -isystem are defined for the same path, then the -I version is ignored, which
# effectively demotes this include path below the contrib -isystem paths.
# TODO: it's not clear to me that using -isystem for contribs is at all the
# right thing to do, since they are libraries we control, aren't actually
# system libraries, and always take lower precedence than any -I that might be
# caused by environment variables.
INCLUDES_L += -isystem /usr/local/include
endif

LIBS += $(CONTRIB_LIBS) $(EXTRA_LIBS)


ifdef ANDROID
CFLAGS   := $(CFOPTIMIZE) $(CFOTHERS) $(CFWARN) $(CFLAGS)
else
CFLAGS   := $(CFOPTIMIZE) $(CFOTHERS) $(CFWARN)
endif
CFLAGS_L := $(CFOPTIMIZE_L) $(DEFINES_L) $(CFWARN_L) $(INCLUDES_L) $(CFOTHERS_L)
ALL_CFLAGS := $(CFLAGS) $(CFLAGS_L)
YACC_CFLAGS  := $(ALL_CFLAGS) -Wno-unused-function -Wno-sign-compare -DYYENABLE_NLS=0 -DYYLTYPE_IS_TRIVIAL=0 -Wno-null-conversion
RLTILES_CFLAGS := $(ALL_CFLAGS) -Wno-tautological-compare


DOC_BASE        := ../docs
DOC_TEMPLATES   := $(DOC_BASE)/template
GENERATED_DOCS  := $(DOC_BASE)/aptitudes.txt $(DOC_BASE)/aptitudes-wide.txt $(DOC_BASE)/FAQ.html $(DOC_BASE)/crawl_manual.txt $(DOC_BASE)/quickstart.txt
# Headers that need to exist before attempting to compile cc files
GENERATED_HEADERS := art-enum.h config.h mon-mst.h species-type.h job-type.h
# All other generated files will be created later
GENERATED_FILES := $(GENERATED_HEADERS) art-data.h mi-enum.h \
                   $(RLTILES)/dc-unrand.txt build.h compflag.h dat/dlua/tags.lua \
                   cmd-name.h species-data.h aptitudes.h species-groups.h mon-data.h job-data.h job-groups.h

LANGUAGES = $(filter-out en, $(notdir $(wildcard dat/descript/??)))
SRC_PKG_BASE  := stone_soup
SRC_VERSION   := $(shell git describe $(MERGE_BASE) 2>/dev/null || cat util/release_ver)
MAJOR_VERSION = $(shell echo "$(SRC_VERSION)"|$(SED) -r 's/-.*//;s/^([^.]+\.[^.]+).*/\1/')
RECENT_TAG    := $(shell git describe --abbrev=0 $(MERGE_BASE))
WINARCH := $(shell $(GXX) -dumpmachine | grep -q x64_64 && echo win64 || echo win32)

export SRC_VERSION
export WINARCH

# Update .ver file if SRC_VERSION has changed
.ver: FORCE
	@echo '$(SRC_VERSION)' | cmp -s - $@ || echo '$(SRC_VERSION)' > $@

PKG_SRC_DIR   := $(SRC_PKG_BASE)-$(SRC_VERSION)
SRC_PKG_TAR   := $(PKG_SRC_DIR).tar.xz
SRC_PKG_TAR_NODEPS := $(PKG_SRC_DIR)-nodeps.tar.xz
SRC_PKG_ZIP   := $(PKG_SRC_DIR).zip

greet:
	@if [ ! -e $(GAME) ]; then\
		printf '  * %s\n' "If you experience any problems building Crawl, please take a second look"\
		                  "at INSTALL.md: the solution to your problem just might be in there!";\
	fi

docs: $(GENERATED_DOCS)


# Webtiles data
.PHONY: webserver clean-webserver

webserver/static/%.png: dat/tiles/%.png
	$(QUIET_COPY)$(COPY) $< webserver/static/

webserver/game_data/static/%.png: dat/tiles/%.png
	$(QUIET_COPY)$(COPY) $< webserver/game_data/static/

TILEINFOJS = $(TILEIMAGEFILES) dngn
TITLEIMGS = denzi_dragon denzi_kitchen_duty denzi_summoner \
            denzi_undead_warrior omndra_zot_demon firemage \
            shadyamish_octm denzi_evil_mage denzi_invasion \
            psiweapon_kiku white_noise_entering_the_dungeon \
            white_noise_grabbing_the_orb pooryurik_knight \
            psiweapon_roxanne baconkid_gastronok baconkid_mnoleg \
            peileppe_bloax_eye baconkid_duvessa_dowan ploomutoo_ijyb \
            froggy_thunder_fist_nikola froggy_rune_and_run_failed_on_dis \
            froggy_natasha_and_boris froggy_jiyva_felid \
            froggy_goodgod_tengu_gold Cws_Minotauros nibiki_octopode \
            e_m_fields sastrei_chei anon_octopus_wizard arbituhhh_tesu \
            kaonedong_ignis_the_dying_flame kaonedong_menkaure_prince_of_dust \
            philosopheropposite_palentonga_paladin gompami_kohu_xbow \
            micah_c_ereshkigal SpinningBird_djinn_sears_gnolls \
			benadryl_antaeus king7artist_eustachio

STATICFILES = $(TILEIMAGEFILES:%=webserver/game_data/static/%.png) \
              webserver/static/stone_soup_icon-32x32.png \
              $(TITLEIMGS:%=webserver/static/title_%.png) \
              $(TILEINFOJS:%=webserver/game_data/static/tileinfo-%.js) \
              webserver/webtiles/version.txt

$(TILEINFOJS:%=$(RLTILES)/tileinfo-%.js): build-rltiles

webserver/webtiles/version.txt: .ver
	@git describe $(MERGE_BASE) > webserver/webtiles/version.txt

webserver/game_data/static/%.js: $(RLTILES)/%.js
	$(QUIET_COPY)$(COPY) $< webserver/game_data/static/

clean-webserver:
	$(RM) $(STATICFILES) webserver/*.pyc

ifdef WEBTILES
webserver: $(STATICFILES)
else
webserver:
endif

GAME_OBJS=$(OBJECTS) main.o $(EXTRA_OBJECTS)
MONSTER_OBJS=$(OBJECTS) util/monster/monster-main.o $(EXTRA_OBJECTS)
CATCH2_TEST_OBJECTS = $(OBJECTS) $(TEST_OBJECTS) catch2-tests/catch_amalgamated.o catch2-tests/test_main.o $(EXTRA_OBJECTS)


ifneq (,$(filter plug-and-play-tests,$(MAKECMDGOALS)))
    ifeq ("$(wildcard catch2-tests/test_plug_and_play.cc)","")
$(info  ----------------------------------------------------------------)
$(info  ERROR: 'catch2-tests_files/test_plug_and_play.cc' is not present)
$(info  you must create a catch2-tests_files/test_plug_and_play.cc file)
$(info  in order to use the 'make plug-and-play-tests' option. See )
$(info  'docs/develop/test_plug_and_play_cc.txt' for more details.)
$(info  ----------------------------------------------------------------)
$(error See above ^ ^ ^ ^ ^ ^ ^)

    endif
endif

# There might not have been any goals passed on the commandline, so...
GOALS = $(or $(MAKECMDGOALS),$(.DEFAULT_GOAL))
# Unless these are the only goals...
ifneq ($(GOALS),$(filter clean test %-windows,$(GOALS)))

#
# CFLAGS difference check
#
# Check for flag changes between the previous build and the current one,
# because any CFLAGS change could result in an inconsistent build if the
# person building it isn't careful.
#
# This should eliminate an annoying need to use 'make clean' every time.
#

TRACK_CFLAGS = $(strip $(subst ','\'',$(CC) $(CXX) $(ALL_CFLAGS)))
        # (stray ' for highlights)
OLD_CFLAGS = $(strip $(subst ','\'',$(shell cat .cflags 2>/dev/null)))
        # (stray ' for highlights)

ifneq ($(TRACK_CFLAGS),$(OLD_CFLAGS))
  NEED_REBUILD = y
endif

.cflags: .force-cflags
ifdef NEED_REBUILD
	@echo "    * rebuilding crawl: new build flags or prefix"
	@echo 'TRACK_CFLAGS = $(TRACK_CFLAGS) #EOL'
	@echo 'OLD_CFLAGS   = $(OLD_CFLAGS) #EOL'
	@echo '$(TRACK_CFLAGS)' > .cflags
endif

.android-cflags: .cflags
	@echo $(STDFLAG) $(CFLAGS_L) | sed 's@"@\\"@g' >.android-cxxflags
	@echo $(CFLAGS_L) | sed 's@"@\\"@g' >.android-cflags

.PHONY: .force-cflags

endif

##########################################################################
# Dependencies

DEPS := $(ALL_OBJECTS:.o=.d)

# Any stale .d files are worse than useless, as they can break builds by
# telling make a header or file no longer present is needed.  This is very
# likely to happen if the compilation mode changes (normal vs debug, tiles
# vs console vs webtiles) or if the compiler itself changes (gcc vs
# gcc-snapshot vs clang, or x86_64-linux-gnu-gcc vs x86_64-w64-mingw32-g++).
#
# In such cases, the dependency forces a rebuild of that file. If the
# dependency file referred to a non-existent header, as determined by
# util/good-depfile, then replace it with a rule that forces a rebuild.
# That rebuild will overwrite the stopgap .d file with a good one for
# the next build.
ifndef NEED_REBUILD
  VALID_DEPS := $(shell checkdep () { \
                          x_d="$$1"; \
                          x="$${x_d%%.d}"; x_cc="$${x}.cc"; x_o="$${x}.o"; \
                          if [ "$$x_cc" -ot "$$x_d" \
                               -a "$$x_d" -nt .cflags ]; \
                          then \
                            util/good-depfile "$$x_d" \
                              || echo "$$x_o: FORCE" > "$$x_d"; \
                            echo "$$x_d"; \
                          fi; \
                        }; \
                        for x_d in $(DEPS); do \
                          checkdep "$$x_d" & \
                        done)
endif
-include $(VALID_DEPS)


# This information is included in crash reports, and is printed with
# "crawl -version"
compflag.h: $(OBJECTS:.o=.cc) main.cc util/gen-cflg.pl .cflags
	$(QUIET_GEN)util/gen-cflg.pl compflag.h "$(ALL_CFLAGS)" "$(LDFLAGS)" "$(HOST)" "$(ARCH)"

build.h: $(OBJECTS:.o=.cc) main.cc util/gen_ver.pl .ver
	$(QUIET_GEN)util/gen_ver.pl $@ $(MERGE_BASE)

version.o: build.h compflag.h

##########################################################################
# Documentation
#
$(DOC_BASE)/aptitudes.txt: $(DOC_TEMPLATES)/apt-tmpl.txt species-data.h aptitudes.h \
						   util/gen-apt.pl
	$(QUIET_GEN)./util/gen-apt.pl $@ $^

$(DOC_BASE)/aptitudes-wide.txt: $(DOC_TEMPLATES)/apt-tmpl-wide.txt species-data.h \
						   aptitudes.h util/gen-apt.pl
	$(QUIET_GEN)./util/gen-apt.pl $@ $^

$(DOC_BASE)/FAQ.html: dat/database/FAQ.txt util/FAQ2html.pl
	$(QUIET_GEN)./util/FAQ2html.pl $< $@

# Generate a .txt version because some servers need it
# TODO: actually render this
$(DOC_BASE)/quickstart.txt: $(DOC_BASE)/quickstart.md
	$(QUIET_COPY)$(COPY) $< $@

$(DOC_BASE)/crawl_manual.txt: $(DOC_BASE)/crawl_manual.rst util/unrest.pl
	$(QUIET_GEN)util/unrest.pl <$< >$@

api:
	mkdir -p $(DOC_BASE)/develop/lua/
	# need abspath here because ldoc is fragile
	ldoc --config util/config.ld --dir $(abspath $(DOC_BASE)/lua/) .

api-dev:
	mkdir -p $(DOC_BASE)/develop/lua/
	# need abspath here because ldoc is fragile
	ldoc --config util/config.ld --dir $(abspath $(DOC_BASE)/develop/lua/) \
	     --all .

##########################################################################
# The level compiler
#

$(YACC_OBJECTS): $(CONTRIB_LIBS)

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
ifdef ANDROID
  # during build, data goes in a different directory to when crawl runs!
  prefix_fp = ..
  datadir_fp = android-project/app/src/main/assets
  savedir_fp =
  shareddir_fp =
endif

install-data: $(GENERATED_FILES)
ifeq ($(DESTDIR)$(prefix)$(ANDROID),)
	@echo Neither "DESTDIR" nor "prefix" defined -- nowhere to install to, aborting.
	@exit 1
endif
	[ -d $(datadir_fp) ] || mkdir -p $(datadir_fp)
	mkdir -p $(datadir_fp)/dat/des
	mkdir -p $(datadir_fp)/dat/dlua
	mkdir -p $(datadir_fp)/dat/clua
	mkdir -p $(datadir_fp)/dat/database
	mkdir -p $(datadir_fp)/dat/defaults
	mkdir -p $(datadir_fp)/dat/descript
	for LANG in $(LANGUAGES); \
		do mkdir -p $(datadir_fp)/dat/descript/$$LANG; \
	done
	mkdir -p $(datadir_fp)/docs/develop
	mkdir -p $(datadir_fp)/docs/develop/levels
	mkdir -p $(datadir_fp)/docs/license
	mkdir -p $(datadir_fp)/settings
	$(COPY_R) dat/des/* $(datadir_fp)/dat/des/
	$(COPY_R) dat/dlua/* $(datadir_fp)/dat/dlua/
	echo "-- Autogenerated list of maps to load and compile:" \
	    >$(datadir_fp)/dat/dlua/loadmaps.lua
	find dat -name '*.des'|LC_ALL=C sort|sed s:dat/::| \
	while read x; \
	    do echo "dgn.load_des_file('$$x')"; \
	    done >>$(datadir_fp)/dat/dlua/loadmaps.lua
	$(COPY)   dat/clua/*.lua $(datadir_fp)/dat/clua/
	$(COPY_R) dat/database/* $(datadir_fp)/dat/database/
	$(COPY_R) dat/defaults/* $(datadir_fp)/dat/defaults/
	$(COPY) dat/descript/*.txt $(datadir_fp)/dat/descript/
	for LANG in $(LANGUAGES); \
		do $(COPY) dat/descript/$$LANG/*.txt $(datadir_fp)/dat/descript/$$LANG; \
	done
	mkdir -p $(datadir_fp)/dat/dist_bones
	$(COPY) dat/dist_bones/* $(datadir_fp)/dat/dist_bones/
	$(COPY) ../docs/*.txt $(datadir_fp)/docs/
	$(COPY) ../docs/*.md $(datadir_fp)/docs/
	$(COPY) ../docs/develop/*.txt $(datadir_fp)/docs/develop/
	$(COPY) ../docs/develop/levels/*.txt $(datadir_fp)/docs/develop/levels/
	$(COPY) ../docs/license/*.txt $(datadir_fp)/docs/license/
	$(COPY) ../CREDITS.txt $(datadir_fp)/docs/
	$(COPY_R) ../settings/* $(datadir_fp)/settings/
ifeq ($(GAME),crawl.exe)
	$(SED) -i 's/$$/\r/' `find $(datadir_fp) -iname '*.txt' -or -iname '*.des'`
endif
ifdef TILES
	mkdir -p $(datadir_fp)/dat/tiles
	$(COPY) dat/tiles/*.png $(datadir_fp)/dat/tiles/
ifneq (,$(INSTALL_FONTS))
	$(COPY) $(INSTALL_FONTS) $(datadir_fp)/dat/tiles/
endif
endif
ifdef WEBTILES
	mkdir -p $(webdir_fp)
	$(COPY_R) webserver/game_data/* $(webdir_fp)/
endif
ifneq ($(savedir_fp),)
	mkdir -p $(savedir_fp)/saves
	mkdir -p $(savedir_fp)/morgue
ifeq ($(USE_DGAMELAUNCH),)
	$(CHOWN) -R $(INSTALL_UGRP) $(datadir_fp) || true
	$(CHOWN) -R $(INSTALL_UGRP) $(savedir_fp) || true
	$(CHMOD) $(MCHMOD_SAVEDIR) $(savedir_fp) || true
	$(CHMOD) $(MCHMOD_SAVEDIR) $(savedir_fp)/saves || true
	$(CHMOD) $(MCHMOD_SAVEDIR) $(savedir_fp)/morgue || true
endif
endif
ifneq ($(shareddir_fp),)
	mkdir -p $(shareddir_fp)
ifneq ($(patsubst /var/%,%,$(shareddir_fp)),$(shareddir_fp))
# Only if we're being installed for real.  Installations to a staging dir
# which are then packaged would trample existing files; these need to be
# handled by packagers themselves.
	touch $(shareddir_fp)/logfile
	touch $(shareddir_fp)/scores
endif
ifeq ($(USE_DGAMELAUNCH),)
	$(CHOWN) -R $(INSTALL_UGRP) $(shareddir_fp) || true
	$(CHMOD) $(MCHMOD_SAVEDIR) $(shareddir_fp) || true
	$(CHMOD) $(MCHMOD_LOGS) $(shareddir_fp)/logfile || true
	$(CHMOD) $(MCHMOD_LOGS) $(shareddir_fp)/scores || true
endif
endif

install-xdg-data:
	[ -d $(prefix_fp)/share/applications ] || mkdir -p $(prefix_fp)/share/applications
	[ -d $(prefix_fp)/share/metainfo ] || mkdir -p $(prefix_fp)/share/metainfo
	[ -d $(prefix_fp)/share/icons/hicolor/32x32/apps ] || mkdir -p $(prefix_fp)/share/icons/hicolor/32x32/apps
	[ -d $(prefix_fp)/share/icons/hicolor/48x48/apps ] || mkdir -p $(prefix_fp)/share/icons/hicolor/48x48/apps
	[ -d $(prefix_fp)/share/icons/hicolor/512x512/apps ] || mkdir -p $(prefix_fp)/share/icons/hicolor/512x512/apps
	[ -d $(prefix_fp)/share/icons/hicolor/scalable/apps ] || mkdir -p $(prefix_fp)/share/icons/hicolor/scalable/apps
	$(COPY) xdg-data/$(XDG_ORIG).desktop $(prefix_fp)/share/applications/$(XDG_NAME).desktop
	echo Icon=$(XDG_NAME) >>$(prefix_fp)/share/applications/$(XDG_NAME).desktop
ifdef LINUXDEPLOY
	echo Exec=$(GAME) >>$(prefix_fp)/share/applications/$(XDG_NAME).desktop
else
	echo Exec=$(prefix)/$(bin_prefix)/$(GAME) >>$(prefix_fp)/share/applications/$(XDG_NAME).desktop
endif
	sed "s/@XDG_NAME@/$(XDG_NAME)/" <xdg-data/$(XDG_ORIG).appdata.xml >$(prefix_fp)/share/metainfo/$(XDG_NAME).appdata.xml
	$(COPY) dat/tiles/stone_soup_icon-32x32.png $(prefix_fp)/share/icons/hicolor/32x32/apps/$(XDG_NAME).png
	$(COPY) dat/tiles/stone_soup_icon-48x48.png $(prefix_fp)/share/icons/hicolor/48x48/apps/$(XDG_NAME).png
	$(COPY) dat/tiles/stone_soup_icon-512x512.png $(prefix_fp)/share/icons/hicolor/512x512/apps/$(XDG_NAME).png
	$(COPY) dat/tiles/stone_soup_icon.svg $(prefix_fp)/share/icons/hicolor/scalable/apps/$(XDG_NAME).svg

install: all install-data
	[ -d $(prefix_fp)/$(bin_prefix) ] || mkdir -p $(prefix_fp)/$(bin_prefix)
	$(COPY) $(GAME) $(prefix_fp)/$(bin_prefix)/
	$(STRIP) $(prefix_fp)/$(bin_prefix)/$(GAME)
ifeq ($(USE_DGAMELAUNCH),)
	$(CHOWN) $(INSTALL_UGRP) $(prefix_fp)/$(bin_prefix)/$(GAME) || true
	$(CHMOD) $(MCHMOD) $(prefix_fp)/$(bin_prefix)/$(GAME) || true
endif

clean: clean-rltiles clean-webserver clean-android clean-monster clean-catch2 \
       clean-plug-and-play-tests clean-coverage-full clean-mac clean-appimage
	+$(MAKE) -C $(UTIL) clean
	$(RM) $(GAME) $(GAME).exe $(GENERATED_FILES) $(EXTRA_OBJECTS) libw32c.o\
	    libunix.o $(ALL_OBJECTS) $(ALL_OBJECTS:.o=.d) *.ixx  \
	    .contrib-libs .cflags AppHdr.h.gch AppHdr.h.d util/fake_pty \
            rltiles/tiledef-unrand.cc
	$(RM) -r build-win
	$(RM) -r build

clean-contrib:
	+$(MAKE) -C contrib clean

clean-android:
	$(RM) -r android-project/app/src/main/assets
	$(RM) android-project/app/src/main/res/drawable/*.png
	$(RM) android-project/local.properties
	$(RM) android-project/app/build.gradle

distclean: clean clean-contrib clean-rltiles
	$(RM) -r morgue saves
	$(RM) scores $(GAME) core $(DEPENDENCY_MKF)
	$(RM) -r mac-app-zips
	$(RM) -r $(DOXYGEN_HTML_GEN)

$(GAME): $(GAME_OBJS) $(CONTRIB_LIBS) dat/dlua/tags.lua
	+$(QUIET_LINK)$(CXX) $(LDFLAGS) $(GAME_OBJS) -o $(GAME) $(LIBS)

util/monster/vault_monster_data.h: dat/des util/gather_mons
	util/gather_mons -v > $@

util/monster/monster-main.o: util/monster/vault_monster_data.h

util/monster/monster: $(MONSTER_OBJS) $(CONTRIB_LIBS) dat/dlua/tags.lua
	+$(QUIET_LINK)$(CXX) $(LDFLAGS) $(MONSTER_OBJS) -o $@ $(LIBS)

monster: util/monster/monster

test-monster: monster
	util/monster/monster quasit

clean-monster:
	$(RM) util/monster/monster util/monster/vault_monster_data.h tile_info.txt

install-monster: monster
	util/gather_mons -t > tile_info.txt
	strip -s util/monster/monster
	cp util/monster/monster $(HOME)/bin/
	if [ -f ~/source/announcements.log ]; then \
	  echo 'Monster database of master branch on crawl.develz.org updated to: $(VERSION)' >>~/source/announcements.log;\
	fi

catch2-tests-executable: $(CATCH2_TEST_OBJECTS) $(CONTRIB_LIBS) dat/dlua/tags.lua
	+$(QUIET_LINK)$(CXX) $(LDFLAGS) $(CATCH2_TEST_OBJECTS) -o catch2-tests-executable $(LIBS)

CATCH2_PNP_OBJECTS = $(OBJECTS) catch2-tests/test_plug_and_play.o \
                     catch2-tests/test_main.o $(EXTRA_OBJECTS)

plug-and-play-tests: $(CATCH2_PNP_OBJECTS) $(CONTRIB_LIBS) dat/dlua/tags.lua
	+$(QUIET_LINK)$(CXX) $(LDFLAGS) $(CATCH2_PNP_OBJECTS) -o $@ $(LIBS)

catch2-tests: catch2-tests-executable
	./catch2-tests-executable

clean-coverage-full: clean-coverage
	find . -type f -name '*.gcno' -delete

clean-coverage:
	$(RM) default.profraw default.profdata coverage.info coverage.profdata
	find . -type f \( -name '*.gcda' -or -name '*.gcov' \) -delete
	$(RM) -r coverage-html-report

clean-catch2:
	$(RM) catch2-tests-executable catch2-tests-executable.exe

clean-plug-and-play-tests:
	$(RM) plug-and-play-tests plug-and-play-tests.exe

debug: all
debug-lite: all
profile: all

doxy-simple: doxygen-simple
doxygen-simple:
	$(DOXYGEN) $(DOXYGEN_SIMPLE_CONF)

doxy-all: doxygen-all
doxygen-all:
	$(DOXYGEN) $(DOXYGEN_ALL_CONF)

# Dependency generation
#
ifndef NO_INLINE_DEPGEN
# See info node: (gcc) Preprocessor Options
INLINE_DEPGEN_CFLAGS = -MMD
endif

$(DEPS): %.d: %.cc

# Precompiled headers
#
ifdef PCH
-include AppHdr.h.d
%.gch: % .cflags $(CONTRIB_LIBS)
# This needs -MD, not -MMD, lest we be haunted by the ghosts of stale
# system headers.
	$(QUIET_PCH)$(CXX) $(STDFLAG) $(ALL_CFLAGS) -MD -c $< -o $@

CC_DEP := AppHdr.h.gch
endif

#
# Header build rule: order before regular .o rule for backwards compatibility
# with gnu make <3.82.
#

%.h.o: %.h | $(GENERATED_HEADERS)
ifdef NO_INLINE_DEPGEN
	$(QUIET_DEPEND)$(or $(DEPCXX),$(CXX)) -MM $(STDFLAG) $(ALL_CFLAGS) -MT $*.o $< > $*.d
endif
	$(QUIET_CXX)$(CXX) $(STDFLAG) $(ALL_CFLAGS) $(INLINE_DEPGEN_CFLAGS) -x c++-header -c $< -o $@
ifndef NO_INLINE_DEPGEN
	@if [ -f $(@:%.o=%.d) ]; then touch -r $@ $(@:%.o=%.d); fi
endif


# Plain old compilation
#

$(OBJECTS:%.o=%.cc) main.cc: $(CONTRIB_LIBS)

$(UTIL)%.o: ALL_CFLAGS=$(YACC_CFLAGS)
$(TILEDEFOBJS): ALL_CFLAGS=$(RLTILES_CFLAGS)

# The "|" designates "order-only" dependencies. See: (make) Prerequisite Types.
#
# This ensures that we update these headers before building anything
# that might depend on them without our knowing it yet, but lets the
# .d files take care of *whether* or not to rebuild.
#
# This is kind of important when building without ccache.

%.o: %.m .cflags | $(GENERATED_HEADERS)
	$(QUIET_CC)$(CC) $(ALL_CFLAGS) $(INLINE_DEPGEN_CFLAGS) -c $< -o $@

%.o: %.cc %.d .cflags $(CC_DEP) | $(GENERATED_HEADERS) $(TILEDEFHDRS)
ifdef NO_INLINE_DEPGEN
	$(QUIET_DEPEND)$(or $(DEPCXX),$(CXX)) -MM $(STDFLAG) $(ALL_CFLAGS) -MT $*.o $< > $*.d
endif
	$(QUIET_CXX)$(CXX) $(STDFLAG) $(ALL_CFLAGS) $(INLINE_DEPGEN_CFLAGS) -c $< -o $@
ifndef NO_INLINE_DEPGEN
	@if [ -f $(@:%.o=%.d) ]; then touch -r $@ $(@:%.o=%.d); fi
endif

icon.o: util/crawl.rc dat/tiles/stone_soup_icon.ico .cflags
	$(QUIET_WINDRES)$(WINDRES) util/crawl.rc icon.o

config.h: util/configure .cflags
	${QUIET_GEN}util/configure "$(CXX)" $(STDFLAG) $(ALL_CFLAGS)

#
# Header build tests.
#

.PHONY: header-build-tests
header-build-tests: $(HEADER_OBJECTS)

#
# Contribs
#

$(CONTRIB_LIBS): .contrib-libs
	@:

.contrib-libs: .cflags
ifneq (,$(CONTRIBS))
	@echo "    * Need to build contribs: $(CONTRIBS)"
	+@$(MAKE) -C contrib $(CONTRIBS) ARCH=$(ARCH)
endif
	@touch $@

$(foreach t,$(CONTRIB_LIBS),$(if $(wildcard $t),,$(shell rm -f .contrib-libs)))

#############################################################################
# Build unrandart data
art-data.h: art-data.txt util/art-data.pl art-func.h $(RLTILES)/dc-player.txt
	util/art-data.pl
art-enum.h rltiles/dc-unrand.txt rltiles/tiledef-unrand.cc: art-data.h

mon-mst.h: mon-spell.h util/gen-mst.pl mon-data.h
	$(QUIET_GEN)util/gen-mst.pl

cmd-name.h: command-type.h util/cmd-name.pl
	$(QUIET_GEN)util/cmd-name.pl

dat/dlua/tags.lua: tag-version.h util/gen-luatags.pl
	$(QUIET_GEN)util/gen-luatags.pl

mi-enum.h: mon-info.h util/gen-mi-enum
	$(QUIET_GEN)util/gen-mi-enum

mon-data.h: dat/mons/*.yaml util/mon-gen.py util/mon-gen/*.txt
	$(QUIET_PYTHON)$(PYTHON) util/mon-gen.py dat/mons/ util/mon-gen/ mon-data.h

# species-gen.py creates multiple files at once. Ensure Make doesn't run it once
# per target file by adding all the files into a single dependency chain.
# Ref: https://stackoverflow.com/q/2973445
species-data.h: dat/species/*.yaml util/species-gen.py util/species-gen/*.txt
	$(QUIET_PYTHON)$(PYTHON) util/species-gen.py dat/species/ util/species-gen/ species-data.h aptitudes.h species-groups.h species-type.h
# no-op
aptitudes.h: species-data.h
	$(QUIET_GEN)
# no-op
species-groups.h: aptitudes.h
	$(QUIET_GEN)
# no-op
species-type.h: species-groups.h
	$(QUIET_GEN)

job-data.h: dat/jobs/*.yaml util/job-gen.py util/job-gen/*.txt
	$(QUIET_PYTHON)$(PYTHON) util/job-gen.py dat/jobs/ util/job-gen/ job-data.h job-groups.h job-type.h

# no-op
job-groups.h: job-data.h
	$(QUIET_GEN)

# no-op
job-type.h: job-groups.h
	$(QUIET_GEN)

$(RLTILES)/dc-unrand.txt: art-data.h

artefact.o: art-data.h art-enum.h
mon-util.o: mon-mst.h
mon-util.d: mon-mst.h
l-moninf.o: mi-enum.h
macro.o: cmd-name.h

#############################################################################
# RLTiles
#

#.PHONY: build-rltiles
build-rltiles: .contrib-libs $(RLTILES)/dc-unrand.txt
ifdef ANDROID
        # force submake to run properly
	+$(MAKE) -C $(RLTILES) all ANDROID=$(ANDROID) TILES=1 V=1
        # prove that tiles were generated properly
	grep tile_info rltiles/*.cc| head
else
# ensure we don't use contrib libpng when cross-compiling - it won't work.
ifdef CROSSHOST
	+$(MAKE) -C $(RLTILES) all ARCH=$(ARCH) TILES=$(TILES)$(WEBTILES)
else
	+$(MAKE) -C $(RLTILES) all ARCH=$(ARCH) NO_PKGCONFIG=$(NO_PKGCONFIG) TILES=$(TILES)$(WEBTILES)
endif
endif

$(TILEDEFSRCS) $(TILEDEFHDRS) $(ORIGTILEFILES): build-rltiles

dat/tiles/%.png: $(RLTILES)/%.png
	$(QUIET_PNGCRUSH)$(PNGCRUSH) $< $@
ifdef USE_ADVPNG
	$(QUIET_ADVPNG)$(ADVPNG) $@
endif

clean-rltiles:
	$(RM) $(DESTTILEFILES)
	+$(MAKE) -C $(RLTILES) distclean

ifdef TILES_ANY
install-data: $(DESTTILEFILES)
$(GAME): $(DESTTILEFILES)
endif

.PHONY: check-fonts
check-fonts:
ifneq (,$(INSTALL_FONTS))
	@for x in $(INSTALL_FONTS); do if [ ! -e "$$x" ]; then echo \
	"Font file $${x##contrib/fonts/} not found. Please install it (possibly via contribs)."; \
	exit 1; fi; done
endif

#############################################################################
# Packaging a source tarball for release
#

# To package, you *must* have lex and yacc to generate the intermediates.
BSRC = build/crawl-ref/source/
source: removeold prebuildyacc
	@git branch >/dev/null 2>/dev/null || (echo You can package source only from git. && false)
	rm -rf build
	mkdir build
	(cd ../..;git ls-files| \
		grep -v -f crawl-ref/source/misc/src-pkg-excludes.lst| \
		tar cf - -T -)|tar xf - -C build
	for x in lua pcre sqlite libpng freetype sdl2 sdl2-image sdl2-mixer zlib fonts; \
	  do \
	   mkdir -p $(BSRC)contrib/$$x; \
	   (cd contrib/$$x;git ls-files|tar cf - -T -)| \
		tar xf - -C $(BSRC)contrib/$$x; \
	  done
	find build -name .gitignore -execdir rm -f '{}' +
	git describe $(MERGE_BASE) > $(BSRC)util/release_ver
	cp build/LICENSE build/crawl-ref/LICENSE
	mv build/crawl-ref build/$(PKG_SRC_DIR)

package-tarball-deps: source
	cd build && tar cfJ ../../../$(SRC_PKG_TAR) $(PKG_SRC_DIR)

package-zipball-deps: source
	@if which zip >/dev/null; then \
	  echo "cd build && zip -rq ../../../$(SRC_PKG_ZIP) $(PKG_SRC_DIR)"; \
	  cd build && zip -rq ../../../$(SRC_PKG_ZIP) $(PKG_SRC_DIR); \
	else \
	  echo "**** No ZIP installed -- not creating the zipball."; \
	fi

package-tarball-nodeps: source
	cd build && tar cfJ ../../../$(SRC_PKG_TAR_NODEPS) --exclude contrib --exclude simplebar.js $(PKG_SRC_DIR)

package-source: package-tarball-deps package-zipball-deps package-tarball-nodeps
	$(RM) -r build-win

removeold:
	if [ -f ../../$(SRC_PKG_TAR) ]; then $(RM) ../../$(SRC_PKG_TAR); fi
	if [ -f ../../$(SRC_PKG_ZIP) ]; then $(RM) ../../$(SRC_PKG_ZIP); fi

#############################################################################
# Linux AppImage
#

ifdef LINUXDEPLOY
endif

appimage: install install-xdg-data
ifeq ($(LINUXDEPLOY),)
	@echo Please define LINUXDEPLOY path to use this build.
	@exit 1
endif
	$(LINUXDEPLOY) --appdir appimage\
	               --desktop-file appimage/usr/share/applications/$(XDG_NAME).desktop \
	               --icon-file appimage/usr/share/icons/hicolor/scalable/apps/$(XDG_NAME).svg \
	               --output appimage
	ln -sf "stone_soup-$(SRC_VERSION)-linux-$(appimage_build).$(uname_M).AppImage" "stone_soup-latest-linux-$(appimage_build).$(uname_M).AppImage"

clean-appimage:
	$(RM) stone_soup-*.AppImage
	$(RM) -r appimage

#############################################################################
# Building the unified Windows package.
#

# You need to have NSIS installed.
package-windows-installer:
ifneq (x$(SRC_VERSION),x$(shell cat build-win/version.txt 2>/dev/null))
	+$(MAKE) build-windows
endif
	makensis -NOCD -DVERSION=$(SRC_VERSION) -DWINARCH="$(WINARCH)" util/crawl.nsi
	ln -sf "stone_soup-$(SRC_VERSION)-$(WINARCH)-installer.exe" "stone_soup-latest-$(WINARCH)-installer.exe"
	$(RM) -r build-win

build-windows:
ifneq ($(GAME),crawl.exe)
	@echo "This is only for Windows; please specify CROSSHOST.";false
endif
	+$(MAKE) clean
	+$(MAKE) TILES=y DESTDIR=build-win FORCE_SSE=y SAVEDIR='~/crawl' install
	mv build-win/crawl.exe build-win/crawl-tiles.exe
	+$(MAKE) TILES=  DESTDIR=build-win FORCE_SSE=y SAVEDIR='~/crawl' install
	mv build-win/crawl.exe build-win/crawl-console.exe
	mv build-win/docs/CREDITS.txt build-win/
	echo $(SRC_VERSION) >build-win/version.txt

ZIP_DOCS=../../LICENSE
package-windows-tiles:
ifneq ($(GAME),crawl.exe)
	@echo "This is only for Windows; please specify CROSSHOST.";false
endif
	+$(MAKE) clean
	+$(MAKE) TILES=y DESTDIR=build-win/stone_soup-tiles-$(MAJOR_VERSION) FORCE_SSE=y install
	cp -p $(ZIP_DOCS) build-win/stone_soup-tiles-$(MAJOR_VERSION)/
	cd build-win && zip -9r ../stone_soup-$(SRC_VERSION)-tiles-win32.zip *
	if which advzip >/dev/null;then advzip -z3 stone_soup-$(SRC_VERSION)-tiles-win32.zip;fi
	ln -sf stone_soup-$(SRC_VERSION)-tiles-win32.zip stone_soup-latest-tiles-win32.zip
	$(RM) -r build-win

package-windows-console:
ifneq ($(GAME),crawl.exe)
	@echo "This is only for Windows; please specify CROSSHOST.";false
endif
	+$(MAKE) clean
	+$(MAKE) DESTDIR=build-win/stone_soup-console-$(MAJOR_VERSION) FORCE_SSE=y install
	cp -p $(ZIP_DOCS) build-win/stone_soup-console-$(MAJOR_VERSION)/
	cd build-win && zip -9r ../stone_soup-$(SRC_VERSION)-console-win32.zip *
	if which advzip >/dev/null;then advzip -z3 stone_soup-$(SRC_VERSION)-console-win32.zip;fi
	ln -sf stone_soup-$(SRC_VERSION)-console-win32.zip stone_soup-latest-console-win32.zip
	$(RM) -r build-win

package-windows-zips: package-windows-tiles package-windows-console

#############################################################################
# Building Mac app bundles
#
# apple silicon universal builds: just brute-force this with submake commands.
# I couldn't see a cleaner way to do this within the Makefile structure,
# relative to my current knowledge of how this can be done: it requires
# building binaries for each target and then merging them with the `lipo`
# utility.

# Note: cross-compiling tiles on mac requires pkg-config and libpng to be
# installed, e.g. via homebrew

# XX parameterise these in some less dumb way
# these two targets rely heavily on passing flags to submakes
crawl-x86_64-apple-macos10.7:
ifeq ($(MAC_TARGET),)
	+$(MAKE) MAC_TARGET=x86_64-apple-macos10.7
endif

crawl-arm64-apple-macos11:
ifeq ($(MAC_TARGET),)
	+$(MAKE) MAC_TARGET=arm64-apple-macos11
endif

crawl-universal:
	# do these via submake so that -jn doesn't create havoc
	+$(MAKE) crawl-arm64-apple-macos11
	+$(MAKE) crawl-x86_64-apple-macos10.7
	lipo -create -output crawl-universal crawl-arm64-apple-macos11 crawl-x86_64-apple-macos10.7

clean-mac:
	$(RM) crawl-universal crawl-arm64-apple-macos11 crawl-x86_64-apple-macos10.7

# XX is there a way to make these just respect TILES=y and have only 2 targets?
mac-app-tiles-universal: crawl-universal
	+$(MAKE) -j1 -C mac -f Makefile.app-bundle tiles BUILD_UNIVERSAL=y

mac-app-console-universal: crawl-universal
	+$(MAKE) -j1 -C mac -f Makefile.app-bundle BUILD_UNIVERSAL=y

mac-app-tiles: all
	+$(MAKE) -j1 -C mac -f Makefile.app-bundle tiles

mac-app-console: all
	+$(MAKE) -j1 -C mac -f Makefile.app-bundle

#############################################################################
# Building the Android package
#

NPROC := $(shell nproc)

android-project/app/build.gradle: android-project/app/build.gradle.in
	sed -e "s/@ANDROID_VERSION@/${ANDROID}/" -e "s/@CRAWL_VERSION@/${SRC_VERSION}/" -e "s/@NPROC@/${NPROC}/" <android-project/app/build.gradle.in >android-project/app/build.gradle
android-project/local.properties: android-project/local.properties.in
	sed -e "s~@HOME@~${HOME}~" <android-project/local.properties.in >android-project/local.properties

android: greet check-fonts docs install-data .android-cflags android-project/local.properties android-project/app/build.gradle
ifneq ($(ANDROID),)
	mkdir -p android-project/app/src/main/res/drawable
	$(COPY) dat/tiles/logo.png android-project/app/src/main/res/drawable/logo.png
	$(COPY) dat/tiles/stone_soup_icon-512x512.png android-project/app/src/main/res/drawable/icon.png
else
	@echo Please define ANDROID to use this build.
	@exit 1
endif

#############################################################################
# Canned tests
#

test: test-test test-all
nonwiztest: test-test test-nonwiz
nondebugtest: test-all

test-%: $(GAME) builddb util/fake_pty
	test/stress/run $*
	@echo "Finished: $*"

util/fake_pty: util/fake_pty.c
	$(QUIET_HOSTCC)$(if $(HOSTCC),$(HOSTCC),$(CC)) $(if $(TRAVIS),-DTIMEOUT=9,-DTIMEOUT=60) -Wall $< -o $@ -lutil

# Should be not needed, but the race condition in bug #6509 is hard to fix.
builddb: $(GAME)
	./$(GAME) --builddb --reset-cache
.PHONY: builddb
