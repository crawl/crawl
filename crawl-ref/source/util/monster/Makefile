# Use 'make stable' to compile monster, 'make trunk' to compile monster-trunk.
# Use 'make install' to install monster, 'make install-trunk' to install
# monster-trunk.

.PHONY: crawl

# Use master
TRUNK = master

CRAWL_PATH=crawl-ref/crawl-ref/source

CXX = g++

PYTHON = python

ifdef USE_MERGE_BASE
MERGE_BASE := $(shell cd $(CRAWL_PATH) ; git merge-base HEAD $(USE_MERGE_BASE))
endif

VERSION = $(shell cd $(CRAWL_PATH) ; git describe)

CFLAGS = -Wall -Wno-parentheses -DNDEBUG -DUNIX -I$(CRAWL_PATH) \
	-I$(CRAWL_PATH)/rltiles -I/usr/include/ncursesw -g -O0 --std=c++11

LFLAGS = -lncursesw -lz -lpthread -g

LUA_INCLUDE_DIR = /usr/include/lua5.1

ifeq (,$(wildcard $(LUA_INCLUDE_DIR)/lua.h))
	BUILD_LUA = y
endif

ifeq (,$(BUILD_LUA))
	CFLAGS += -I$(LUA_INCLUDE_DIR)
	LFLAGS += -llua5.1
else
	LUASRC := $(CRAWL_PATH)/contrib/lua/src
	LUALIB = lua
	LUALIBA = lib$(LUALIB).a
	CFLAGS += -I$(LUASRC)
	LFLAGS += $(LUASRC)/$(LUALIBA)
	CONTRIB_OBJECTS += $(LUASRC)/$(LUALIBA)
endif

SQLITE_INCLUDE_DIR = /usr/include

ifeq (,$(wildcard $(SQLITE_INCLUDE_DIR)/sqlite3.h))
	BUILD_SQLITE = y
endif

ifeq (,$(BUILD_SQLITE))
	LFLAGS += -lsqlite3
else
	SQLSRC := $(CRAWL_PATH)/contrib/sqlite
	SQLLIB   := sqlite3
	SQLLIBA  := lib$(SQLLIB).a
	FSQLLIBA := $(SQLSRC)/$(SQLLIBA)
	CFLAGS += -I$(SQLSRC)
	LFLAGS += $(FSQLLIBA)
	CONTRIB_OBJECTS += $(FSQLLIBA)
endif


include $(CRAWL_PATH)/Makefile.obj

CRAWL_OBJECTS := $(OBJECTS:main.o=)
CRAWL_OBJECTS += libunix.o version.o
TILEDEFS := floor wall feat main player gui icons dngn unrand
CRAWL_OBJECTS += $(TILEDEFS:%=rltiles/tiledef-%.o)

MONSTER_OBJECTS = monster-main.o vault_monster_data.o vault_monsters.o
ALL_OBJECTS = $(MONSTER_OBJECTS) $(CRAWL_OBJECTS:%=$(CRAWL_PATH)/%)

all: vaults trunk

crawl:
	+${MAKE} -C $(CRAWL_PATH) NO_OPTIMIZE=y DEBUG=$(DEBUG) TILES= NO_LUA_BINDINGS=y

.cc.o:
	${CXX} ${CFLAGS} -o $@ -c $<

vault_monster_data.o: vaults

trunk: monster-trunk

vault_monster_data.o:
	${CXX} ${CFLAGS} -o vault_monster_data.o -c vault_monster_data.cc

vaults: | update-cdo-git
	rm -f vault_monster_data.cc vault_monster_data.o
	${PYTHON} parse_des.py --verbose

update-cdo-git:
	[ "`hostname`" != "ipx14623" ] || sudo -H -u git /var/cache/git/crawl-ref.git/update.sh

monster-trunk: vaults update-cdo-git crawl $(MONSTER_OBJECTS) $(CONTRIB_OBJECTS)
	g++ $(CFLAGS) -o $@ $(ALL_OBJECTS) $(LFLAGS)

$(LUASRC)/$(LUALIBA):
	echo Building Lua...
	cd $(LUASRC) && $(MAKE) all

$(FSQLLIBA):
	echo Building SQLite
	cd $(SQLSRC) && $(MAKE)

test: monster
	./monster-trunk quasit

install-trunk: monster-trunk tile_info.txt
	strip -s monster-trunk
	cp monster-trunk $(HOME)/bin/
	if [ -f ~/source/announcements.log ]; then \
	  echo 'Monster database of master branch on crawl.develz.org updated to: $(VERSION)' >>~/source/announcements.log;\
	fi

tile_info.txt:
	${PYTHON} parse_tiles.py --verbose

clean:
	rm -f *.o
	rm -f monster monster-trunk
	rm -f *.pyc vault_monster_data.cc
	cd $(CRAWL_PATH) && git clean -f -d -x && git pull
