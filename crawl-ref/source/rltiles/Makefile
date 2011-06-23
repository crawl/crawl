uname_S := $(shell uname -s)

ifneq (,$(findstring MINGW,$(uname_S)))
LDFLAGS += -lmingw32
endif

# Note: since generation of tiles is done on the host, we don't care about
# cross-compilation here.
#
# Also, cross-compilation with no system libraries for host rather than target
# is not supported.  If host=target, contribs are enough.

ifndef CONTRIB_SDL
SDL_CFLAGS := $(shell sdl-config --cflags 2> /dev/null || echo "-I../contrib/install/$(ARCH)/include/SDL")
SDL_LDFLAGS := $(shell sdl-config --libs 2> /dev/null && echo "-lSDL_image" || echo "../contrib/install/$(ARCH)/lib/libSDL.a ../contrib/install/$(ARCH)/lib/libSDLmain.a ../contrib/install/$(ARCH)/lib/libSDL_image.a")
else
SDL_CFLAGS := -I../contrib/install/$(ARCH)/include/SDL
SDL_LDFLAGS := ../contrib/install/$(ARCH)/lib/libSDL.a ../contrib/install/$(ARCH)/lib/libSDLmain.a ../contrib/install/$(ARCH)/lib/libSDL_image.a
endif

ifndef NO_PKGCONFIG
PNG_INCLUDE := $(shell pkg-config libpng --cflags 2> /dev/null || echo "-I../contrib/install/$(ARCH)/include")
PNG_LIB := $(shell pkg-config libpng --libs 2> /dev/null || echo "../contrib/install/$(ARCH)/lib/libpng.a ../contrib/install/$(ARCH)/lib/libz.a")
else
PNG_INCLUDE := -I../contrib/install/$(ARCH)/include
PNG_LIB := ../contrib/install/$(ARCH)/lib/libpng.a ../contrib/install/$(ARCH)/lib/libz.a
endif

CFLAGS := -O2 $(SDL_CFLAGS) $(PNG_INCLUDE)
LDFLAGS += $(SDL_LDFLAGS) $(PNG_LIB)

ifneq (,$(findstring MINGW,$(uname_S)))
LDFLAGS += -lgdi32 -lwinmm
endif
ifeq ($(uname_S),Darwin)
LDFLAGS += -framework AppKit -framework AudioUnit -framework Carbon -framework IOKit -framework OpenGL
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
CXX = $(LMACH)g++

DELETE = rm -f

TOOLDIR := tool
TILEGEN := $(TOOLDIR)/tilegen.elf

INPUTS := main dngn floor wall feat player gui icons
INPUTFILES := $(INPUTS:%=dc-%.txt)
HEADERS := $(INPUTS:%=tiledef-%.h)
HTML := $(INPUTS:%=tile-%.html)
SOURCE := $(INPUTS:%=tiledef-%.cc)
IMAGES := $(INPUTS:%=%.png)

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
        QUIET_CXX      = @echo '   ' CXX $@;
        QUIET_GEN      = @echo '   ' GEN $(lastword $(subst -, ,$*));
        QUIET_LINK     = @echo '   ' LINK $@;
        export V
endif
endif

BASE_OBJECTS := tile_colour.o tile.o tile_page.o tile_list_processor.o main.o
OBJECTS := $(BASE_OBJECTS:%=$(TOOLDIR)/%)

all: $(TILEGEN) $(HEADERS) $(SOURCE) $(IMAGES)

%.png tiledef-%.h tiledef-%.cc: dc-%.txt | $(TILEGEN)
	$(QUIET_GEN)$(TILEGEN) $<

DEPS := $(shell ls $(OBJECTS:%.o=%.d) $(INPUTS:%=%.d) 2> /dev/null)

ifneq ($(DEPS),)
-include $(DEPS)
endif

clean:
	$(DELETE) $(HEADERS) $(OBJECTS) $(TILEGEN) $(SOURCE) $(IMAGES) $(HTML) $(DEPS)

distclean: clean

%.o: %.cc
	$(QUIET_CXX)$(CXX) $(CFLAGS) -Wp,-MD,$*.d,-MT,$@ -c $< -o $@

$(TILEGEN): $(OBJECTS)
	$(QUIET_LINK)$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: all clean distclean
