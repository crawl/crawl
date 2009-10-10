YACC := bison -y
LEX := flex

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
        QUIET_YACC     = @echo '   ' YACC $@;
        QUIET_LEX      = @echo '   ' LEX $@;
        export V
endif
endif

all: levcomp.tab.cc levcomp.lex.cc

clean:
	rm -f levcomp.tab.cc levcomp.tab.h
	rm -f levcomp.lex.cc
	rm -f *.o

%.tab.cc: %.tab.c
	@mv $< $@

%.lex.cc: %.lpp
	$(QUIET_LEX)$(LEX) -o$@ $<

%.tab.c: %.ypp
	$(QUIET_YACC)$(YACC) -d -b $* $<
