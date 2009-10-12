# -*- Makefile -*- for PCRE (Win32, MinGW)

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
        QUIET_CC       = @echo '   ' CC $@;
        QUIET_AR       = @echo '   ' AR $@;
        export V
endif
endif

OBJECTS := \
	pcre_chartables.o \
	pcre_compile.o \
	pcre_config.o \
	pcre_dfa_exec.o \
	pcre_exec.o \
	pcre_fullinfo.o \
	pcre_get.o \
	pcre_globals.o \
	pcre_info.o \
	pcre_maketables.o \
	pcre_newline.o \
	pcre_ord2utf8.o \
	pcre_refcount.o \
	pcre_study.o \
	pcre_tables.o \
	pcre_try_flipped.o \
	pcre_ucp_searchfuncs.o \
	pcre_valid_utf8.o \
	pcre_version.o \
	pcre_xclass.o

CC ?= gcc
AR := ar rcu
RANLIB := ranlib
DEL := rm -f

CFLAGS += -I. -DHAVE_CONFIG_H
LDFLAGS :=

LIBNAME := libpcre.a

all: $(LIBNAME)

clean:
	$(DEL) $(LIBNAME)
	$(DEL) $(OBJECTS)

$(LIBNAME) : $(OBJECTS)
	$(QUIET_AR)$(AR) $@ $?
	@$(RANLIB) $@

%.o: %.c
	$(QUIET_CC)$(CC) $(CFLAGS) -o $@ -c $<

