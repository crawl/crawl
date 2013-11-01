YACC := bison -y
LEX  := flex
RM   ?= rm -f

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
        QUIET_YACC     = @echo '   ' YACC $@;
        QUIET_LEX      = @echo '   ' LEX $@;
        export V
endif
endif

all: levcomp.tab.cc levcomp.lex.cc

clean:
	$(RM) levcomp.tab.cc levcomp.tab.h levcomp.lex.cc *.o *.d

%.lex.cc: %.lpp
	$(QUIET_LEX)$(LEX) -o$@ $<

%.tab.cc: %.ypp
	$(QUIET_YACC)$(YACC) --defines=$*.tab.h -b $* -o $@ $<
