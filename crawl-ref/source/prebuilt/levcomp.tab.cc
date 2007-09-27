/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

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
     COMMA = 283,
     INTEGER = 284,
     CHARACTER = 285,
     STRING = 286,
     MAP_LINE = 287,
     MONSTER_NAME = 288,
     ITEM_INFO = 289,
     LUA_LINE = 290
   };
#endif
/* Tokens.  */
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
#define COMMA 283
#define INTEGER 284
#define CHARACTER 285
#define STRING 286
#define MAP_LINE 287
#define MONSTER_NAME 288
#define ITEM_INFO 289
#define LUA_LINE 290




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

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 46 "levcomp.ypp"
{
    int i;
    const char *text;
    raw_range range;
}
/* Line 193 of yacc.c.  */
#line 216 "levcomp.tab.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 229 "levcomp.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
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
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   71

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  36
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  67
/* YYNRULES -- Number of rules.  */
#define YYNRULES  117
/* YYNRULES -- Number of states.  */
#define YYNSTATES  135

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   290

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
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
      35
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     6,     9,    11,    13,    15,    17,
      19,    22,    25,    26,    29,    31,    33,    36,    38,    40,
      42,    44,    46,    48,    50,    52,    54,    56,    58,    60,
      62,    64,    66,    68,    70,    72,    74,    76,    78,    80,
      82,    84,    87,    88,    91,    93,    96,    97,   100,   102,
     105,   106,   109,   111,   114,   115,   118,   120,   123,   124,
     127,   129,   131,   134,   136,   139,   141,   144,   146,   149,
     152,   154,   158,   160,   163,   164,   167,   169,   172,   175,
     176,   180,   182,   183,   186,   188,   191,   193,   197,   199,
     202,   204,   208,   210,   213,   215,   219,   221,   223,   226,
     230,   232,   234,   236,   239,   243,   245,   247,   250,   252,
     255,   258,   260,   263,   266,   268,   271,   273
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      37,     0,    -1,    38,    -1,    -1,    38,    39,    -1,    41,
      -1,    43,    -1,    30,    -1,    42,    -1,    48,    -1,     3,
      31,    -1,    46,    44,    -1,    -1,    44,    45,    -1,    47,
      -1,   100,    -1,    11,    31,    -1,    95,    -1,    96,    -1,
      97,    -1,    98,    -1,    99,    -1,    92,    -1,    89,    -1,
      75,    -1,    86,    -1,    83,    -1,    80,    -1,    67,    -1,
      70,    -1,    73,    -1,    74,    -1,    63,    -1,    65,    -1,
      64,    -1,    66,    -1,    51,    -1,    60,    -1,    54,    -1,
      57,    -1,    40,    -1,    21,    49,    -1,    -1,    49,    50,
      -1,    35,    -1,    21,    52,    -1,    -1,    52,    53,    -1,
      35,    -1,    22,    55,    -1,    -1,    55,    56,    -1,    35,
      -1,    23,    58,    -1,    -1,    58,    59,    -1,    35,    -1,
      20,    61,    -1,    -1,    61,    62,    -1,    35,    -1,     7,
      -1,     7,    31,    -1,     9,    -1,     9,    31,    -1,     8,
      -1,     8,    31,    -1,    10,    -1,    10,    31,    -1,     4,
      68,    -1,    69,    -1,    68,    28,    69,    -1,    34,    -1,
       6,    71,    -1,    -1,    71,    72,    -1,    31,    -1,    26,
      31,    -1,    27,    31,    -1,    -1,    18,    76,    77,    -1,
      78,    -1,    -1,    78,    79,    -1,    31,    -1,    19,    81,
      -1,    82,    -1,    81,    28,    82,    -1,    34,    -1,    24,
      84,    -1,    85,    -1,    84,    28,    85,    -1,    34,    -1,
       5,    87,    -1,    88,    -1,    88,    28,    87,    -1,    34,
      -1,    17,    -1,    17,    90,    -1,    90,    28,    91,    -1,
      91,    -1,    34,    -1,    16,    -1,    16,    93,    -1,    94,
      28,    93,    -1,    94,    -1,    33,    -1,    14,    31,    -1,
      12,    -1,    12,    31,    -1,    15,    29,    -1,    13,    -1,
      13,    31,    -1,    25,    31,    -1,   101,    -1,   101,   102,
      -1,   102,    -1,    32,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    66,    66,    69,    70,    73,    74,    77,    83,    84,
      87,    97,   119,   120,   123,   124,   127,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   177,   179,   180,   183,   188,   190,   191,   194,   199,
     201,   202,   205,   210,   212,   213,   216,   221,   223,   224,
     227,   232,   233,   241,   242,   250,   251,   259,   260,   268,
     271,   272,   275,   283,   286,   287,   290,   299,   308,   318,
     317,   328,   330,   331,   334,   346,   349,   350,   353,   362,
     365,   366,   369,   378,   381,   382,   385,   394,   395,   398,
     399,   402,   410,   411,   414,   415,   418,   427,   436,   437,
     446,   454,   455,   464,   473,   476,   477,   480
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "DEFAULT_DEPTH", "SHUFFLE", "SUBST",
  "TAGS", "KFEAT", "KITEM", "KMONS", "KMASK", "NAME", "DEPTH", "ORIENT",
  "PLACE", "CHANCE", "MONS", "ITEM", "MARKER", "COLOUR", "PRELUDE", "MAIN",
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
  "colour", "colour_specifiers", "colour_specifier", "nsubst",
  "nsubst_specifiers", "nsubst_spec", "subst", "subst_specifiers",
  "subst_spec", "items", "item_specifiers", "item_specifier", "mons",
  "mnames", "mname", "place", "depth", "chance", "orientation", "welcome",
  "map_def", "map_lines", "map_line", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    36,    37,    38,    38,    39,    39,    40,    41,    41,
      42,    43,    44,    44,    45,    45,    46,    47,    47,    47,
      47,    47,    47,    47,    47,    47,    47,    47,    47,    47,
      47,    47,    47,    47,    47,    47,    47,    47,    47,    47,
      47,    48,    49,    49,    50,    51,    52,    52,    53,    54,
      55,    55,    56,    57,    58,    58,    59,    60,    61,    61,
      62,    63,    63,    64,    64,    65,    65,    66,    66,    67,
      68,    68,    69,    70,    71,    71,    72,    73,    74,    76,
      75,    77,    78,    78,    79,    80,    81,    81,    82,    83,
      84,    84,    85,    86,    87,    87,    88,    89,    89,    90,
      90,    91,    92,    92,    93,    93,    94,    95,    96,    96,
      97,    98,    98,    99,   100,   101,   101,   102
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     1,     1,     1,     1,
       2,     2,     0,     2,     1,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     0,     2,     1,     2,     0,     2,     1,     2,
       0,     2,     1,     2,     0,     2,     1,     2,     0,     2,
       1,     1,     2,     1,     2,     1,     2,     1,     2,     2,
       1,     3,     1,     2,     0,     2,     1,     2,     2,     0,
       3,     1,     0,     2,     1,     2,     1,     3,     1,     2,
       1,     3,     1,     2,     1,     3,     1,     1,     2,     3,
       1,     1,     1,     2,     3,     1,     1,     2,     1,     2,
       2,     1,     2,     2,     1,     2,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,     0,     0,    42,     4,     5,     8,
       6,    12,     9,    10,    16,    41,    11,    44,    43,     0,
       0,    74,    61,    65,    63,    67,   108,   111,     0,     0,
     102,    97,    79,     0,    58,    46,    50,    54,     0,     0,
       0,     0,     7,   117,    40,    13,    14,    36,    38,    39,
      37,    32,    34,    33,    35,    28,    29,    30,    31,    24,
      27,    26,    25,    23,    22,    17,    18,    19,    20,    21,
      15,   114,   116,    72,    69,    70,    96,    93,    94,    73,
      62,    66,    64,    68,   109,   112,   107,   110,   106,   103,
     105,   101,    98,   100,    82,    88,    85,    86,    57,    45,
      49,    53,    92,    89,    90,   113,    77,    78,   115,     0,
       0,    76,    75,     0,     0,    80,    81,     0,    60,    59,
      48,    47,    52,    51,    56,    55,     0,    71,    95,   104,
      99,    84,    83,    87,    91
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     7,    44,     8,     9,    10,    16,    45,
      11,    46,    12,    15,    18,    47,    99,   121,    48,   100,
     123,    49,   101,   125,    50,    98,   119,    51,    52,    53,
      54,    55,    74,    75,    56,    79,   112,    57,    58,    59,
      94,   115,   116,   132,    60,    96,    97,    61,   103,   104,
      62,    77,    78,    63,    92,    93,    64,    89,    90,    65,
      66,    67,    68,    69,    70,    71,    72
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -72
static const yytype_int8 yypact[] =
{
     -72,     7,    21,   -72,    -6,    -2,   -72,   -72,   -72,   -72,
     -72,   -72,   -72,   -72,   -72,    -8,    -4,   -72,   -72,    -3,
      -1,   -72,     3,     4,     5,     6,     8,     9,    10,     1,
      11,    12,   -72,    13,   -72,   -72,   -72,   -72,    14,    18,
      19,    20,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,
     -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,
     -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,
     -72,    22,   -72,   -72,    15,   -72,   -72,   -72,    17,    24,
     -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,
      25,   -72,    28,   -72,   -72,   -72,    29,   -72,    23,    26,
      27,    30,   -72,    31,   -72,   -72,   -72,   -72,   -72,    -3,
      -1,   -72,   -72,    11,    12,   -72,    32,    13,   -72,   -72,
     -72,   -72,   -72,   -72,   -72,   -72,    14,   -72,   -72,   -72,
     -72,   -72,   -72,   -72,   -72
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,
     -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,
     -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,   -72,
     -72,   -72,   -72,   -71,   -72,   -72,   -72,   -72,   -72,   -72,
     -72,   -72,   -72,   -72,   -72,   -72,   -65,   -72,   -72,   -66,
     -72,   -46,   -72,   -72,   -72,   -48,   -72,   -45,   -72,   -72,
     -72,   -72,   -72,   -72,   -72,   -72,     0
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      19,    20,    21,    22,    23,    24,    25,     3,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     4,    13,    42,    17,    43,    14,
      87,    73,     5,    76,    80,    81,    82,    83,   127,    84,
      85,    86,     6,   109,    88,   110,    91,    95,   102,   105,
     106,   107,   133,   113,    43,   111,   114,   117,   118,   126,
     134,   120,   122,   131,   128,   124,   130,     0,   129,     0,
       0,   108
};

static const yytype_int8 yycheck[] =
{
       4,     5,     6,     7,     8,     9,    10,     0,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,     3,    31,    30,    35,    32,    31,
      29,    34,    11,    34,    31,    31,    31,    31,   109,    31,
      31,    31,    21,    28,    33,    28,    34,    34,    34,    31,
      31,    31,   117,    28,    32,    31,    28,    28,    35,    28,
     126,    35,    35,    31,   110,    35,   114,    -1,   113,    -1,
      -1,    71
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    37,    38,     0,     3,    11,    21,    39,    41,    42,
      43,    46,    48,    31,    31,    49,    44,    35,    50,     4,
       5,     6,     7,     8,     9,    10,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    30,    32,    40,    45,    47,    51,    54,    57,
      60,    63,    64,    65,    66,    67,    70,    73,    74,    75,
      80,    83,    86,    89,    92,    95,    96,    97,    98,    99,
     100,   101,   102,    34,    68,    69,    34,    87,    88,    71,
      31,    31,    31,    31,    31,    31,    31,    29,    33,    93,
      94,    34,    90,    91,    76,    34,    81,    82,    61,    52,
      55,    58,    34,    84,    85,    31,    31,    31,   102,    28,
      28,    31,    72,    28,    28,    77,    78,    28,    35,    62,
      35,    53,    35,    56,    35,    59,    28,    69,    87,    93,
      91,    31,    79,    82,    85
};

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
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
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
    while (YYID (0))
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
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

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
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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
  YYUSE (yyvaluep);

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
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

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

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
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

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

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

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

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
                    std::string err = dgn_set_default_depth((yyvsp[(2) - (2)].text));
                    if (!err.empty())
                        yyerror(make_stringf("Bad default-depth: %s (%s)",
                                (yyvsp[(2) - (2)].text), err.c_str()).c_str());
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
                    lc_map.name = (yyvsp[(2) - (2)].text);

                    map_load_info_t::const_iterator i = 
                        lc_loaded_maps.find((yyvsp[(2) - (2)].text));

                    if (i != lc_loaded_maps.end())
                    {
                        yyerror(
                            make_stringf(
                                "Map named '%s' already loaded at %s:%d",
                                (yyvsp[(2) - (2)].text),
                                i->second.filename.c_str(),
                                i->second.lineno).c_str() );
                    }

                    lc_map.place_loaded_from =
                        map_file_place(lc_desfile, yylineno);
                    lc_loaded_maps[(yyvsp[(2) - (2)].text)] = lc_map.place_loaded_from;
                }
    break;

  case 40:
#line 174 "levcomp.ypp"
    {}
    break;

  case 41:
#line 177 "levcomp.ypp"
    { }
    break;

  case 42:
#line 179 "levcomp.ypp"
    { }
    break;

  case 43:
#line 180 "levcomp.ypp"
    { }
    break;

  case 44:
#line 184 "levcomp.ypp"
    {
                    lc_global_prelude.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 45:
#line 188 "levcomp.ypp"
    { }
    break;

  case 46:
#line 190 "levcomp.ypp"
    { }
    break;

  case 47:
#line 191 "levcomp.ypp"
    { }
    break;

  case 48:
#line 195 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 49:
#line 199 "levcomp.ypp"
    { }
    break;

  case 50:
#line 201 "levcomp.ypp"
    { }
    break;

  case 51:
#line 202 "levcomp.ypp"
    { }
    break;

  case 52:
#line 206 "levcomp.ypp"
    {
                    lc_map.validate.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 53:
#line 210 "levcomp.ypp"
    { }
    break;

  case 54:
#line 212 "levcomp.ypp"
    { }
    break;

  case 55:
#line 213 "levcomp.ypp"
    { }
    break;

  case 56:
#line 217 "levcomp.ypp"
    {
                    lc_map.veto.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 57:
#line 221 "levcomp.ypp"
    { }
    break;

  case 58:
#line 223 "levcomp.ypp"
    { }
    break;

  case 59:
#line 224 "levcomp.ypp"
    { }
    break;

  case 60:
#line 228 "levcomp.ypp"
    {
                    lc_map.prelude.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 61:
#line 232 "levcomp.ypp"
    { }
    break;

  case 62:
#line 234 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kfeat(\"%s\")", 
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 63:
#line 241 "levcomp.ypp"
    { }
    break;

  case 64:
#line 243 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kmons(\"%s\")", 
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 65:
#line 250 "levcomp.ypp"
    { }
    break;

  case 66:
#line 252 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kitem(\"%s\")", 
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 67:
#line 259 "levcomp.ypp"
    { }
    break;

  case 68:
#line 261 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("kmask(\"%s\")", 
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 69:
#line 268 "levcomp.ypp"
    {}
    break;

  case 72:
#line 276 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("shuffle(\"%s\")", 
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 73:
#line 283 "levcomp.ypp"
    {}
    break;

  case 76:
#line 291 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("tags(\"%s\")", 
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 77:
#line 300 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lflags(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 78:
#line 309 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("bflags(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 79:
#line 318 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, "marker(");
                    start_marker_segment = true;
                }
    break;

  case 80:
#line 323 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, ")");
                }
    break;

  case 84:
#line 335 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf(
                            "%s\"%s\"",
                            start_marker_segment? "" : " .. ",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                    start_marker_segment = false;
                }
    break;

  case 85:
#line 346 "levcomp.ypp"
    { }
    break;

  case 86:
#line 349 "levcomp.ypp"
    { }
    break;

  case 87:
#line 350 "levcomp.ypp"
    { }
    break;

  case 88:
#line 354 "levcomp.ypp"
    {
                      lc_map.main.add(
                          yylineno,
                          make_stringf("colour(\"%s\")",
                              quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                  }
    break;

  case 89:
#line 362 "levcomp.ypp"
    { }
    break;

  case 90:
#line 365 "levcomp.ypp"
    { }
    break;

  case 91:
#line 366 "levcomp.ypp"
    { }
    break;

  case 92:
#line 370 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("nsubst(\"%s\")", 
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 93:
#line 378 "levcomp.ypp"
    { }
    break;

  case 96:
#line 386 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("subst(\"%s\")", 
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 97:
#line 394 "levcomp.ypp"
    {}
    break;

  case 98:
#line 395 "levcomp.ypp"
    {}
    break;

  case 101:
#line 403 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("item(\"%s\")", 
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 102:
#line 410 "levcomp.ypp"
    {}
    break;

  case 103:
#line 411 "levcomp.ypp"
    {}
    break;

  case 106:
#line 419 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("mons(\"%s\")", 
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 107:
#line 428 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("place(\"%s\")", 
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 108:
#line 436 "levcomp.ypp"
    {}
    break;

  case 109:
#line 438 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("depth(\"%s\")", 
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 110:
#line 447 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("chance(\"%d\")", (yyvsp[(2) - (2)].i)));
                }
    break;

  case 111:
#line 454 "levcomp.ypp"
    {}
    break;

  case 112:
#line 456 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("orient(\"%s\")", 
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 113:
#line 465 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("welcome(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 117:
#line 481 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno, 
                        make_stringf("map(\"%s\")", 
                                     quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;


/* Line 1267 of yacc.c.  */
#line 2116 "levcomp.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
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
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
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

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
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


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
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
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 489 "levcomp.ypp"


