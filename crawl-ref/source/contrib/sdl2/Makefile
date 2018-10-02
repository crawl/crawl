# -*- Makefile -*- for sdl2

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
	QUIET_CC       = @echo '   ' CC $@;
	QUIET_AR       = @echo '   ' AR $@;
	QUIET_RANLIB   = @echo '   ' RANLIB $@;
	QUIET_INSTALL  = @echo '   ' INSTALL $<;
	export V
endif
endif

uname_S ?= $(shell uname -s)

# Since Windows builds could be done with MinGW or Cygwin,
# set a TARGET_OS_WINDOWS flag when either shows up.
ifneq (,$(findstring MINGW,$(uname_S)))
TARGET_OS_WINDOWS := YesPlease
endif
ifneq (,$(findstring MSYS,$(uname_S)))
TARGET_OS_WINDOWS := YesPlease
endif
ifneq (,$(findstring CYGWIN,$(uname_S)))
TARGET_OS_WINDOWS := YesPlease
endif

SDL2_LIB = libSDL2.a
SDL2MAIN_LIB = libSDL2main.a
AR    ?= ar
CC    ?= gcc
RANLIB?= ranlib
RM    ?= rm -f

CFLAGS ?= -O2

prefix ?= /usr/local
libdir := $(prefix)/lib
includedir := $(prefix)/include/SDL2

HEADERS = include/*.h

SDL2_SOURCES = \
    src/*.c \
    src/audio/*.c \
    src/atomic/*.c \
    src/cpuinfo/*.c \
    src/events/*.c \
    src/file/*.c \
    src/haptic/*.c \
    src/joystick/*.c \
    src/power/*.c \
    src/render/*.c \
    src/stdlib/*.c \
    src/thread/*.c \
    src/timer/*.c \
    src/video/*.c \
	src/audio/disk/*.c \
	src/audio/dummy/*.c \
	src/filesystem/dummy/*.c \
	src/haptic/dummy/*.c \
	src/joystick/dummy/*.c \
	src/render/software/*.c \
	src/render/opengl/*.c \
	src/timer/dummy/*.c \
	src/video/dummy/*.c \
	src/loadso/dummy/*.c

SDL2MAIN_SOURCES = \
    src/main/dummy/*.c

ifeq ($(uname_S),Linux)
SDL2_SOURCES += \
    src/audio/alsa/*.c \
    src/audio/dsp/*.c \
    src/core/linux/*.c \
    src/core/unix/*.c \
    src/haptic/linux/*.c \
    src/joystick/linux/*.c \
    src/joystick/steam/*.c \
    src/loadso/dlopen/*.c \
    src/power/linux/*.c \
    src/thread/pthread/*.c \
    src/timer/unix/*.c \
    src/video/directfb/*.c \
    src/video/x11/*.c

CFLAGS += -D_GNU_SOURCE -D_REENTRANT
endif

ifeq ($(uname_S),Darwin)
SDL2_SOURCES += \
    src/audio/coreaudio/*.m \
    src/file/cocoa/*.m \
    src/filesystem/cocoa/*.m \
    src/haptic/darwin/*.c \
    src/joystick/darwin/*.c \
    src/loadso/dlopen/*.c \
    src/power/macosx/*.c \
    src/thread/pthread/*.c \
    src/timer/unix/*.c \
    src/video/cocoa/*.m
endif

ifdef TARGET_OS_WINDOWS
SDL2_SOURCES += \
    src/audio/directsound/*.c \
    src/audio/winmm/*.c \
    src/core/windows/*.c \
    src/filesystem/windows/*.c \
    src/haptic/windows/*.c \
    src/joystick/windows/*.c \
    src/libm/*.c \
    src/loadso/windows/*.c \
    src/power/windows/*.c \
    src/render/direct3d/*.c \
    src/thread/windows/*.c \
    src/thread/generic/*.c \
    src/timer/windows/*.c \
    src/video/windows/*.c \

SDL2MAIN_SOURCES += \
    src/main/windows/*.c
endif

SDL2_SOURCES := $(shell echo $(SDL2_SOURCES))
SDL2MAIN_SOURCES := $(shell echo $(SDL2MAIN_SOURCES))
HEADERS := $(shell echo $(HEADERS))

HEADERS_INST := $(patsubst include/%,$(includedir)/%,$(HEADERS))

# First handle .c files
SDL2_OBJECTS := $(patsubst %.c,%.o,$(SDL2_SOURCES))
SDL2MAIN_OBJECTS := $(patsubst %.c,%.o,$(SDL2MAIN_SOURCES))

# Now .m files
SDL2_OBJECTS := $(patsubst %.m,%.o,$(SDL2_OBJECTS))
SDL2MAIN_OBJECTS := $(patsubst %.m,%.o,$(SDL2MAIN_OBJECTS))

CFLAGS += -I. -Iinclude -DNO_STDIO_REDIRECT

ifeq (,$(shell ./test_xinput.sh))
CFLAGS += -DHAVE_XINPUT_GAMEPAD_EX -DHAVE_XINPUT_STATE_EX
endif

.PHONY: install

all: $(SDL2_LIB) $(SDL2MAIN_LIB)

$(includedir)/%.h: include/%.h
	-@if [ ! -d $(includedir)  ]; then mkdir -p $(includedir); fi
	$(QUIET_INSTALL)cp $< $@
	@chmod 0644 $@

$(libdir)/%.a: %.a
	-@if [ ! -d $(libdir)  ]; then mkdir -p $(libdir); fi
	$(QUIET_INSTALL)cp $< $@
	@chmod 0644 $@

install: $(HEADERS_INST) $(libdir)/$(SDL2_LIB) $(libdir)/$(SDL2MAIN_LIB)

clean:
	$(RM) $(SDL2_OBJECTS) $(SDL2MAIN_OBJECTS) *.a .cflags

distclean: clean

$(SDL2_LIB): $(SDL2_OBJECTS)
	@$(RM) $@
	$(QUIET_AR)$(AR) rcu $@ $^
	$(QUIET_RANLIB)$(RANLIB) $@

$(SDL2MAIN_LIB): $(SDL2MAIN_OBJECTS)
	@$(RM) $@
	$(QUIET_AR)$(AR) rcu $@ $^
	$(QUIET_RANLIB)$(RANLIB) $@

%.o: %.c .cflags
	$(QUIET_CC)$(CC) $(CFLAGS) -o $@ -c $<

%.o: %.m .cflags
	$(QUIET_CC)$(CC) $(CFLAGS) -o $@ -c $<

TRACK_CFLAGS = $(subst ','\'',$(CC) $(CFLAGS))

.cflags: .force-cflags
	@FLAGS='$(TRACK_CFLAGS)'; \
    if test x"$$FLAGS" != x"`cat .cflags 2>/dev/null`" ; then \
	echo "    * rebuilding sdl2: new build flags or prefix"; \
	echo "$$FLAGS" > .cflags; \
    fi

.PHONY: .force-cflags
