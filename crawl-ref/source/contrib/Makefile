PREFIX:=$(shell pwd)/install

SUBDIRS = lua/src sqlite sdl sdl-image freetype libpng pcre zlib

all:
	@$(MAKE) $(SUBDIRS)

clean distclean:
	@for a in $(SUBDIRS); do \
	    make -C $$a distclean; \
	done
	rm -rf $(PREFIX)

$(SUBDIRS):
	@if [ ! -x $@ ]; then \
	    echo "*******************************************************************************"; \
		echo; \
	    echo "The '$@' folder is missing. Have you run 'git submodule update --init'?"; \
		echo; \
	    echo "*******************************************************************************"; \
	    exit 1; \
	fi
	@cd $@ && \
	$(MAKE) all prefix=$(PREFIX) && \
	$(MAKE) install prefix=$(PREFIX)

libpng freetype: zlib
sdl-image: sdl libpng

.PHONY: $(SUBDIRS)
