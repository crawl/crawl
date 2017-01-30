uname_S := $(shell uname -s)

ifneq (,$(findstring MINGW,$(uname_S)))
LDFLAGS += -lmingw32
endif

# Note: since generation of tiles is done on the host, we don't care about
# cross-compilation here.
#
# Also, cross-compilation with no system libraries for host rather than target
# is not supported.  If host=target, contribs are enough.

PKGCONFIG = pkg-config
CFLAGS := -O2 $(STDFLAG) -g -Wall -Wextra -Wno-parentheses -Wno-unused-parameter

ifdef ANDROID
  CXXFLAGS :=
  LDFLAGS :=
  LIBS :=
endif

ifdef TILES
  ifndef NO_PKGCONFIG
    PNG_INCLUDE := $(shell $(PKGCONFIG) libpng --cflags 2> /dev/null || echo "-I../contrib/install/$(ARCH)/include")
    PNG_LIB := $(shell $(PKGCONFIG) libpng --libs 2> /dev/null || echo "../contrib/install/$(ARCH)/lib/libpng.a ../contrib/install/$(ARCH)/lib/libz.a")
  else
    PNG_INCLUDE := -I../contrib/install/$(ARCH)/include
    PNG_LIB := ../contrib/install/$(ARCH)/lib/libpng.a ../contrib/install/$(ARCH)/lib/libz.a
  endif

  CFLAGS += $(PNG_INCLUDE)
  LDFLAGS += $(PNG_LIB)

  CFLAGS += -DUSE_TILE
endif

ifdef DEBUG
CFLAGS += -O0 -DDEBUG -ggdb
endif

ifneq (,$(findstring MINGW,$(uname_S)))
LDFLAGS += -lgdi32 -lwinmm -lole32 -loleaut32 -limm32 -lshell32 -lversion -luuid
endif
ifeq ($(uname_S),Darwin)
LDFLAGS += -framework AppKit -framework AudioUnit -framework CoreAudio -framework ForceFeedback -framework Carbon -framework IOKit -framework OpenGL
endif
ifeq ($(uname_S),Linux)
LDFLAGS += -ldl -lpthread
endif

# Attempt to use a full compiler name, to make
# distcc builds work nicely.
LMACH := $(shell gcc -dumpmachine)-
ifeq ($(LMACH),-)
LMACH :=
endif
ifeq ($(shell which $(LMACH)gcc > /dev/null 2> /dev/null && echo "Yes"),)
LMACH :=
endif
HOSTCXX ?= $(LMACH)g++

DELETE = rm -f

TOOLDIR := tool
TILEGEN := $(TOOLDIR)/tilegen.elf

INPUTS := main dngn floor wall feat player gui icons
INPUTFILES := $(INPUTS:%=dc-%.txt)
HEADERS := $(INPUTS:%=tiledef-%.h)
HTML := $(INPUTS:%=tile-%.html)
SOURCE := $(INPUTS:%=tiledef-%.cc)
IMAGES := $(INPUTS:%=%.png)
JAVASCRIPT := $(INPUTS:%=tileinfo-%.js)

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
	QUIET_HOSTCXX  = @echo '   ' HOSTCXX $@;
	QUIET_GEN      = @echo '   ' GEN $@;
	QUIET_HOSTLINK = @echo '   ' HOSTLINK $@;
	export V
endif
endif

BASE_OBJECTS := tile_colour.o tile.o tile_page.o tile_list_processor.o main.o

OBJECTS := $(BASE_OBJECTS:%=$(TOOLDIR)/%)

all: $(TILEGEN) $(HEADERS) $(SOURCE) $(JAVASCRIPT)
ifdef TILES
all: $(IMAGES)
endif

%.png: dc-%.txt $(TILEGEN)
	$(QUIET_GEN)$(TILEGEN) -i $<

ifdef TILES
# Keep coordinates fresh
tiledef-%.h tiledef-%.cc tileinfo-%.js: dc-%.txt $(TILEGEN) %.png
else
tiledef-%.h tiledef-%.cc tileinfo-%.js: dc-%.txt $(TILEGEN)
endif
	$(QUIET_GEN)$(TILEGEN) -c $<

# CFLAGS difference check
TRACK_CFLAGS = $(subst ','\'',$(HOSTCXX) $(CFLAGS))           # (stray ' for highlights)

.cflags: .force-cflags
	@FLAGS='$(TRACK_CFLAGS)'; \
    if test x"$$FLAGS" != x"`cat .cflags 2>/dev/null`" ; then \
	echo "    * rebuilding tilegen: new build flags or prefix"; \
	echo "$$FLAGS" > .cflags; \
    fi

.PHONY: .force-cflags

##########################################################################
# Dependencies

gui.png tiledef-gui.h tileinfo-gui.js: dc-spells.txt dc-skills.txt dc-commands.txt dc-abilities.txt dc-invocations.txt
main.png tiledef-main.h tileinfo-main.js: dc-item.txt dc-unrand.txt dc-corpse.txt dc-misc.txt
player.png tiledef-player.h tileinfo-player.js: dc-mon.txt dc-tentacles.txt dc-zombie.txt dc-demon.txt

DEPS := $(OBJECTS:%.o=%.d) $(INPUTS:%=%.d)

-include $(DEPS)

clean:
	$(DELETE) $(HEADERS) $(OBJECTS) $(TILEGEN) $(SOURCE) $(IMAGES) $(HTML) \
		$(DEPS) $(JAVASCRIPT) .cflags

distclean: clean

%.o: %.cc .cflags
	$(QUIET_HOSTCXX)$(HOSTCXX) $(CFLAGS) -MMD -c $< -o $@

$(TILEGEN): $(OBJECTS)
	$(QUIET_HOSTLINK)$(HOSTCXX) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: all clean distclean
