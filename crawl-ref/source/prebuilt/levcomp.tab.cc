
/*  A Bison parser, made from levcomp.ypp
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	BRANCHDEF	257
#define	BRANCH	258
#define	DESC	259
#define	DEFAULT	260
#define	DEFAULT_DEPTH	261
#define	SYMBOL	262
#define	TAGS	263
#define	NAME	264
#define	DEPTH	265
#define	ORIENT	266
#define	PLACE	267
#define	CHANCE	268
#define	FLAGS	269
#define	MONS	270
#define	ITEM	271
#define	ROOT_DEPTH	272
#define	ENTRY_MSG	273
#define	EXIT_MSG	274
#define	ROCK_COLOUR	275
#define	FLOOR_COLOUR	276
#define	ENCOMPASS	277
#define	FLOAT	278
#define	NORTH	279
#define	EAST	280
#define	SOUTH	281
#define	WEST	282
#define	NORTHEAST	283
#define	SOUTHEAST	284
#define	SOUTHWEST	285
#define	NORTHWEST	286
#define	LEVEL	287
#define	END	288
#define	PVAULT	289
#define	PMINIVAULT	290
#define	MONSTERS	291
#define	ENDMONSTERS	292
#define	CHARACTER	293
#define	NO_HMIRROR	294
#define	NO_VMIRROR	295
#define	NO_ROTATE	296
#define	PANDEMONIC	297
#define	DASH	298
#define	COMMA	299
#define	QUOTE	300
#define	OPAREN	301
#define	CPAREN	302
#define	INTEGER	303
#define	STRING	304
#define	MAP_LINE	305
#define	MONSTER_NAME	306
#define	ITEM_INFO	307
#define	IDENTIFIER	308

#line 1 "levcomp.ypp"


#include "AppHdr.h"
#include "libutil.h"
#include "levcomp.h"
#include "stuff.h"

int yylex();

extern int yylineno;

void yyerror(const char *e)
{
    io_cleanup();
    fprintf(stderr, "%s:%d: %s\n", lc_desfile.c_str(), yylineno, e);
    // Bail bail bail.
    exit(1);
}


#line 22 "levcomp.ypp"
typedef union
{
    int i;
    const char *text;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		78
#define	YYFLAG		-32768
#define	YYNTBASE	55

#define YYTRANSLATE(x) ((unsigned)(x) <= 308 ? yytranslate[x] : 85)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
    17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
    27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    48,    49,    50,    51,    52,    53,    54
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     3,     6,     8,    10,    12,    15,    20,    23,
    24,    27,    29,    31,    33,    35,    37,    39,    41,    43,
    45,    48,    49,    52,    54,    57,    59,    62,    66,    68,
    70,    72,    75,    79,    81,    83,    86,    88,    91,    95,
    97,   100,   102,   105,   107,   109,   111,   113,   115,   117,
   119,   121,   123,   125,   128,   129,   132,   134,   136,   138,
   140,   142,   145
};

static const short yyrhs[] = {    56,
     0,     0,    57,    56,     0,    58,     0,    60,     0,    59,
     0,     7,    75,     0,    61,    62,    82,    62,     0,    10,
    50,     0,     0,    63,    62,     0,    73,     0,    74,     0,
    76,     0,    77,     0,    79,     0,    70,     0,    67,     0,
    66,     0,    64,     0,     9,    65,     0,     0,    50,    65,
     0,     8,     0,     8,    50,     0,    17,     0,    17,    68,
     0,    69,    45,    68,     0,    69,     0,    53,     0,    16,
     0,    16,    71,     0,    72,    45,    71,     0,    72,     0,
    52,     0,    13,    50,     0,    11,     0,    11,    75,     0,
    49,    44,    49,     0,    49,     0,    14,    49,     0,    12,
     0,    12,    78,     0,    23,     0,    25,     0,    26,     0,
    27,     0,    28,     0,    29,     0,    30,     0,    31,     0,
    32,     0,    24,     0,    15,    80,     0,     0,    81,    80,
     0,    40,     0,    41,     0,    42,     0,    83,     0,    84,
     0,    84,    83,     0,    51,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
    54,    57,    58,    61,    62,    65,    68,    74,   113,   121,
   122,   125,   126,   127,   128,   129,   130,   131,   132,   133,
   136,   139,   140,   148,   149,   155,   156,   159,   160,   163,
   178,   179,   182,   183,   186,   201,   207,   208,   214,   219,
   225,   231,   232,   238,   239,   240,   241,   242,   243,   244,
   245,   246,   247,   250,   253,   254,   270,   271,   272,   275,
   278,   279,   282
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","BRANCHDEF",
"BRANCH","DESC","DEFAULT","DEFAULT_DEPTH","SYMBOL","TAGS","NAME","DEPTH","ORIENT",
"PLACE","CHANCE","FLAGS","MONS","ITEM","ROOT_DEPTH","ENTRY_MSG","EXIT_MSG","ROCK_COLOUR",
"FLOOR_COLOUR","ENCOMPASS","FLOAT","NORTH","EAST","SOUTH","WEST","NORTHEAST",
"SOUTHEAST","SOUTHWEST","NORTHWEST","LEVEL","END","PVAULT","PMINIVAULT","MONSTERS",
"ENDMONSTERS","CHARACTER","NO_HMIRROR","NO_VMIRROR","NO_ROTATE","PANDEMONIC",
"DASH","COMMA","QUOTE","OPAREN","CPAREN","INTEGER","STRING","MAP_LINE","MONSTER_NAME",
"ITEM_INFO","IDENTIFIER","file","definitions","definition","def","defdepth",
"level","name","metalines","metaline","tags","tagstrings","symbol","items","item_specifiers",
"item_specifier","mons","mnames","mname","place","depth","depth_range","chance",
"orientation","orient_name","flags","flagnames","flagname","map_def","map_lines",
"map_line", NULL
};
#endif

static const short yyr1[] = {     0,
    55,    56,    56,    57,    57,    58,    59,    60,    61,    62,
    62,    63,    63,    63,    63,    63,    63,    63,    63,    63,
    64,    65,    65,    66,    66,    67,    67,    68,    68,    69,
    70,    70,    71,    71,    72,    73,    74,    74,    75,    75,
    76,    77,    77,    78,    78,    78,    78,    78,    78,    78,
    78,    78,    78,    79,    80,    80,    81,    81,    81,    82,
    83,    83,    84
};

static const short yyr2[] = {     0,
     1,     0,     2,     1,     1,     1,     2,     4,     2,     0,
     2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     2,     0,     2,     1,     2,     1,     2,     3,     1,     1,
     1,     2,     3,     1,     1,     2,     1,     2,     3,     1,
     2,     1,     2,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     2,     0,     2,     1,     1,     1,     1,
     1,     2,     1
};

static const short yydefact[] = {     2,
     0,     0,     1,     2,     4,     6,     5,    10,    40,     7,
     9,     3,    24,    22,    37,    42,     0,     0,    55,    31,
    26,     0,    10,    20,    19,    18,    17,    12,    13,    14,
    15,    16,     0,    25,    22,    21,    38,    44,    53,    45,
    46,    47,    48,    49,    50,    51,    52,    43,    36,    41,
    57,    58,    59,    54,    55,    35,    32,    34,    30,    27,
    29,    63,    10,    60,    61,    11,    39,    23,    56,     0,
     0,     8,    62,    33,    28,     0,     0,     0
};

static const short yydefgoto[] = {    76,
     3,     4,     5,     6,     7,     8,    22,    23,    24,    36,
    25,    26,    60,    61,    27,    57,    58,    28,    29,    10,
    30,    31,    48,    32,    54,    55,    63,    64,    65
};

static const short yypact[] = {    14,
   -36,   -28,-32768,    14,-32768,-32768,-32768,     3,   -21,-32768,
-32768,-32768,   -20,   -19,   -36,   -22,   -18,   -16,   -15,   -24,
   -17,   -14,     3,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   -11,-32768,   -19,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   -15,-32768,-32768,   -10,-32768,-32768,
    -6,-32768,     3,-32768,   -14,-32768,-32768,-32768,-32768,   -24,
   -17,-32768,-32768,-32768,-32768,    29,    34,-32768
};

static const short yypgoto[] = {-32768,
    37,-32768,-32768,-32768,-32768,-32768,   -23,-32768,-32768,     7,
-32768,-32768,   -27,-32768,-32768,   -25,-32768,-32768,-32768,    28,
-32768,-32768,-32768,-32768,    -9,-32768,-32768,   -13,-32768
};


#define	YYLAST		52


static const short yytable[] = {    66,
    38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
    13,    14,     9,    15,    16,    17,    18,    19,    20,    21,
     1,    11,    33,     2,    51,    52,    53,    56,    77,    34,
    35,    49,    50,    78,    70,    59,    62,    67,    71,    72,
    12,    68,    37,    75,    74,    69,     0,     0,     0,     0,
     0,    73
};

static const short yycheck[] = {    23,
    23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
     8,     9,    49,    11,    12,    13,    14,    15,    16,    17,
     7,    50,    44,    10,    40,    41,    42,    52,     0,    50,
    50,    50,    49,     0,    45,    53,    51,    49,    45,    63,
     4,    35,    15,    71,    70,    55,    -1,    -1,    -1,    -1,
    -1,    65
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison.simple"
/* This file comes from bison-1.28.  */

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

#ifndef YYSTACK_USE_ALLOCA
#ifdef alloca
#define YYSTACK_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define YYSTACK_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
/* We think this test detects Watcom and Microsoft C.  */
/* This used to test MSDOS, but that is a bad idea
   since that symbol is in the user namespace.  */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#if 0 /* No need for malloc.h, which pollutes the namespace;
	 instead, just don't use alloca.  */
#include <malloc.h>
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
/* #include <malloc.h>  */
 #pragma alloca
#define YYSTACK_USE_ALLOCA
#else /* not MSDOS, or __TURBOC__, or _AIX */
#if 0
#ifdef __hpux /* haible@ilog.fr says this works for HPUX 9.05 and up,
		 and on HPUX 10.  Eventually we can turn this on.  */
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#endif /* __hpux */
#endif
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* YYSTACK_USE_ALLOCA not defined */

#ifdef YYSTACK_USE_ALLOCA
#define YYSTACK_ALLOC alloca
#else
#define YYSTACK_ALLOC malloc
#endif

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Define __yy_memcpy.  Note that the size argument
   should be passed with type unsigned int, because that is what the non-GCC
   definitions require.  With GCC, __builtin_memcpy takes an arg
   of type size_t, but it can handle unsigned int.  */

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     unsigned int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, unsigned int count)
{
  register char *t = to;
  register char *f = from;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 217 "/usr/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
#ifdef YYPARSE_PARAM
int yyparse (void *);
#else
int yyparse (void);
#endif
#endif

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;
  int yyfree_stacks = 0;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  if (yyfree_stacks)
	    {
	      free (yyss);
	      free (yyvs);
#ifdef YYLSP_NEEDED
	      free (yyls);
#endif
	    }
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
#ifndef YYSTACK_USE_ALLOCA
      yyfree_stacks = 1;
#endif
      yyss = (short *) YYSTACK_ALLOC (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1,
		   size * (unsigned int) sizeof (*yyssp));
      yyvs = (YYSTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1,
		   size * (unsigned int) sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1,
		   size * (unsigned int) sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
#line 54 "levcomp.ypp"
{ ;
    break;}
case 2:
#line 57 "levcomp.ypp"
{;
    break;}
case 3:
#line 58 "levcomp.ypp"
{;
    break;}
case 4:
#line 61 "levcomp.ypp"
{;
    break;}
case 5:
#line 62 "levcomp.ypp"
{;
    break;}
case 7:
#line 69 "levcomp.ypp"
{
                    lc_default_depth = lc_range;
                ;
    break;}
case 8:
#line 75 "levcomp.ypp"
{
                    if (lc_map.orient == MAP_FLOAT
                        || lc_map.is_minivault())
                    {
                        if (lc_map.map.width() > GXM - MAPGEN_BORDER * 2
                            || lc_map.map.height() > GYM - MAPGEN_BORDER * 2)
                        {
                            char buf[300];
                            snprintf(buf, sizeof buf, 
                                 "%s is too big: %dx%d - max %dx%d",
                                 lc_map.is_minivault()? "Minivault" : "Float",
                                 lc_map.map.width(), lc_map.map.height(),
                                 GXM - MAPGEN_BORDER * 2,
                                 GYM - MAPGEN_BORDER * 2);
                            yyerror(buf);
                        }
                    }
                    else
                    {
                        if (lc_map.map.width() > GXM
                            || lc_map.map.height() > GYM)
                        {
                            char buf[300];
                            snprintf(buf, sizeof buf, 
                                 "Map is too big: %dx%d - max %dx%d",
                                 lc_map.map.width(), lc_map.map.height(),
                                 GXM, GYM);
                            yyerror(buf);
                        }
                    }

                    if (lc_map.map.height() == 0)
                        yyerror("Must define map.");

                    add_parsed_map( lc_map );
                ;
    break;}
case 9:
#line 114 "levcomp.ypp"
{
                    lc_map.init();
                    lc_map.depth = lc_default_depth;
                    lc_map.name = yyvsp[0].text;
                ;
    break;}
case 21:
#line 136 "levcomp.ypp"
{;
    break;}
case 23:
#line 141 "levcomp.ypp"
{
                    lc_map.tags += " ";
                    lc_map.tags += yyvsp[-1].text;
                    lc_map.tags += " ";
                ;
    break;}
case 24:
#line 148 "levcomp.ypp"
{;
    break;}
case 25:
#line 150 "levcomp.ypp"
{
                    lc_map.random_symbols = yyvsp[0].text;
                ;
    break;}
case 26:
#line 155 "levcomp.ypp"
{;
    break;}
case 27:
#line 156 "levcomp.ypp"
{;
    break;}
case 30:
#line 164 "levcomp.ypp"
{
                    std::string error = lc_map.items.add_item(yyvsp[0].text);
                    if (error.size())
                    {
                        char errbuf[300];
                        snprintf(errbuf, sizeof errbuf,
                            "Invalid item descriptor: '%s' (%s)",
                                yyvsp[0].text, error.c_str());
                        yyerror(errbuf);
                    }
                    if (lc_map.items.size() > 8)
                        yyerror("Too many items specified (max 8)");
                ;
    break;}
case 31:
#line 178 "levcomp.ypp"
{;
    break;}
case 32:
#line 179 "levcomp.ypp"
{;
    break;}
case 35:
#line 187 "levcomp.ypp"
{
                    bool recognised = lc_map.mons.add_mons(yyvsp[0].text);
                    if (!recognised)
                    {
                        char buf[300];
                        snprintf(buf, sizeof buf, "unknown monster '%s'",
                                 yyvsp[0].text);
                        yyerror(buf);
                    }
                    if (lc_map.mons.size() > 7)
                        yyerror("Too many monsters specified (max 7)");
                ;
    break;}
case 36:
#line 202 "levcomp.ypp"
{
                    lc_map.place = yyvsp[0].text;
                ;
    break;}
case 37:
#line 207 "levcomp.ypp"
{;
    break;}
case 38:
#line 209 "levcomp.ypp"
{
                    lc_map.depth = lc_range;
                ;
    break;}
case 39:
#line 215 "levcomp.ypp"
{
                    lc_range.set(yyvsp[-2].i, yyvsp[0].i);
                ;
    break;}
case 40:
#line 220 "levcomp.ypp"
{
                    lc_range.set(yyvsp[0].i);
                ;
    break;}
case 41:
#line 226 "levcomp.ypp"
{
                    lc_map.chance = yyvsp[0].i;
                ;
    break;}
case 42:
#line 231 "levcomp.ypp"
{;
    break;}
case 43:
#line 233 "levcomp.ypp"
{
                    lc_map.orient = (map_section_type) yyvsp[0].i;
                ;
    break;}
case 44:
#line 238 "levcomp.ypp"
{ yyval.i = MAP_ENCOMPASS; ;
    break;}
case 45:
#line 239 "levcomp.ypp"
{ yyval.i = MAP_NORTH; ;
    break;}
case 46:
#line 240 "levcomp.ypp"
{ yyval.i = MAP_EAST; ;
    break;}
case 47:
#line 241 "levcomp.ypp"
{ yyval.i = MAP_SOUTH; ;
    break;}
case 48:
#line 242 "levcomp.ypp"
{ yyval.i = MAP_WEST; ;
    break;}
case 49:
#line 243 "levcomp.ypp"
{ yyval.i = MAP_NORTHEAST; ;
    break;}
case 50:
#line 244 "levcomp.ypp"
{ yyval.i = MAP_SOUTHEAST; ;
    break;}
case 51:
#line 245 "levcomp.ypp"
{ yyval.i = MAP_SOUTHWEST; ;
    break;}
case 52:
#line 246 "levcomp.ypp"
{ yyval.i = MAP_NORTHWEST; ;
    break;}
case 53:
#line 247 "levcomp.ypp"
{ yyval.i = MAP_FLOAT; ;
    break;}
case 54:
#line 250 "levcomp.ypp"
{;
    break;}
case 56:
#line 255 "levcomp.ypp"
{
                    switch (yyvsp[-1].i) {
                    case NO_HMIRROR:
                        lc_map.flags &= ~MAPF_MIRROR_HORIZONTAL;
                        break;
                    case NO_VMIRROR:
                        lc_map.flags &= ~MAPF_MIRROR_VERTICAL;
                        break;
                    case NO_ROTATE:
                        lc_map.flags &= ~MAPF_ROTATE;
                        break;
                    }
                ;
    break;}
case 57:
#line 270 "levcomp.ypp"
{ yyval.i = NO_HMIRROR; ;
    break;}
case 58:
#line 271 "levcomp.ypp"
{ yyval.i = NO_VMIRROR; ;
    break;}
case 59:
#line 272 "levcomp.ypp"
{ yyval.i = NO_ROTATE;  ;
    break;}
case 63:
#line 283 "levcomp.ypp"
{
                    lc_map.map.add_line(yyvsp[0].text);
                ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 543 "/usr/share/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;

 yyacceptlab:
  /* YYACCEPT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 0;

 yyabortlab:
  /* YYABORT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 1;
}
#line 288 "levcomp.ypp"

