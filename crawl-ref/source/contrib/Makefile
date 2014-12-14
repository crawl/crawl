PREFIX := install

SUBDIRS = sqlite sdl2 sdl2-image sdl2-mixer freetype libpng pcre zlib
ARCH = unknown

ifdef USE_LUAJIT
SUBDIRS += luajit/src
else
SUBDIRS += lua/src
endif

all:
	@$(MAKE) $(SUBDIRS)

clean distclean:
	@for a in $(SUBDIRS); do \
	    $(MAKE) -C $$a distclean; \
	done
	rm -rf $(PREFIX)

$(SUBDIRS):
	@if [ ! -x $@ ]; then \
	    echo "**********************************************************"; \
	    echo; \
	    echo "The '$@' directory is missing. Have you run"; \
	    echo "   'git submodule update --init'?"; \
	    echo "(It's probably better to install all native dependencies instead.)"; \
	    echo; \
	    echo "**********************************************************"; \
	    exit 1; \
	fi
	@if [ ! -f $@/Makefile ] && [ ! -f $@/makefile ]; then \
	    echo "**********************************************************"; \
	    echo; \
	    echo "The '$@' directory exists, but the Makefile is missing!"; \
	    echo "Did 'git submodule update' work properly?"; \
	    echo "(It's probably better to install all native dependencies instead.)"; \
	    echo; \
	    echo "**********************************************************"; \
	    exit 1; \
	fi
	@cd $@ && \
	$(MAKE) all prefix=$(shell echo $@ | perl -pe 's/[^\/]+/../g')/$(PREFIX)/$(ARCH) && \
	$(MAKE) install prefix=$(shell echo $@ | perl -pe 's/[^\/]+/../g')/$(PREFIX)/$(ARCH)

libpng freetype: zlib
sdl2-image: sdl2 libpng
sdl2-mixer: sdl2

.PHONY: $(SUBDIRS)
