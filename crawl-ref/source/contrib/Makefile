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
	@cd $@; \
	$(MAKE) all prefix=$(PREFIX); \
	$(MAKE) install prefix=$(PREFIX)

libpng freetype: zlib
sdl-image: sdl libpng

.PHONY: $(SUBDIRS)
