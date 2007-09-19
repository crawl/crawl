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
     PRELUDE = 274,
     MAIN = 275,
     VALIDATE = 276,
     VETO = 277,
     NSUBST = 278,
     WELCOME = 279,
     LFLAGS = 280,
     BFLAGS = 281,
     COMMA = 282,
     INTEGER = 283,
     CHARACTER = 284,
     STRING = 285,
     MAP_LINE = 286,
     MONSTER_NAME = 287,
     ITEM_INFO = 288,
     LUA_LINE = 289
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
#define PRELUDE 274
#define MAIN 275
#define VALIDATE 276
#define VETO 277
#define NSUBST 278
#define WELCOME 279
#define LFLAGS 280
#define BFLAGS 281
#define COMMA 282
#define INTEGER 283
#define CHARACTER 284
#define STRING 285
#define MAP_LINE 286
#define MONSTER_NAME 287
#define ITEM_INFO 288
#define LUA_LINE 289




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
#line 194 "levcomp.tab.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 213 of yacc.c.  */
#line 206 "levcomp.tab.c"

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
#define YYLAST   64

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  35
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  64
/* YYNRULES -- Number of rules. */
#define YYNRULES  112
/* YYNRULES -- Number of states. */
#define YYNSTATES  128

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   289

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
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34
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
      82,    85,    86,    89,    91,    94,    95,    98,   100,   103,
     104,   107,   109,   112,   113,   116,   118,   121,   122,   125,
     127,   129,   132,   134,   137,   139,   142,   144,   147,   150,
     152,   156,   158,   161,   162,   165,   167,   170,   173,   174,
     178,   180,   181,   184,   186,   189,   191,   195,   197,   200,
     202,   206,   208,   210,   213,   217,   219,   221,   223,   226,
     230,   232,   234,   237,   239,   242,   245,   247,   250,   253,
     255,   258,   260
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      36,     0,    -1,    37,    -1,    -1,    37,    38,    -1,    40,
      -1,    42,    -1,    29,    -1,    41,    -1,    47,    -1,     3,
      30,    -1,    45,    43,    -1,    -1,    43,    44,    -1,    46,
      -1,    96,    -1,    11,    30,    -1,    91,    -1,    92,    -1,
      93,    -1,    94,    -1,    95,    -1,    88,    -1,    85,    -1,
      74,    -1,    82,    -1,    79,    -1,    66,    -1,    69,    -1,
      72,    -1,    73,    -1,    62,    -1,    64,    -1,    63,    -1,
      65,    -1,    50,    -1,    59,    -1,    53,    -1,    56,    -1,
      39,    -1,    20,    48,    -1,    -1,    48,    49,    -1,    34,
      -1,    20,    51,    -1,    -1,    51,    52,    -1,    34,    -1,
      21,    54,    -1,    -1,    54,    55,    -1,    34,    -1,    22,
      57,    -1,    -1,    57,    58,    -1,    34,    -1,    19,    60,
      -1,    -1,    60,    61,    -1,    34,    -1,     7,    -1,     7,
      30,    -1,     9,    -1,     9,    30,    -1,     8,    -1,     8,
      30,    -1,    10,    -1,    10,    30,    -1,     4,    67,    -1,
      68,    -1,    67,    27,    68,    -1,    33,    -1,     6,    70,
      -1,    -1,    70,    71,    -1,    30,    -1,    25,    30,    -1,
      26,    30,    -1,    -1,    18,    75,    76,    -1,    77,    -1,
      -1,    77,    78,    -1,    30,    -1,    23,    80,    -1,    81,
      -1,    80,    27,    81,    -1,    33,    -1,     5,    83,    -1,
      84,    -1,    84,    27,    83,    -1,    33,    -1,    17,    -1,
      17,    86,    -1,    86,    27,    87,    -1,    87,    -1,    33,
      -1,    16,    -1,    16,    89,    -1,    90,    27,    89,    -1,
      90,    -1,    32,    -1,    14,    30,    -1,    12,    -1,    12,
      30,    -1,    15,    28,    -1,    13,    -1,    13,    30,    -1,
      24,    30,    -1,    97,    -1,    97,    98,    -1,    98,    -1,
      31,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,    66,    66,    69,    70,    73,    74,    77,    83,    84,
      87,    97,   119,   120,   123,   124,   127,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     176,   178,   179,   182,   187,   189,   190,   193,   198,   200,
     201,   204,   209,   211,   212,   215,   220,   222,   223,   226,
     231,   232,   240,   241,   249,   250,   258,   259,   267,   270,
     271,   274,   282,   285,   286,   289,   298,   307,   317,   316,
     327,   329,   330,   333,   345,   348,   349,   352,   361,   364,
     365,   368,   377,   378,   381,   382,   385,   393,   394,   397,
     398,   401,   410,   419,   420,   429,   437,   438,   447,   456,
     459,   460,   463
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "DEFAULT_DEPTH", "SHUFFLE", "SUBST",
  "TAGS", "KFEAT", "KITEM", "KMONS", "KMASK", "NAME", "DEPTH", "ORIENT",
  "PLACE", "CHANCE", "MONS", "ITEM", "MARKER", "PRELUDE", "MAIN",
  "VALIDATE", "VETO", "NSUBST", "WELCOME", "LFLAGS", "BFLAGS", "COMMA",
  "INTEGER", "CHARACTER", "STRING", "MAP_LINE", "MONSTER_NAME",
  "ITEM_INFO", "LUA_LINE", "$accept", "file", "definitions", "definition",
  "error_seq", "def", "defdepth", "level", "map_specs", "map_spec", "name",
  "metaline", "global_lua", "global_lua_lines", "global_lua_line",
  "main_lua", "main_lua_lines", "main_lua_line", "validate_lua",
  "validate_lua_lines", "validate_lua_line", "veto_lua", "veto_lua_lines",
  "veto_lua_line", "prelude_lua", "prelude_lua_lines", "prelude_lua_line",
  "kfeat", "kmons", "kitem", "kmask", "shuffle", "shuffle_specifiers",
  "shuffle_spec", "tags", "tagstrings", "tagstring", "lflags", "bflags",
  "marker", "@1", "marker_spec", "mspec_segments", "mspec_segment",
  "nsubst", "nsubst_specifiers", "nsubst_spec", "subst",
  "subst_specifiers", "subst_spec", "items", "item_specifiers",
  "item_specifier", "mons", "mnames", "mname", "place", "depth", "chance",
  "orientation", "welcome", "map_def", "map_lines", "map_line", 0
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
     285,   286,   287,   288,   289
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    35,    36,    37,    37,    38,    38,    39,    40,    40,
      41,    42,    43,    43,    44,    44,    45,    46,    46,    46,
      46,    46,    46,    46,    46,    46,    46,    46,    46,    46,
      46,    46,    46,    46,    46,    46,    46,    46,    46,    46,
      47,    48,    48,    49,    50,    51,    51,    52,    53,    54,
      54,    55,    56,    57,    57,    58,    59,    60,    60,    61,
      62,    62,    63,    63,    64,    64,    65,    65,    66,    67,
      67,    68,    69,    70,    70,    71,    72,    73,    75,    74,
      76,    77,    77,    78,    79,    80,    80,    81,    82,    83,
      83,    84,    85,    85,    86,    86,    87,    88,    88,    89,
      89,    90,    91,    92,    92,    93,    94,    94,    95,    96,
      97,    97,    98
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     0,     2,     1,     1,     1,     1,     1,
       2,     2,     0,     2,     1,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     0,     2,     1,     2,     0,     2,     1,     2,     0,
       2,     1,     2,     0,     2,     1,     2,     0,     2,     1,
       1,     2,     1,     2,     1,     2,     1,     2,     2,     1,
       3,     1,     2,     0,     2,     1,     2,     2,     0,     3,
       1,     0,     2,     1,     2,     1,     3,     1,     2,     1,
       3,     1,     1,     2,     3,     1,     1,     1,     2,     3,
       1,     1,     2,     1,     2,     2,     1,     2,     2,     1,
       2,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       3,     0,     2,     1,     0,     0,    41,     4,     5,     8,
       6,    12,     9,    10,    16,    40,    11,    43,    42,     0,
       0,    73,    60,    64,    62,    66,   103,   106,     0,     0,
      97,    92,    78,    57,    45,    49,    53,     0,     0,     0,
       0,     7,   112,    39,    13,    14,    35,    37,    38,    36,
      31,    33,    32,    34,    27,    28,    29,    30,    24,    26,
      25,    23,    22,    17,    18,    19,    20,    21,    15,   109,
     111,    71,    68,    69,    91,    88,    89,    72,    61,    65,
      63,    67,   104,   107,   102,   105,   101,    98,   100,    96,
      93,    95,    81,    56,    44,    48,    52,    87,    84,    85,
     108,    76,    77,   110,     0,     0,    75,    74,     0,     0,
      79,    80,    59,    58,    47,    46,    51,    50,    55,    54,
       0,    70,    90,    99,    94,    83,    82,    86
};

/* YYDEFGOTO[NTERM-NUM]. */
static const yysigned_char yydefgoto[] =
{
      -1,     1,     2,     7,    43,     8,     9,    10,    16,    44,
      11,    45,    12,    15,    18,    46,    94,   115,    47,    95,
     117,    48,    96,   119,    49,    93,   113,    50,    51,    52,
      53,    54,    72,    73,    55,    77,   107,    56,    57,    58,
      92,   110,   111,   126,    59,    98,    99,    60,    75,    76,
      61,    90,    91,    62,    87,    88,    63,    64,    65,    66,
      67,    68,    69,    70
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -68
static const yysigned_char yypact[] =
{
     -68,     7,    20,   -68,    -6,    -2,   -68,   -68,   -68,   -68,
     -68,   -68,   -68,   -68,   -68,    -8,    -4,   -68,   -68,    -3,
      -1,   -68,     3,     4,     5,     6,     8,     9,    11,     1,
      10,    12,   -68,   -68,   -68,   -68,   -68,    13,    14,    17,
      18,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,
     -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,
     -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,    19,
     -68,   -68,    16,   -68,   -68,   -68,    22,    21,   -68,   -68,
     -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,    25,   -68,
      26,   -68,   -68,    23,    24,    27,    28,   -68,    29,   -68,
     -68,   -68,   -68,   -68,    -3,    -1,   -68,   -68,    10,    12,
     -68,    30,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,
      13,   -68,   -68,   -68,   -68,   -68,   -68,   -68
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
     -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,
     -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,
     -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,   -68,
     -68,   -68,   -68,   -67,   -68,   -68,   -68,   -68,   -68,   -68,
     -68,   -68,   -68,   -68,   -68,   -68,   -66,   -68,   -50,   -68,
     -68,   -68,   -46,   -68,   -49,   -68,   -68,   -68,   -68,   -68,
     -68,   -68,   -68,    -5
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
      38,    39,    40,     4,    13,    41,    17,    42,    14,    85,
      71,     5,    74,    78,    79,    80,    81,   121,    82,    83,
       6,    84,    86,   104,   100,    89,    97,   101,   102,   105,
      42,   106,   108,   109,   127,   122,   120,   112,   114,   123,
     125,   116,   118,   124,   103
};

static const unsigned char yycheck[] =
{
       4,     5,     6,     7,     8,     9,    10,     0,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,     3,    30,    29,    34,    31,    30,    28,
      33,    11,    33,    30,    30,    30,    30,   104,    30,    30,
      20,    30,    32,    27,    30,    33,    33,    30,    30,    27,
      31,    30,    27,    27,   120,   105,    27,    34,    34,   108,
      30,    34,    34,   109,    69
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    36,    37,     0,     3,    11,    20,    38,    40,    41,
      42,    45,    47,    30,    30,    48,    43,    34,    49,     4,
       5,     6,     7,     8,     9,    10,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    29,    31,    39,    44,    46,    50,    53,    56,    59,
      62,    63,    64,    65,    66,    69,    72,    73,    74,    79,
      82,    85,    88,    91,    92,    93,    94,    95,    96,    97,
      98,    33,    67,    68,    33,    83,    84,    70,    30,    30,
      30,    30,    30,    30,    30,    28,    32,    89,    90,    33,
      86,    87,    75,    60,    51,    54,    57,    33,    80,    81,
      30,    30,    30,    98,    27,    27,    30,    71,    27,    27,
      76,    77,    34,    61,    34,    52,    34,    55,    34,    58,
      27,    68,    83,    89,    87,    30,    78,    81
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
#line 66 "levcomp.ypp"
    { }
    break;

  case 3:
#line 69 "levcomp.ypp"
    {}
    break;

  case 4:
#line 70 "levcomp.ypp"
    {}
    break;

  case 5:
#line 73 "levcomp.ypp"
    {}
    break;

  case 6:
#line 74 "levcomp.ypp"
    {}
    break;

  case 7:
#line 78 "levcomp.ypp"
    {
                    yyerror("Unexpected character sequence.");
                }
    break;

  case 8:
#line 83 "levcomp.ypp"
    {}
    break;

  case 9:
#line 84 "levcomp.ypp"
    {}
    break;

  case 10:
#line 88 "levcomp.ypp"
    {
                    dgn_reset_default_depth();
                    std::string err = dgn_set_default_depth((yyvsp[0].text));
                    if (!err.empty())
                        yyerror(make_stringf("Bad default-depth: %s (%s)",
                                (yyvsp[0].text), err.c_str()).c_str());
                }
    break;

  case 11:
#line 98 "levcomp.ypp"
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
#line 119 "levcomp.ypp"
    { }
    break;

  case 13:
#line 120 "levcomp.ypp"
    { }
    break;

  case 14:
#line 123 "levcomp.ypp"
    { }
    break;

  case 15:
#line 124 "levcomp.ypp"
    { }
    break;

  case 16:
#line 128 "levcomp.ypp"
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

  case 39:
#line 173 "levcomp.ypp"
    {}
    break;

  case 40:
#line 176 "levcomp.ypp"
    { }
    break;

  case 41:
#line 178 "levcomp.ypp"
    { }
    break;

  case 42:
#line 179 "levcomp.ypp"
    { }
    break;

  case 43:
#line 183 "levcomp.ypp"
    {
                    lc_global_prelude.add(yylineno, (yyvsp[0].text));
                }
    break;

  case 44:
#line 187 "levcomp.ypp"
    { }
    break;

  case 45:
#line 189 "levcomp.ypp"
    { }
    break;

  case 46:
#line 190 "levcomp.ypp"
    { }
    break;

  case 47:
#line 194 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, (yyvsp[0].text));
                }
    break;

  case 48:
#line 198 "levcomp.ypp"
    { }
    break;

  case 49:
#line 200 "levcomp.ypp"
    { }
    break;

  case 50:
#line 201 "levcomp.ypp"
    { }
    break;

  case 51:
#line 205 "levcomp.ypp"
    {
                    lc_map.validate.add(yylineno, (yyvsp[0].text));
                }
    break;

  case 52:
#line 209 "levcomp.ypp"
    { }
    break;

  case 53:
#line 211 "levcomp.ypp"
    { }
    break;

  case 54:
#line 212 "levcomp.ypp"
    { }
    break;

  case 55:
#line 216 "levcomp.ypp"
    {
                    lc_map.veto.add(yylineno, (yyvsp[0].text));
                }
    break;

  case 56:
#line 220 "levcomp.ypp"
    { }
    break;

  case 57:
#line 222 "levcomp.ypp"
    { }
    break;

  case 58:
#line 223 "levcomp.ypp"
    { }
    break;

  case 59:
#line 227 "levcomp.ypp"
    {
                    lc_map.prelude.add(yylineno, (yyvsp[0].text));
                }
    break;

  case 60:
#line 231 "levcomp.ypp"
    { }
    break;

  case 61:
#line 233 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kfeat(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 62:
#line 240 "levcomp.ypp"
    { }
    break;

  case 63:
#line 242 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kmons(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 64:
#line 249 "levcomp.ypp"
    { }
    break;

  case 65:
#line 251 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kitem(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 66:
#line 258 "levcomp.ypp"
    { }
    break;

  case 67:
#line 260 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kmask(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 68:
#line 267 "levcomp.ypp"
    {}
    break;

  case 71:
#line 275 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("shuffle(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 72:
#line 282 "levcomp.ypp"
    {}
    break;

  case 75:
#line 290 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("tags(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 76:
#line 299 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lflags(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 77:
#line 308 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("bflags(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 78:
#line 317 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, "marker(");
                    start_marker_segment = true;
                }
    break;

  case 79:
#line 322 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, ")");
                }
    break;

  case 83:
#line 334 "levcomp.ypp"
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

  case 84:
#line 345 "levcomp.ypp"
    { }
    break;

  case 87:
#line 353 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("nsubst(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 88:
#line 361 "levcomp.ypp"
    { }
    break;

  case 91:
#line 369 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("subst(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 92:
#line 377 "levcomp.ypp"
    {}
    break;

  case 93:
#line 378 "levcomp.ypp"
    {}
    break;

  case 96:
#line 386 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("item(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 97:
#line 393 "levcomp.ypp"
    {}
    break;

  case 98:
#line 394 "levcomp.ypp"
    {}
    break;

  case 101:
#line 402 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("mons(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 102:
#line 411 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("place(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 103:
#line 419 "levcomp.ypp"
    {}
    break;

  case 104:
#line 421 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("depth(\"%s\")", 
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 105:
#line 430 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("chance(\"%d\")", (yyvsp[0].i)));
                }
    break;

  case 106:
#line 437 "levcomp.ypp"
    {}
    break;

  case 107:
#line 439 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("orient(\"%s\")", 
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 108:
#line 448 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("welcome(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;

  case 112:
#line 464 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("map(\"%s\")", 
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
    break;


    }

/* Line 1037 of yacc.c.  */
#line 1765 "levcomp.tab.c"

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


#line 472 "levcomp.ypp"


