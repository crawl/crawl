# -*- Makefile -*- for stripped down SQLite 3 static lib.

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
        QUIET_CC       = @echo '   ' CC $@;
        QUIET_AR       = @echo '   ' AR $@;
        export V
endif
endif

LIBSQL = libsqlite3.a
AR     = ar rcu
CC    ?= gcc
RANLIB = ranlib
RM_F   = rm -f

# Omit SQLite features we don't need.
CFLAGS ?= -O2
CFLAGS +=-DSQLITE_OMIT_AUTHORIZATION \
		 -DSQLITE_OMIT_AUTOVACUUM \
		 -DSQLITE_OMIT_COMPLETE \
		 -DSQLITE_OMIT_BLOB_LITERAL \
		 -DSQLITE_OMIT_COMPOUND_SELECT \
		 -DSQLITE_OMIT_CONFLICT_CLAUSE \
		 -DSQLITE_OMIT_DATETIME_FUNCS \
		 -DSQLITE_OMIT_EXPLAIN \
	     -DSQLITE_OMIT_INTEGRITY_CHECK \
		 -DSQLITE_OMIT_PAGER_PRAGMAS \
		 -DSQLITE_OMIT_PROGRESS_CALLBACK \
		 -DSQLITE_OMIT_SCHEMA_PRAGMAS \
		 -DSQLITE_OMIT_SCHEMA_VERSION_PRAGMAS \
		 -DSQLITE_OMIT_TCL_VARIABLE \
	     -DSQLITE_OMIT_LOAD_EXTENSION \
		 -DSQLITE_PREFER_PROXY_LOCKING=0 \
		 -DSQLITE_THREADSAFE=0 \
		 -w

all: $(LIBSQL)

clean:
	$(RM_F) *.o
	$(RM_F) *.a

$(LIBSQL): sqlite3.o
	$(QUIET_AR)$(AR) $@ $^
	@$(RANLIB) $@

%.o: %.c
	$(QUIET_CC)$(CC) $(CFLAGS) -o $@ -c $<
