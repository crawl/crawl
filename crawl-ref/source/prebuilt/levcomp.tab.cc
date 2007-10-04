/* A Bison parser, made by GNU Bison 2.0.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     DEFAULT_DEPTH = 258,
     SHUFFLE = 259,
     SUBST = 260,
     TAGS = 261,
     KFEAT = 262,
     KITEM = 263,
     KMONS = 264,
     KMASK = 265,
     NAME = 266,
     DEPTH = 267,
     ORIENT = 268,
     PLACE = 269,
     CHANCE = 270,
     MONS = 271,
     ITEM = 272,
     MARKER = 273,
     COLOUR = 274,
     PRELUDE = 275,
     MAIN = 276,
     VALIDATE = 277,
     VETO = 278,
     NSUBST = 279,
     WELCOME = 280,
     LFLAGS = 281,
     BFLAGS = 282,
     FLOORCOL = 283,
     ROCKCOL = 284,
     COMMA = 285,
     INTEGER = 286,
     CHARACTER = 287,
     STRING = 288,
     MAP_LINE = 289,
     MONSTER_NAME = 290,
     ITEM_INFO = 291,
     LUA_LINE = 292
   };
#endif
#define DEFAULT_DEPTH 258
#define SHUFFLE 259
#define SUBST 260
#define TAGS 261
#define KFEAT 262
#define KITEM 263
#define KMONS 264
#define KMASK 265
#define NAME 266
#define DEPTH 267
#define ORIENT 268
#define PLACE 269
#define CHANCE 270
#define MONS 271
#define ITEM 272
#define MARKER 273
#define COLOUR 274
#define PRELUDE 275
#define MAIN 276
#define VALIDATE 277
#define VETO 278
#define NSUBST 279
#define WELCOME 280
#define LFLAGS 281
#define BFLAGS 282
#define FLOORCOL 283
#define ROCKCOL 284
#define COMMA 285
#define INTEGER 286
#define CHARACTER 287
#define STRING 288
#define MAP_LINE 289
#define MONSTER_NAME 290
#define ITEM_INFO 291
#define LUA_LINE 292




/* Copy the first part of user declarations.  */
#line 1 "levcomp.ypp"


#include "AppHdr.h"
#include "clua.h"
#include "libutil.h"
#include "luadgn.h"
#include "mapdef.h"
#include "maps.h"
#include "stuff.h"
#include <map>

#define YYERROR_VERBOSE 1

int yylex();

extern int yylineno;

static bool start_marker_segment = false;

void yyerror(const char *e)
{
    if (strstr(e, lc_desfile.c_str()) == e)
        fprintf(stderr, "%s\n", e);
    else
        fprintf(stderr, "%s:%d: %s\n", lc_desfile.c_str(), yylineno, e);
    // Bail bail bail.
    end(1);
}

level_range set_range(const char *s, int start, int end)
{
    try
    {
        lc_range.set(s, start, end);
    }
    catch (const std::string &err)
    {
        yyerror(err.c_str());
    }
    return (lc_range);
}



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 46 "levcomp.ypp"
typedef union YYSTYPE {
    int i;
    const char *text;
    raw_range range;
} YYSTYPE;
/* Line 185 of yacc.c.  */
#line 200 "levcomp.tab.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 213 of yacc.c.  */
#line 212 "levcomp.tab.c"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

# ifndef YYFREE
#  define YYFREE free
# endif
# ifndef YYMALLOC
#  define YYMALLOC malloc
# endif

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   else
#    define YYSTACK_ALLOC alloca
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short int yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short int) + sizeof (YYSTYPE))			\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short int yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   75

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  38
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  69
/* YYNRULES -- Number of rules. */
#define YYNRULES  123
/* YYNRULES -- Number of states. */
#define YYNSTATES  141

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   292

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     5,     6,     9,    11,    13,    15,    17,
      19,    22,    25,    26,    29,    31,    33,    36,    38,    40,
      42,    44,    46,    48,    50,    52,    54,    56,    58,    60,
      62,    64,    66,    68,    70,    72,    74,    76,    78,    80,
      82,    84,    86,    88,    91,    92,    95,    97,   100,   101,
     104,   106,   109,   110,   113,   115,   118,   119,   122,   124,
     127,   128,   131,   133,   135,   138,   140,   143,   145,   148,
     150,   153,   156,   158,   162,   164,   167,   168,   171,   173,
     176,   179,   180,   184,   186,   187,   190,   192,   195,   197,
     200,   202,   205,   207,   211,   213,   216,   218,   222,   224,
     227,   229,   233,   235,   237,   240,   244,   246,   248,   250,
     253,   257,   259,   261,   264,   266,   269,   272,   274,   277,
     280,   282,   285,   287
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      39,     0,    -1,    40,    -1,    -1,    40,    41,    -1,    43,
      -1,    45,    -1,    32,    -1,    44,    -1,    50,    -1,     3,
      33,    -1,    48,    46,    -1,    -1,    46,    47,    -1,    49,
      -1,   104,    -1,    11,    33,    -1,    99,    -1,   100,    -1,
     101,    -1,   102,    -1,   103,    -1,    96,    -1,    93,    -1,
      77,    -1,    90,    -1,    87,    -1,    82,    -1,    83,    -1,
      84,    -1,    69,    -1,    72,    -1,    75,    -1,    76,    -1,
      65,    -1,    67,    -1,    66,    -1,    68,    -1,    53,    -1,
      62,    -1,    56,    -1,    59,    -1,    42,    -1,    21,    51,
      -1,    -1,    51,    52,    -1,    37,    -1,    21,    54,    -1,
      -1,    54,    55,    -1,    37,    -1,    22,    57,    -1,    -1,
      57,    58,    -1,    37,    -1,    23,    60,    -1,    -1,    60,
      61,    -1,    37,    -1,    20,    63,    -1,    -1,    63,    64,
      -1,    37,    -1,     7,    -1,     7,    33,    -1,     9,    -1,
       9,    33,    -1,     8,    -1,     8,    33,    -1,    10,    -1,
      10,    33,    -1,     4,    70,    -1,    71,    -1,    70,    30,
      71,    -1,    36,    -1,     6,    73,    -1,    -1,    73,    74,
      -1,    33,    -1,    26,    33,    -1,    27,    33,    -1,    -1,
      18,    78,    79,    -1,    80,    -1,    -1,    80,    81,    -1,
      33,    -1,    19,    85,    -1,    28,    -1,    28,    33,    -1,
      29,    -1,    29,    33,    -1,    86,    -1,    85,    30,    86,
      -1,    36,    -1,    24,    88,    -1,    89,    -1,    88,    30,
      89,    -1,    36,    -1,     5,    91,    -1,    92,    -1,    92,
      30,    91,    -1,    36,    -1,    17,    -1,    17,    94,    -1,
      94,    30,    95,    -1,    95,    -1,    36,    -1,    16,    -1,
      16,    97,    -1,    98,    30,    97,    -1,    98,    -1,    35,
      -1,    14,    33,    -1,    12,    -1,    12,    33,    -1,    15,
      31,    -1,    13,    -1,    13,    33,    -1,    25,    33,    -1,
     105,    -1,   105,   106,    -1,   106,    -1,    34,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,    67,    67,    70,    71,    74,    75,    78,    84,    85,
      88,    98,   120,   121,   124,   125,   128,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   180,   182,   183,   186,   191,   193,   194,
     197,   202,   204,   205,   208,   213,   215,   216,   219,   224,
     226,   227,   230,   235,   236,   244,   245,   253,   254,   262,
     263,   271,   274,   275,   278,   286,   289,   290,   293,   302,
     311,   321,   320,   331,   333,   334,   337,   349,   352,   353,
     361,   362,   370,   371,   374,   383,   386,   387,   390,   399,
     402,   403,   406,   415,   416,   419,   420,   423,   431,   432,
     435,   436,   439,   448,   457,   458,   467,   475,   476,   485,
     494,   497,   498,   501
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "DEFAULT_DEPTH", "SHUFFLE", "SUBST",
  "TAGS", "KFEAT", "KITEM", "KMONS", "KMASK", "NAME", "DEPTH", "ORIENT",
  "PLACE", "CHANCE", "MONS", "ITEM", "MARKER", "COLOUR", "PRELUDE", "MAIN",
  "VALIDATE", "VETO", "NSUBST", "WELCOME", "LFLAGS", "BFLAGS", "FLOORCOL",
  "ROCKCOL", "COMMA", "INTEGER", "CHARACTER", "STRING", "MAP_LINE",
  "MONSTER_NAME", "ITEM_INFO", "LUA_LINE", "$accept", "file",
  "definitions", "definition", "error_seq", "def", "defdepth", "level",
  "map_specs", "map_spec", "name", "metaline", "global_lua",
  "global_lua_lines", "global_lua_line", "main_lua", "main_lua_lines",
  "main_lua_line", "validate_lua", "validate_lua_lines",
  "validate_lua_line", "veto_lua", "veto_lua_lines", "veto_lua_line",
  "prelude_lua", "prelude_lua_lines", "prelude_lua_line", "kfeat", "kmons",
  "kitem", "kmask", "shuffle", "shuffle_specifiers", "shuffle_spec",
  "tags", "tagstrings", "tagstring", "lflags", "bflags", "marker", "@1",
  "marker_spec", "mspec_segments", "mspec_segment", "colour", "floorcol",
  "rockcol", "colour_specifiers", "colour_specifier", "nsubst",
  "nsubst_specifiers", "nsubst_spec", "subst", "subst_specifiers",
  "subst_spec", "items", "item_specifiers", "item_specifier", "mons",
  "mnames", "mname", "place", "depth", "chance", "orientation", "welcome",
  "map_def", "map_lines", "map_line", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short int yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    38,    39,    40,    40,    41,    41,    42,    43,    43,
      44,    45,    46,    46,    47,    47,    48,    49,    49,    49,
      49,    49,    49,    49,    49,    49,    49,    49,    49,    49,
      49,    49,    49,    49,    49,    49,    49,    49,    49,    49,
      49,    49,    49,    50,    51,    51,    52,    53,    54,    54,
      55,    56,    57,    57,    58,    59,    60,    60,    61,    62,
      63,    63,    64,    65,    65,    66,    66,    67,    67,    68,
      68,    69,    70,    70,    71,    72,    73,    73,    74,    75,
      76,    78,    77,    79,    80,    80,    81,    82,    83,    83,
      84,    84,    85,    85,    86,    87,    88,    88,    89,    90,
      91,    91,    92,    93,    93,    94,    94,    95,    96,    96,
      97,    97,    98,    99,   100,   100,   101,   102,   102,   103,
     104,   105,   105,   106
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     0,     2,     1,     1,     1,     1,     1,
       2,     2,     0,     2,     1,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     0,     2,     1,     2,     0,     2,
       1,     2,     0,     2,     1,     2,     0,     2,     1,     2,
       0,     2,     1,     1,     2,     1,     2,     1,     2,     1,
       2,     2,     1,     3,     1,     2,     0,     2,     1,     2,
       2,     0,     3,     1,     0,     2,     1,     2,     1,     2,
       1,     2,     1,     3,     1,     2,     1,     3,     1,     2,
       1,     3,     1,     1,     2,     3,     1,     1,     1,     2,
       3,     1,     1,     2,     1,     2,     2,     1,     2,     2,
       1,     2,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       3,     0,     2,     1,     0,     0,    44,     4,     5,     8,
       6,    12,     9,    10,    16,    43,    11,    46,    45,     0,
       0,    76,    63,    67,    65,    69,   114,   117,     0,     0,
     108,   103,    81,     0,    60,    48,    52,    56,     0,     0,
       0,     0,    88,    90,     7,   123,    42,    13,    14,    38,
      40,    41,    39,    34,    36,    35,    37,    30,    31,    32,
      33,    24,    27,    28,    29,    26,    25,    23,    22,    17,
      18,    19,    20,    21,    15,   120,   122,    74,    71,    72,
     102,    99,   100,    75,    64,    68,    66,    70,   115,   118,
     113,   116,   112,   109,   111,   107,   104,   106,    84,    94,
      87,    92,    59,    47,    51,    55,    98,    95,    96,   119,
      79,    80,    89,    91,   121,     0,     0,    78,    77,     0,
       0,    82,    83,     0,    62,    61,    50,    49,    54,    53,
      58,    57,     0,    73,   101,   110,   105,    86,    85,    93,
      97
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,     1,     2,     7,    46,     8,     9,    10,    16,    47,
      11,    48,    12,    15,    18,    49,   103,   127,    50,   104,
     129,    51,   105,   131,    52,   102,   125,    53,    54,    55,
      56,    57,    78,    79,    58,    83,   118,    59,    60,    61,
      98,   121,   122,   138,    62,    63,    64,   100,   101,    65,
     107,   108,    66,    81,    82,    67,    96,    97,    68,    93,
      94,    69,    70,    71,    72,    73,    74,    75,    76
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -76
static const yysigned_char yypact[] =
{
     -76,     7,    23,   -76,    -6,    -2,   -76,   -76,   -76,   -76,
     -76,   -76,   -76,   -76,   -76,    -8,    -4,   -76,   -76,    -3,
      -1,   -76,     3,     4,     5,     6,     8,     9,    10,     1,
      11,    12,   -76,    13,   -76,   -76,   -76,   -76,    14,    18,
      19,    20,    21,    22,   -76,   -76,   -76,   -76,   -76,   -76,
     -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,
     -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,
     -76,   -76,   -76,   -76,   -76,    24,   -76,   -76,    15,   -76,
     -76,   -76,    17,    26,   -76,   -76,   -76,   -76,   -76,   -76,
     -76,   -76,   -76,   -76,    27,   -76,    30,   -76,   -76,   -76,
      31,   -76,    25,    28,    29,    32,   -76,    33,   -76,   -76,
     -76,   -76,   -76,   -76,   -76,    -3,    -1,   -76,   -76,    11,
      12,   -76,    34,    13,   -76,   -76,   -76,   -76,   -76,   -76,
     -76,   -76,    14,   -76,   -76,   -76,   -76,   -76,   -76,   -76,
     -76
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
     -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,
     -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,
     -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,
     -76,   -76,   -76,   -75,   -76,   -76,   -76,   -76,   -76,   -76,
     -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,   -67,   -76,
     -76,   -68,   -76,   -48,   -76,   -76,   -76,   -50,   -76,   -47,
     -76,   -76,   -76,   -76,   -76,   -76,   -76,   -76,     0
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
      19,    20,    21,    22,    23,    24,    25,     3,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,     4,    13,    44,    17,
      45,    14,    91,    77,     5,    80,    84,    85,    86,    87,
     133,    88,    89,    90,     6,   115,    92,   116,    95,    99,
     106,   109,   110,   111,   112,   113,   139,   119,    45,   117,
     120,   123,   124,   132,   140,   126,   128,   137,   134,   130,
     136,     0,   135,     0,     0,   114
};

static const short int yycheck[] =
{
       4,     5,     6,     7,     8,     9,    10,     0,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,     3,    33,    32,    37,
      34,    33,    31,    36,    11,    36,    33,    33,    33,    33,
     115,    33,    33,    33,    21,    30,    35,    30,    36,    36,
      36,    33,    33,    33,    33,    33,   123,    30,    34,    33,
      30,    30,    37,    30,   132,    37,    37,    33,   116,    37,
     120,    -1,   119,    -1,    -1,    75
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    39,    40,     0,     3,    11,    21,    41,    43,    44,
      45,    48,    50,    33,    33,    51,    46,    37,    52,     4,
       5,     6,     7,     8,     9,    10,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    32,    34,    42,    47,    49,    53,
      56,    59,    62,    65,    66,    67,    68,    69,    72,    75,
      76,    77,    82,    83,    84,    87,    90,    93,    96,    99,
     100,   101,   102,   103,   104,   105,   106,    36,    70,    71,
      36,    91,    92,    73,    33,    33,    33,    33,    33,    33,
      33,    31,    35,    97,    98,    36,    94,    95,    78,    36,
      85,    86,    63,    54,    57,    60,    36,    88,    89,    33,
      33,    33,    33,    33,   106,    30,    30,    33,    74,    30,
      30,    79,    80,    30,    37,    64,    37,    55,    37,    58,
      37,    61,    30,    71,    91,    97,    95,    33,    81,    86,
      89
};

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (N)								\
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (0)
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
              (Loc).first_line, (Loc).first_column,	\
              (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Type, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short int *bottom, short int *top)
#else
static void
yy_stack_print (bottom, top)
    short int *bottom;
    short int *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);


# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  register short int *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;


  yyvsp[0] = yylval;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short int *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short int *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a look-ahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to look-ahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 67 "levcomp.ypp"
    { }
    break;

  case 3:
#line 70 "levcomp.ypp"
    {}
    break;

  case 4:
#line 71 "levcomp.ypp"
    {}
    break;

  case 5:
#line 74 "levcomp.ypp"
    {}
    break;

  case 6:
#line 75 "levcomp.ypp"
    {}
    break;

  case 7:
#line 79 "levcomp.ypp"
    {
                    yyerror("Unexpected character sequence.");
                }
    break;

  case 8:
#line 84 "levcomp.ypp"
    {}
    break;

  case 9:
#line 85 "levcomp.ypp"
    {}
    break;

  case 10:
#line 89 "levcomp.ypp"
    {
                    dgn_reset_default_depth();
                    std::string err = dgn_set_default_depth((yyvsp[0].text));
                    if (!err.empty())
                        yyerror(make_stringf("Bad default-depth: %s (%s)",
                                (yyvsp[0].text), err.c_str()).c_str());
                }
    break;

  case 11:
#line 99 "levcomp.ypp"
    {
                    lc_map.set_file(lc_desfile);
                    
                    if (lc_run_global_prelude && !lc_global_prelude.empty())
                    {
                        lc_global_prelude.set_file(lc_desfile);
                        lc_run_global_prelude = false;
                        if (lc_global_prelude.load_call(dlua, NULL))
                            yyerror( lc_global_prelude.orig_error().c_str() );
                    }

                    std::string err = lc_map.validate_map_def();
                    if (!err.empty())
                        yyerror(err.c_str());
                    if (!lc_map.has_depth() && !lc_default_depths.empty())
                        lc_map.add_depths(lc_default_depths.begin(),
                                          lc_default_depths.end());
                    add_parsed_map(lc_map);
                }
    break;

  case 12:
#line 120 "levcomp.ypp"
    { }
    break;

  case 13:
#line 121 "levcomp.ypp"
    { }
    break;

  case 14:
#line 124 "levcomp.ypp"
    { }
    break;

  case 15:
#line 125 "levcomp.ypp"
    { }
    break;

  case 16:
#line 129 "levcomp.ypp"
    {
                    lc_map.init();
                    lc_map.name = (yyvsp[0].text);

                    map_load_info_t::const_iterator i = 
                        lc_loaded_maps.find((yyvsp[0].text));

                    if (i != lc_loaded_maps.end())
                    {
                        yyerror(
                            make_stringf(
                                "Map named '%s' already loaded at %s:%d",
                                (yyvsp[0].text),
                                i->second.filename.c_str(),
                                i->second.lineno).c_str() );
                    }

                    lc_map.place_loaded_from =
                        map_file_place(lc_desfile, yylineno);
                    lc_loaded_maps[(yyvsp[0].text)] = lc_map.place_loaded_from;
                }
    break;

  case 42:
#line 177 "levcomp.ypp"
    {}
    break;

  case 43:
#line 180 "levcomp.ypp"
    { }
    break;

  case 44:
#line 182 "levcomp.ypp"
    { }
    break;

  case 45:
#line 183 "levcomp.ypp"
    { }
    break;

  case 46:
#line 187 "levcomp.ypp"
    {
                    lc_global_prelude.add(yylineno, (yyvsp[0].text));
                }
    break;

  case 47:
#line 191 "levcomp.ypp"
    { }
    break;

  case 48:
#line 193 "levcomp.ypp"
    { }
    break;

  case 49:
#line 194 "levcomp.ypp"
    { }
    break;

  case 50:
#line 198 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, (yyvsp[0].text));
                }
    break;

  case 51:
#line 202 "levcomp.ypp"
    { }
    break;

  case 52:
#line 204 "levcomp.ypp"
    { }
    break;

  case 53:
#line 205 "levcomp.ypp"
    { }
    break;

  case 54:
#line 209 "levcomp.ypp"
    {
                    lc_map.validate.add(yylineno, (yyvsp[0].text));
                }
    break;

  case 55:
#line 213 "levcomp.ypp"
    { }
    break;

  case 56:
#line 215 "levcomp.ypp"
    { }
    break;

  case 57:
#line 216 "levcomp.ypp"
    { }
    break;

  case 58:
#line 220 "levcomp.ypp"
    {
                    lc_map.veto.add(yylineno, (yyvsp[0].text));
                }
    break;

  case 59:
#line 224 "levcomp.ypp"
    { }
    break;

  case 60:
#line 226 "levcomp.ypp"
    { }
    break;

  case 61:
#line 227 "levcomp.ypp"
    { }
    break;

  case 62:
#line 231 "levcomp.ypp"
    {
                    lc_map.prelude.add(yylineno, (yyvsp[0].text));
                }
    break;

  case 63:
#line 235 "levcomp.ypp"
    { }
    break;

  case 64:
#line 237 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kfeat(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 65:
#line 244 "levcomp.ypp"
    { }
    break;

  case 66:
#line 246 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kmons(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 67:
#line 253 "levcomp.ypp"
    { }
    break;

  case 68:
#line 255 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kitem(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 69:
#line 262 "levcomp.ypp"
    { }
    break;

  case 70:
#line 264 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kmask(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 71:
#line 271 "levcomp.ypp"
    {}
    break;

  case 74:
#line 279 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("shuffle(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 75:
#line 286 "levcomp.ypp"
    {}
    break;

  case 78:
#line 294 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("tags(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 79:
#line 303 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lflags(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 80:
#line 312 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("bflags(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 81:
#line 321 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, "marker(");
                    start_marker_segment = true;
                }
    break;

  case 82:
#line 326 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, ")");
                }
    break;

  case 86:
#line 338 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf(
                            "%s\"%s\"",
                            start_marker_segment? "" : " .. ",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                    start_marker_segment = false;
                }
    break;

  case 87:
#line 349 "levcomp.ypp"
    { }
    break;

  case 88:
#line 352 "levcomp.ypp"
    { }
    break;

  case 89:
#line 354 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("floor_colour(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 90:
#line 361 "levcomp.ypp"
    { }
    break;

  case 91:
#line 363 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("rock_colour(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 92:
#line 370 "levcomp.ypp"
    { }
    break;

  case 93:
#line 371 "levcomp.ypp"
    { }
    break;

  case 94:
#line 375 "levcomp.ypp"
    {
                      lc_map.main.add(
                          yylineno,
                          make_stringf("colour(\"%s\")",
                              quote_lua_string((yyvsp[0].text)).c_str()));
                  }
    break;

  case 95:
#line 383 "levcomp.ypp"
    { }
    break;

  case 96:
#line 386 "levcomp.ypp"
    { }
    break;

  case 97:
#line 387 "levcomp.ypp"
    { }
    break;

  case 98:
#line 391 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("nsubst(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 99:
#line 399 "levcomp.ypp"
    { }
    break;

  case 102:
#line 407 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("subst(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 103:
#line 415 "levcomp.ypp"
    {}
    break;

  case 104:
#line 416 "levcomp.ypp"
    {}
    break;

  case 107:
#line 424 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("item(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 108:
#line 431 "levcomp.ypp"
    {}
    break;

  case 109:
#line 432 "levcomp.ypp"
    {}
    break;

  case 112:
#line 440 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("mons(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 113:
#line 449 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("place(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 114:
#line 457 "levcomp.ypp"
    {}
    break;

  case 115:
#line 459 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("depth(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 116:
#line 468 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("chance(\"%d\")", (yyvsp[0].i)));
                }
    break;

  case 117:
#line 475 "levcomp.ypp"
    {}
    break;

  case 118:
#line 477 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("orient(\"%s\")", 
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 119:
#line 486 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("welcome(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 123:
#line 502 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("map(\"%s\")", 
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;


    }

/* Line 1037 of yacc.c.  */
#line 1853 "levcomp.tab.c"

  yyvsp -= yylen;
  yyssp -= yylen;


  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  const char* yyprefix;
	  char *yymsg;
	  int yyx;

	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  int yyxbegin = yyn < 0 ? -yyn : 0;

	  /* Stay within bounds of both yycheck and yytname.  */
	  int yychecklim = YYLAST - yyn;
	  int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
	  int yycount = 0;

	  yyprefix = ", expecting ";
	  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      {
		yysize += yystrlen (yyprefix) + yystrlen (yytname [yyx]);
		yycount += 1;
		if (yycount == 5)
		  {
		    yysize = 0;
		    break;
		  }
	      }
	  yysize += (sizeof ("syntax error, unexpected ")
		     + yystrlen (yytname[yytype]));
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yyprefix = ", expecting ";
		  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			yyp = yystpcpy (yyp, yyprefix);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yyprefix = " or ";
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("syntax error");
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* If at end of input, pop the error token,
	     then the rest of the stack, then return failure.  */
	  if (yychar == YYEOF)
	     for (;;)
	       {

		 YYPOPSTACK;
		 if (yyssp == yyss)
		   YYABORT;
		 yydestruct ("Error: popping",
                             yystos[*yyssp], yyvsp);
	       }
        }
      else
	{
	  yydestruct ("Error: discarding", yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

#ifdef __GNUC__
  /* Pacify GCC when the user code never invokes YYERROR and the label
     yyerrorlab therefore never appears in user code.  */
  if (0)
     goto yyerrorlab;
#endif

yyvsp -= yylen;
  yyssp -= yylen;
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping", yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token. */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yydestruct ("Error: discarding lookahead",
              yytoken, &yylval);
  yychar = YYEMPTY;
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 510 "levcomp.ypp"


