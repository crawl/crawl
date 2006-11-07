
/*  A Bison parser, made from levcomp.ypp
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	DEFAULT_DEPTH	257
#define	SYMBOL	258
#define	TAGS	259
#define	NAME	260
#define	DEPTH	261
#define	ORIENT	262
#define	PLACE	263
#define	CHANCE	264
#define	FLAGS	265
#define	MONS	266
#define	ENCOMPASS	267
#define	NORTH	268
#define	EAST	269
#define	SOUTH	270
#define	WEST	271
#define	NORTH_DIS	272
#define	NORTHEAST	273
#define	SOUTHEAST	274
#define	SOUTHWEST	275
#define	NORTHWEST	276
#define	BAD_CHARACTER	277
#define	NO_HMIRROR	278
#define	NO_VMIRROR	279
#define	PANDEMONIC	280
#define	DASH	281
#define	COMMA	282
#define	INTEGER	283
#define	STRING	284
#define	MAP_LINE	285
#define	MONSTER_NAME	286

#line 1 "levcomp.ypp"


#include "levcomp.h"

int yylex();

extern int yylineno;

void yyerror(const char *e)
{
    fprintf(stderr, "Error on line %d: %s\n", yylineno, e);

    // If an error occurs, delete the file we're supposed to be writing to
    // so that life is easier for makefile writers.
    if (outhandle && outfilename) {
        fclose(outhandle);
        outhandle = NULL;
    }
    
    if (outfilename) {
        unlink(outfilename);
        outfilename = NULL;
    }
}


#line 28 "levcomp.ypp"
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



#define	YYFINAL		70
#define	YYFLAG		-32768
#define	YYNTBASE	33

#define YYTRANSLATE(x) ((unsigned)(x) <= 286 ? yytranslate[x] : 60)

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
    27,    28,    29,    30,    31,    32
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     3,     4,     7,     9,    12,    13,    16,    21,    24,
    25,    28,    30,    32,    34,    36,    38,    40,    42,    44,
    47,    48,    51,    53,    56,    58,    61,    65,    67,    69,
    72,    74,    77,    81,    83,    86,    88,    91,    93,    95,
    97,    99,   101,   103,   105,   107,   109,   111,   114,   115,
   118,   120,   122,   124,   126,   129
};

static const short yyrhs[] = {    34,
    37,     0,     0,    34,    35,     0,    36,     0,     3,    50,
     0,     0,    38,    37,     0,    39,    40,    57,    40,     0,
     6,    30,     0,     0,    41,    40,     0,    48,     0,    49,
     0,    51,     0,    52,     0,    54,     0,    45,     0,    44,
     0,    42,     0,     5,    43,     0,     0,    30,    43,     0,
     4,     0,     4,    30,     0,    12,     0,    12,    46,     0,
    47,    28,    46,     0,    47,     0,    32,     0,     9,    30,
     0,     7,     0,     7,    50,     0,    29,    27,    29,     0,
    29,     0,    10,    29,     0,     8,     0,     8,    53,     0,
    13,     0,    14,     0,    15,     0,    16,     0,    17,     0,
    19,     0,    20,     0,    21,     0,    22,     0,    18,     0,
    11,    55,     0,     0,    56,    55,     0,    24,     0,    25,
     0,    58,     0,    59,     0,    59,    58,     0,    31,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
    54,    63,    64,    67,    70,    76,    77,    80,   100,   108,
   109,   112,   113,   114,   115,   116,   117,   118,   119,   122,
   125,   126,   134,   135,   141,   142,   145,   146,   149,   155,
   161,   162,   168,   173,   179,   185,   186,   192,   193,   194,
   195,   196,   197,   198,   199,   200,   201,   204,   207,   208,
   221,   222,   225,   228,   229,   232
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","DEFAULT_DEPTH",
"SYMBOL","TAGS","NAME","DEPTH","ORIENT","PLACE","CHANCE","FLAGS","MONS","ENCOMPASS",
"NORTH","EAST","SOUTH","WEST","NORTH_DIS","NORTHEAST","SOUTHEAST","SOUTHWEST",
"NORTHWEST","BAD_CHARACTER","NO_HMIRROR","NO_VMIRROR","PANDEMONIC","DASH","COMMA",
"INTEGER","STRING","MAP_LINE","MONSTER_NAME","file","defs","def","defdepth",
"levels","level","name","metalines","metaline","tags","tagstrings","symbol",
"mons","mnames","mname","place","depth","depth_range","chance","orientation",
"orient_name","flags","flagnames","flagname","map_def","map_lines","map_line", NULL
};
#endif

static const short yyr1[] = {     0,
    33,    34,    34,    35,    36,    37,    37,    38,    39,    40,
    40,    41,    41,    41,    41,    41,    41,    41,    41,    42,
    43,    43,    44,    44,    45,    45,    46,    46,    47,    48,
    49,    49,    50,    50,    51,    52,    52,    53,    53,    53,
    53,    53,    53,    53,    53,    53,    53,    54,    55,    55,
    56,    56,    57,    58,    58,    59
};

static const short yyr2[] = {     0,
     2,     0,     2,     1,     2,     0,     2,     4,     2,     0,
     2,     1,     1,     1,     1,     1,     1,     1,     1,     2,
     0,     2,     1,     2,     1,     2,     3,     1,     1,     2,
     1,     2,     3,     1,     2,     1,     2,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     2,     0,     2,
     1,     1,     1,     1,     2,     1
};

static const short yydefact[] = {     2,
     6,     0,     0,     3,     4,     1,     6,    10,    34,     5,
     9,     7,    23,    21,    31,    36,     0,     0,    49,    25,
     0,    10,    19,    18,    17,    12,    13,    14,    15,    16,
     0,    24,    21,    20,    32,    38,    39,    40,    41,    42,
    47,    43,    44,    45,    46,    37,    30,    35,    51,    52,
    48,    49,    29,    26,    28,    56,    10,    53,    54,    11,
    33,    22,    50,     0,     8,    55,    27,     0,     0,     0
};

static const short yydefgoto[] = {    68,
     1,     4,     5,     6,     7,     8,    21,    22,    23,    34,
    24,    25,    54,    55,    26,    27,    10,    28,    29,    46,
    30,    51,    52,    57,    58,    59
};

static const short yypact[] = {-32768,
    17,   -16,    -6,-32768,-32768,-32768,    19,     7,    -1,-32768,
-32768,-32768,    -2,     0,   -16,   -12,     1,     3,    -3,    -5,
     2,     7,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
     5,-32768,     0,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,    -3,-32768,-32768,     8,-32768,     7,-32768,     2,-32768,
-32768,-32768,-32768,    -5,-32768,-32768,-32768,    29,    37,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,    31,-32768,-32768,   -22,-32768,-32768,     6,
-32768,-32768,   -24,-32768,-32768,-32768,    26,-32768,-32768,-32768,
-32768,   -10,-32768,-32768,   -15,-32768
};


#define	YYLAST		44


static const short yytable[] = {    60,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    13,    14,     9,    15,    16,    17,    18,    19,    20,     2,
    49,    50,     3,    11,     3,    31,    53,    32,    69,    33,
    47,    48,    56,    61,    65,    64,    70,    12,    62,    67,
    35,    63,     0,    66
};

static const short yycheck[] = {    22,
    13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
     4,     5,    29,     7,     8,     9,    10,    11,    12,     3,
    24,    25,     6,    30,     6,    27,    32,    30,     0,    30,
    30,    29,    31,    29,    57,    28,     0,     7,    33,    64,
    15,    52,    -1,    59
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
#line 55 "levcomp.ypp"
{
                    if (outhandle && outfilename) {
                        fclose(outhandle);
                        outhandle   = NULL;
                    }
                ;
    break;}
case 5:
#line 71 "levcomp.ypp"
{
                    default_depth = range;
                ;
    break;}
case 8:
#line 81 "levcomp.ypp"
{
                    if (!outhandle) {
                        if (outfilename) {
                            if (!(outhandle = 
                                    fopen(outfilename, 
                                          write_append? "a" :"w")))
                                outfilename = NULL;
                        }
                        if (!outhandle)
                            outhandle = stdout;
                    }
                    if (!autowarned) {
                        fprintf(outhandle, "%s", autogenheader);
                        autowarned = true;
                    }
                    fprintf(outhandle, "%s", map.get_initialiser().c_str());
                ;
    break;}
case 9:
#line 101 "levcomp.ypp"
{
                    map.init();
                    map.depth = default_depth;
                    map.name = yyvsp[0].text;
                ;
    break;}
case 20:
#line 122 "levcomp.ypp"
{;
    break;}
case 22:
#line 127 "levcomp.ypp"
{
                    map.tags += " ";
                    map.tags += yyvsp[-1].text;
                    map.tags += " ";
                ;
    break;}
case 23:
#line 134 "levcomp.ypp"
{;
    break;}
case 24:
#line 136 "levcomp.ypp"
{
                    map.random_symbols = yyvsp[0].text;
                ;
    break;}
case 25:
#line 141 "levcomp.ypp"
{;
    break;}
case 26:
#line 142 "levcomp.ypp"
{;
    break;}
case 29:
#line 150 "levcomp.ypp"
{
                    map.mons.add_mons(yyvsp[0].text);
                ;
    break;}
case 30:
#line 156 "levcomp.ypp"
{
                    map.place = yyvsp[0].text;
                ;
    break;}
case 31:
#line 161 "levcomp.ypp"
{;
    break;}
case 32:
#line 163 "levcomp.ypp"
{
                    map.depth = range;
                ;
    break;}
case 33:
#line 169 "levcomp.ypp"
{
                    range.set(yyvsp[-2].i, yyvsp[0].i);
                ;
    break;}
case 34:
#line 174 "levcomp.ypp"
{
                    range.set(yyvsp[0].i);
                ;
    break;}
case 35:
#line 180 "levcomp.ypp"
{
                    map.chance = yyvsp[0].i;
                ;
    break;}
case 36:
#line 185 "levcomp.ypp"
{;
    break;}
case 37:
#line 187 "levcomp.ypp"
{
                    map.orient = (map_section_type) yyvsp[0].i;
                ;
    break;}
case 38:
#line 192 "levcomp.ypp"
{ yyval.i = MAP_ENCOMPASS; ;
    break;}
case 39:
#line 193 "levcomp.ypp"
{ yyval.i = MAP_NORTH; ;
    break;}
case 40:
#line 194 "levcomp.ypp"
{ yyval.i = MAP_EAST; ;
    break;}
case 41:
#line 195 "levcomp.ypp"
{ yyval.i = MAP_SOUTH; ;
    break;}
case 42:
#line 196 "levcomp.ypp"
{ yyval.i = MAP_WEST; ;
    break;}
case 43:
#line 197 "levcomp.ypp"
{ yyval.i = MAP_NORTHEAST; ;
    break;}
case 44:
#line 198 "levcomp.ypp"
{ yyval.i = MAP_SOUTHEAST; ;
    break;}
case 45:
#line 199 "levcomp.ypp"
{ yyval.i = MAP_SOUTHWEST; ;
    break;}
case 46:
#line 200 "levcomp.ypp"
{ yyval.i = MAP_NORTHWEST; ;
    break;}
case 47:
#line 201 "levcomp.ypp"
{ yyval.i = MAP_NORTH_DIS; ;
    break;}
case 48:
#line 204 "levcomp.ypp"
{;
    break;}
case 50:
#line 209 "levcomp.ypp"
{
                    switch (yyvsp[-1].i) {
                    case NO_HMIRROR:
                        map.flags &= ~MAPF_MIRROR_HORIZONTAL;
                        break;
                    case NO_VMIRROR:
                        map.flags &= ~MAPF_MIRROR_VERTICAL;
                        break;
                    }
                ;
    break;}
case 51:
#line 221 "levcomp.ypp"
{ yyval.i = NO_HMIRROR; ;
    break;}
case 52:
#line 222 "levcomp.ypp"
{ yyval.i = NO_VMIRROR; ;
    break;}
case 56:
#line 233 "levcomp.ypp"
{
                    map.map.add_line(yyvsp[0].text);
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
#line 238 "levcomp.ypp"

