
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 1 "levcomp.ypp"



#include "AppHdr.h"
#include <map>
#include <algorithm>
#include "end.h"
#include "l_defs.h"
#include "mapdef.h"
#include "maps.h"
#include "stringutil.h"

#define YYERROR_VERBOSE 1
#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 0

int yylex();

extern int yylineno;

static NORETURN void yyerror(const char *e)
{
    if (strstr(e, lc_desfile.c_str()) == e)
        fprintf(stderr, "%s\n", e);
    else
        fprintf(stderr, "%s:%d: %s\n", lc_desfile.c_str(), yylineno, e);
    // Bail bail bail.
    end(1);
}



/* Line 189 of yacc.c  */
#line 106 "levcomp.tab.cc"

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


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     DEFAULT_DEPTH = 258,
     SHUFFLE = 259,
     CLEAR = 260,
     SUBST = 261,
     TAGS = 262,
     KFEAT = 263,
     KITEM = 264,
     KMONS = 265,
     KMASK = 266,
     KPROP = 267,
     NAME = 268,
     DEPTH = 269,
     ORIENT = 270,
     PLACE = 271,
     CHANCE = 272,
     WEIGHT = 273,
     MONS = 274,
     ITEM = 275,
     MARKER = 276,
     COLOUR = 277,
     PRELUDE = 278,
     MAIN = 279,
     VALIDATE = 280,
     VETO = 281,
     EPILOGUE = 282,
     NSUBST = 283,
     WELCOME = 284,
     LFLOORCOL = 285,
     LROCKCOL = 286,
     LFLOORTILE = 287,
     LROCKTILE = 288,
     FTILE = 289,
     RTILE = 290,
     TILE = 291,
     SUBVAULT = 292,
     FHEIGHT = 293,
     DESC = 294,
     ORDER = 295,
     COMMA = 296,
     COLON = 297,
     PERC = 298,
     DASH = 299,
     CHARACTER = 300,
     NUMBER = 301,
     STRING = 302,
     MAP_LINE = 303,
     MONSTER_NAME = 304,
     ITEM_INFO = 305,
     LUA_LINE = 306
   };
#endif
/* Tokens.  */
#define DEFAULT_DEPTH 258
#define SHUFFLE 259
#define CLEAR 260
#define SUBST 261
#define TAGS 262
#define KFEAT 263
#define KITEM 264
#define KMONS 265
#define KMASK 266
#define KPROP 267
#define NAME 268
#define DEPTH 269
#define ORIENT 270
#define PLACE 271
#define CHANCE 272
#define WEIGHT 273
#define MONS 274
#define ITEM 275
#define MARKER 276
#define COLOUR 277
#define PRELUDE 278
#define MAIN 279
#define VALIDATE 280
#define VETO 281
#define EPILOGUE 282
#define NSUBST 283
#define WELCOME 284
#define LFLOORCOL 285
#define LROCKCOL 286
#define LFLOORTILE 287
#define LROCKTILE 288
#define FTILE 289
#define RTILE 290
#define TILE 291
#define SUBVAULT 292
#define FHEIGHT 293
#define DESC 294
#define ORDER 295
#define COMMA 296
#define COLON 297
#define PERC 298
#define DASH 299
#define CHARACTER 300
#define NUMBER 301
#define STRING 302
#define MAP_LINE 303
#define MONSTER_NAME 304
#define ITEM_INFO 305
#define LUA_LINE 306




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 34 "levcomp.ypp"

    int i;
    double f;
    const char *text;
    map_chance_pair chance;



/* Line 214 of yacc.c  */
#line 253 "levcomp.tab.cc"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 265 "levcomp.tab.cc"

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
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
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
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
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
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   117

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  52
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  94
/* YYNRULES -- Number of rules.  */
#define YYNRULES  174
/* YYNRULES -- Number of states.  */
#define YYNSTATES  208

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   306

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
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51
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
      82,    84,    86,    88,    90,    92,    94,    96,    98,   100,
     102,   104,   106,   108,   110,   113,   114,   117,   119,   122,
     123,   126,   128,   131,   132,   135,   137,   140,   141,   144,
     146,   149,   150,   153,   155,   158,   159,   162,   164,   166,
     169,   171,   174,   176,   179,   181,   184,   186,   189,   192,
     194,   198,   200,   203,   206,   207,   210,   212,   215,   218,
     220,   223,   225,   228,   230,   233,   235,   238,   241,   243,
     247,   249,   252,   254,   258,   260,   263,   265,   269,   271,
     274,   276,   280,   282,   284,   288,   290,   293,   295,   299,
     301,   304,   306,   310,   312,   314,   317,   321,   323,   325,
     327,   330,   334,   336,   338,   341,   344,   347,   349,   352,
     355,   357,   361,   363,   366,   368,   372,   374,   377,   379,
     382,   386,   388,   391,   393,   395,   398,   401,   403,   406,
     408,   410,   413,   415,   419
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
      53,     0,    -1,    54,    -1,    -1,    54,    55,    -1,    57,
      -1,    59,    -1,    45,    -1,    58,    -1,    64,    -1,     3,
      47,    -1,    62,    60,    -1,    -1,    60,    61,    -1,    63,
      -1,   140,    -1,    13,    47,    -1,   126,    -1,   127,    -1,
     128,    -1,   129,    -1,   130,    -1,   135,    -1,   138,    -1,
     139,    -1,   123,    -1,   120,    -1,    94,    -1,   117,    -1,
     114,    -1,    95,    -1,    96,    -1,    97,    -1,    98,    -1,
      99,    -1,   103,    -1,   106,    -1,   109,    -1,    87,    -1,
      90,    -1,    91,    -1,    82,    -1,    84,    -1,    83,    -1,
      85,    -1,    86,    -1,   100,    -1,   143,    -1,    67,    -1,
      76,    -1,    70,    -1,    73,    -1,    79,    -1,    56,    -1,
      24,    65,    -1,    -1,    65,    66,    -1,    51,    -1,    24,
      68,    -1,    -1,    68,    69,    -1,    51,    -1,    25,    71,
      -1,    -1,    71,    72,    -1,    51,    -1,    26,    74,    -1,
      -1,    74,    75,    -1,    51,    -1,    23,    77,    -1,    -1,
      77,    78,    -1,    51,    -1,    27,    80,    -1,    -1,    80,
      81,    -1,    51,    -1,     8,    -1,     8,    47,    -1,    10,
      -1,    10,    47,    -1,     9,    -1,     9,    47,    -1,    11,
      -1,    11,    47,    -1,    12,    -1,    12,    47,    -1,     4,
      88,    -1,    89,    -1,    88,    41,    89,    -1,    50,    -1,
       5,    47,    -1,     7,    92,    -1,    -1,    92,    93,    -1,
      47,    -1,    21,    47,    -1,    22,   112,    -1,    30,    -1,
      30,    47,    -1,    31,    -1,    31,    47,    -1,    32,    -1,
      32,    47,    -1,    33,    -1,    33,    47,    -1,    38,   101,
      -1,   102,    -1,   101,    41,   102,    -1,    50,    -1,    34,
     104,    -1,   105,    -1,   104,    41,   105,    -1,    50,    -1,
      35,   107,    -1,   108,    -1,   107,    41,   108,    -1,    50,
      -1,    36,   110,    -1,   111,    -1,   110,    41,   111,    -1,
      50,    -1,   113,    -1,   112,    41,   113,    -1,    50,    -1,
      28,   115,    -1,   116,    -1,   115,    41,   116,    -1,    50,
      -1,     6,   118,    -1,   119,    -1,   119,    41,   118,    -1,
      50,    -1,    20,    -1,    20,   121,    -1,   121,    41,   122,
      -1,   122,    -1,    50,    -1,    19,    -1,    19,   124,    -1,
     125,    41,   124,    -1,   125,    -1,    49,    -1,    16,    47,
      -1,    39,    47,    -1,    40,    46,    -1,    14,    -1,    14,
      47,    -1,    17,   131,    -1,    17,    -1,   131,    41,   134,
      -1,   134,    -1,    46,    43,    -1,    46,    -1,    46,    42,
     132,    -1,   132,    -1,   133,    47,    -1,   133,    -1,    18,
     136,    -1,   136,    41,   137,    -1,   137,    -1,    46,    47,
      -1,    46,    -1,    15,    -1,    15,    47,    -1,    29,    47,
      -1,   141,    -1,   141,   142,    -1,   142,    -1,    48,    -1,
      37,   144,    -1,   145,    -1,   144,    41,   145,    -1,    47,
      -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    60,    60,    63,    64,    67,    68,    71,    77,    78,
      81,    91,   112,   113,   116,   117,   120,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,   155,   156,
     157,   158,   159,   160,   161,   162,   163,   164,   165,   166,
     167,   168,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   183,   185,   186,   189,   194,   196,
     197,   200,   205,   207,   208,   211,   216,   218,   219,   222,
     227,   229,   230,   233,   238,   240,   241,   244,   249,   250,
     258,   259,   267,   268,   276,   277,   285,   286,   294,   297,
     298,   301,   309,   318,   321,   322,   325,   334,   367,   370,
     371,   379,   380,   388,   389,   398,   399,   408,   410,   411,
     414,   423,   426,   427,   430,   439,   442,   443,   446,   455,
     458,   459,   462,   472,   473,   476,   485,   488,   489,   492,
     501,   504,   505,   508,   517,   518,   521,   522,   525,   533,
     534,   537,   538,   541,   550,   559,   568,   576,   577,   586,
     587,   590,   591,   593,   597,   602,   607,   614,   622,   631,
     633,   634,   636,   643,   651,   652,   661,   670,   673,   674,
     677,   686,   689,   690,   693
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "DEFAULT_DEPTH", "SHUFFLE", "CLEAR",
  "SUBST", "TAGS", "KFEAT", "KITEM", "KMONS", "KMASK", "KPROP", "NAME",
  "DEPTH", "ORIENT", "PLACE", "CHANCE", "WEIGHT", "MONS", "ITEM", "MARKER",
  "COLOUR", "PRELUDE", "MAIN", "VALIDATE", "VETO", "EPILOGUE", "NSUBST",
  "WELCOME", "LFLOORCOL", "LROCKCOL", "LFLOORTILE", "LROCKTILE", "FTILE",
  "RTILE", "TILE", "SUBVAULT", "FHEIGHT", "DESC", "ORDER", "COMMA",
  "COLON", "PERC", "DASH", "CHARACTER", "NUMBER", "STRING", "MAP_LINE",
  "MONSTER_NAME", "ITEM_INFO", "LUA_LINE", "$accept", "file",
  "definitions", "definition", "error_seq", "def", "defdepth", "level",
  "map_specs", "map_spec", "name", "metaline", "global_lua",
  "global_lua_lines", "global_lua_line", "main_lua", "main_lua_lines",
  "main_lua_line", "validate_lua", "validate_lua_lines",
  "validate_lua_line", "veto_lua", "veto_lua_lines", "veto_lua_line",
  "prelude_lua", "prelude_lua_lines", "prelude_lua_line", "epilogue_lua",
  "epilogue_lua_lines", "epilogue_lua_line", "kfeat", "kmons", "kitem",
  "kmask", "kprop", "shuffle", "shuffle_specifiers", "shuffle_spec",
  "clear", "tags", "tagstrings", "tagstring", "marker", "colour",
  "lfloorcol", "lrockcol", "lfloortile", "lrocktile", "fheight",
  "fheight_specifiers", "fheight_specifier", "ftile", "ftile_specifiers",
  "ftile_specifier", "rtile", "rtile_specifiers", "rtile_specifier",
  "tile", "tile_specifiers", "tile_specifier", "colour_specifiers",
  "colour_specifier", "nsubst", "nsubst_specifiers", "nsubst_spec",
  "subst", "subst_specifiers", "subst_spec", "items", "item_specifiers",
  "item_specifier", "mons", "mnames", "mname", "place", "desc", "order",
  "depth", "chance", "chance_specifiers", "chance_roll", "chance_num",
  "chance_specifier", "weight", "weight_specifiers", "weight_specifier",
  "orientation", "welcome", "map_def", "map_lines", "map_line", "subvault",
  "subvault_specifiers", "subvault_specifier", 0
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
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    52,    53,    54,    54,    55,    55,    56,    57,    57,
      58,    59,    60,    60,    61,    61,    62,    63,    63,    63,
      63,    63,    63,    63,    63,    63,    63,    63,    63,    63,
      63,    63,    63,    63,    63,    63,    63,    63,    63,    63,
      63,    63,    63,    63,    63,    63,    63,    63,    63,    63,
      63,    63,    63,    63,    64,    65,    65,    66,    67,    68,
      68,    69,    70,    71,    71,    72,    73,    74,    74,    75,
      76,    77,    77,    78,    79,    80,    80,    81,    82,    82,
      83,    83,    84,    84,    85,    85,    86,    86,    87,    88,
      88,    89,    90,    91,    92,    92,    93,    94,    95,    96,
      96,    97,    97,    98,    98,    99,    99,   100,   101,   101,
     102,   103,   104,   104,   105,   106,   107,   107,   108,   109,
     110,   110,   111,   112,   112,   113,   114,   115,   115,   116,
     117,   118,   118,   119,   120,   120,   121,   121,   122,   123,
     123,   124,   124,   125,   126,   127,   128,   129,   129,   130,
     130,   131,   131,   132,   132,   133,   133,   134,   134,   135,
     136,   136,   137,   137,   138,   138,   139,   140,   141,   141,
     142,   143,   144,   144,   145
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     1,     1,     1,     1,
       2,     2,     0,     2,     1,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     0,     2,     1,     2,     0,
       2,     1,     2,     0,     2,     1,     2,     0,     2,     1,
       2,     0,     2,     1,     2,     0,     2,     1,     1,     2,
       1,     2,     1,     2,     1,     2,     1,     2,     2,     1,
       3,     1,     2,     2,     0,     2,     1,     2,     2,     1,
       2,     1,     2,     1,     2,     1,     2,     2,     1,     3,
       1,     2,     1,     3,     1,     2,     1,     3,     1,     2,
       1,     3,     1,     1,     3,     1,     2,     1,     3,     1,
       2,     1,     3,     1,     1,     2,     3,     1,     1,     1,
       2,     3,     1,     1,     2,     2,     2,     1,     2,     2,
       1,     3,     1,     2,     1,     3,     1,     2,     1,     2,
       3,     1,     2,     1,     1,     2,     2,     1,     2,     1,
       1,     2,     1,     3,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,     0,     0,    55,     4,     5,     8,
       6,    12,     9,    10,    16,    54,    11,    57,    56,     0,
       0,     0,    94,    78,    82,    80,    84,    86,   147,   164,
       0,   150,     0,   139,   134,     0,     0,    71,    59,    63,
      67,    75,     0,     0,    99,   101,   103,   105,     0,     0,
       0,     0,     0,     0,     0,     7,   170,    53,    13,    14,
      48,    50,    51,    49,    52,    41,    43,    42,    44,    45,
      38,    39,    40,    27,    30,    31,    32,    33,    34,    46,
      35,    36,    37,    29,    28,    26,    25,    17,    18,    19,
      20,    21,    22,    23,    24,    15,   167,   169,    47,    91,
      88,    89,    92,   133,   130,   131,    93,    79,    83,    81,
      85,    87,   148,   165,   144,   154,   149,   156,   158,   152,
     163,   159,   161,   143,   140,   142,   138,   135,   137,    97,
     125,    98,   123,    70,    58,    62,    66,    74,   129,   126,
     127,   166,   100,   102,   104,   106,   114,   111,   112,   118,
     115,   116,   122,   119,   120,   174,   171,   172,   110,   107,
     108,   145,   146,   168,     0,     0,    96,    95,     0,   153,
       0,   157,   162,     0,     0,     0,     0,    73,    72,    61,
      60,    65,    64,    69,    68,    77,    76,     0,     0,     0,
       0,     0,     0,    90,   132,   154,   155,   151,   160,   141,
     136,   124,   128,   113,   117,   121,   173,   109
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     7,    57,     8,     9,    10,    16,    58,
      11,    59,    12,    15,    18,    60,   134,   180,    61,   135,
     182,    62,   136,   184,    63,   133,   178,    64,   137,   186,
      65,    66,    67,    68,    69,    70,   100,   101,    71,    72,
     106,   167,    73,    74,    75,    76,    77,    78,    79,   159,
     160,    80,   147,   148,    81,   150,   151,    82,   153,   154,
     131,   132,    83,   139,   140,    84,   104,   105,    85,   127,
     128,    86,   124,   125,    87,    88,    89,    90,    91,   116,
     117,   118,   119,    92,   121,   122,    93,    94,    95,    96,
      97,    98,   156,   157
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -128
static const yytype_int8 yypact[] =
{
    -128,     9,    34,  -128,    -9,    -5,  -128,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,    -8,    -4,  -128,  -128,    -2,
      -1,     0,  -128,     2,     4,     5,     6,     7,     8,    10,
      12,    14,    15,    13,    16,    17,    18,  -128,  -128,  -128,
    -128,  -128,    19,    20,    23,    24,    25,    26,    27,    28,
      29,    33,    31,    35,    30,  -128,  -128,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,  -128,    36,  -128,  -128,  -128,
      22,  -128,  -128,  -128,  -128,    42,    38,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,    -3,    45,  -128,    40,  -128,
      41,    48,  -128,  -128,  -128,    49,  -128,    50,  -128,  -128,
    -128,    51,  -128,    -6,    43,    44,    46,    47,  -128,    52,
    -128,  -128,  -128,  -128,  -128,  -128,  -128,    55,  -128,  -128,
      58,  -128,  -128,    59,  -128,  -128,    60,  -128,  -128,    61,
    -128,  -128,  -128,  -128,    -2,     0,  -128,  -128,    57,  -128,
      14,  -128,  -128,    15,    13,    16,    18,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,  -128,  -128,    19,    27,    28,
      29,    33,    31,  -128,  -128,    32,  -128,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
    -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,  -128,  -128,  -108,  -128,  -128,
    -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,  -128,
    -127,  -128,  -128,  -114,  -128,  -128,   -85,  -128,  -128,   -84,
    -128,   -71,  -128,  -128,   -80,  -128,   -57,  -128,  -128,  -128,
     -66,  -128,   -64,  -128,  -128,  -128,  -128,  -128,  -128,  -128,
     -56,  -128,   -59,  -128,  -128,   -60,  -128,  -128,  -128,  -128,
      21,  -128,  -128,   -77
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      19,    20,    21,    22,    23,    24,    25,    26,    27,     3,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,     4,    13,   168,
     169,    55,    14,    17,    56,   177,   102,     5,    99,   107,
     103,   108,   109,   110,   111,   112,   193,   113,     6,   114,
     115,   120,   123,   164,   129,   207,   126,   141,   130,   138,
     142,   143,   144,   145,   203,   169,   162,   146,   149,   152,
     155,   158,   161,   165,    56,   166,   170,   171,   172,   173,
     174,   175,   176,   187,   179,   181,   188,   183,   185,   189,
     190,   191,   192,   195,   204,   201,   205,   202,   194,   200,
     199,   197,   196,   198,   206,     0,     0,   163
};

static const yytype_int16 yycheck[] =
{
       4,     5,     6,     7,     8,     9,    10,    11,    12,     0,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,     3,    47,    42,
      43,    45,    47,    51,    48,    51,    47,    13,    50,    47,
      50,    47,    47,    47,    47,    47,   164,    47,    24,    47,
      46,    46,    49,    41,    47,   192,    50,    47,    50,    50,
      47,    47,    47,    47,   188,    43,    46,    50,    50,    50,
      47,    50,    47,    41,    48,    47,    41,    47,    47,    41,
      41,    41,    41,    41,    51,    51,    41,    51,    51,    41,
      41,    41,    41,    46,   189,   176,   190,   187,   165,   175,
     174,   170,   168,   173,   191,    -1,    -1,    96
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    53,    54,     0,     3,    13,    24,    55,    57,    58,
      59,    62,    64,    47,    47,    65,    60,    51,    66,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    45,    48,    56,    61,    63,
      67,    70,    73,    76,    79,    82,    83,    84,    85,    86,
      87,    90,    91,    94,    95,    96,    97,    98,    99,   100,
     103,   106,   109,   114,   117,   120,   123,   126,   127,   128,
     129,   130,   135,   138,   139,   140,   141,   142,   143,    50,
      88,    89,    47,    50,   118,   119,    92,    47,    47,    47,
      47,    47,    47,    47,    47,    46,   131,   132,   133,   134,
      46,   136,   137,    49,   124,   125,    50,   121,   122,    47,
      50,   112,   113,    77,    68,    71,    74,    80,    50,   115,
     116,    47,    47,    47,    47,    47,    50,   104,   105,    50,
     107,   108,    50,   110,   111,    47,   144,   145,    50,   101,
     102,    47,    46,   142,    41,    41,    47,    93,    42,    43,
      41,    47,    47,    41,    41,    41,    41,    51,    78,    51,
      69,    51,    72,    51,    75,    51,    81,    41,    41,    41,
      41,    41,    41,    89,   118,    46,   132,   134,   137,   124,
     122,   113,   116,   105,   108,   111,   145,   102
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
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
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
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
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


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

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
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

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
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
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

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
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

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
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

/* Line 1455 of yacc.c  */
#line 60 "levcomp.ypp"
    { }
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 63 "levcomp.ypp"
    {}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 64 "levcomp.ypp"
    {}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 67 "levcomp.ypp"
    {}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 68 "levcomp.ypp"
    {}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 72 "levcomp.ypp"
    {
                    yyerror("Unexpected character sequence.");
                }
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 77 "levcomp.ypp"
    {}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 78 "levcomp.ypp"
    {}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 82 "levcomp.ypp"
    {
                    dgn_reset_default_depth();
                    string err = dgn_set_default_depth((yyvsp[(2) - (2)].text));
                    if (!err.empty())
                        yyerror(make_stringf("Bad default-depth: %s (%s)",
                                (yyvsp[(2) - (2)].text), err.c_str()).c_str());
                }
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 92 "levcomp.ypp"
    {
                    lc_map.set_file(lc_desfile);

                    if (lc_run_global_prelude && !lc_global_prelude.empty())
                    {
                        lc_global_prelude.set_file(lc_desfile);
                        lc_run_global_prelude = false;
                        if (lc_global_prelude.load_call(dlua, NULL))
                            yyerror( lc_global_prelude.orig_error().c_str() );
                    }

                    string err =
                        lc_map.validate_map_def(lc_default_depths);
                    dump_map(lc_map);
                    if (!err.empty())
                        yyerror(err.c_str());
                    add_parsed_map(lc_map);
                }
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 112 "levcomp.ypp"
    { }
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 113 "levcomp.ypp"
    { }
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 116 "levcomp.ypp"
    { }
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 117 "levcomp.ypp"
    { }
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 121 "levcomp.ypp"
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

  case 53:

/* Line 1455 of yacc.c  */
#line 180 "levcomp.ypp"
    {}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 183 "levcomp.ypp"
    { }
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 185 "levcomp.ypp"
    { }
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 186 "levcomp.ypp"
    { }
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 190 "levcomp.ypp"
    {
                    lc_global_prelude.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 194 "levcomp.ypp"
    { }
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 196 "levcomp.ypp"
    { }
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 197 "levcomp.ypp"
    { }
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 201 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 205 "levcomp.ypp"
    { }
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 207 "levcomp.ypp"
    { }
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 208 "levcomp.ypp"
    { }
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 212 "levcomp.ypp"
    {
                    lc_map.validate.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 216 "levcomp.ypp"
    { }
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 218 "levcomp.ypp"
    { }
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 219 "levcomp.ypp"
    { }
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 223 "levcomp.ypp"
    {
                    lc_map.veto.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 227 "levcomp.ypp"
    { }
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 229 "levcomp.ypp"
    { }
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 230 "levcomp.ypp"
    { }
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 234 "levcomp.ypp"
    {
                    lc_map.prelude.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 238 "levcomp.ypp"
    { }
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 240 "levcomp.ypp"
    { }
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 241 "levcomp.ypp"
    { }
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 245 "levcomp.ypp"
    {
                    lc_map.epilogue.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 249 "levcomp.ypp"
    { }
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 251 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kfeat(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 258 "levcomp.ypp"
    { }
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 260 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmons(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 267 "levcomp.ypp"
    { }
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 269 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kitem(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 276 "levcomp.ypp"
    { }
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 278 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmask(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 285 "levcomp.ypp"
    { }
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 287 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kprop(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 294 "levcomp.ypp"
    {}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 302 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("shuffle(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 310 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("clear(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 318 "levcomp.ypp"
    {}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 326 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tags(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 335 "levcomp.ypp"
    {
                    string key, arg;
                    int sep(0);

                    const string err =
                      mapdef_split_key_item((yyvsp[(2) - (2)].text), &key, &sep, &arg);

                    if (!err.empty())
                      yyerror(err.c_str());

                    // Special treatment for Lua markers.
                    if (arg.find("lua:") == 0)
                    {
                       arg = arg.substr(4);
                       lc_map.main.add(
                         yylineno,
                         make_stringf("lua_marker(\"%s\", function () "
                                      "  return %s "
                                      "end)",
                                      quote_lua_string(key).c_str(),
                                      arg.c_str()));
                    }
                    else
                    {
                       lc_map.main.add(
                         yylineno,
                         make_stringf("marker(\"%s\")",
                                      quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                    }
                }
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 367 "levcomp.ypp"
    { }
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 370 "levcomp.ypp"
    { }
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 372 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloorcol(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 379 "levcomp.ypp"
    { }
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 381 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrockcol(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 103:

/* Line 1455 of yacc.c  */
#line 388 "levcomp.ypp"
    { }
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 390 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloortile(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 398 "levcomp.ypp"
    { }
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 400 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrocktile(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 110:

/* Line 1455 of yacc.c  */
#line 415 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("fheight(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 114:

/* Line 1455 of yacc.c  */
#line 431 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("ftile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 118:

/* Line 1455 of yacc.c  */
#line 447 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("rtile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 122:

/* Line 1455 of yacc.c  */
#line 463 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 123:

/* Line 1455 of yacc.c  */
#line 472 "levcomp.ypp"
    { }
    break;

  case 124:

/* Line 1455 of yacc.c  */
#line 473 "levcomp.ypp"
    { }
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 477 "levcomp.ypp"
    {
                      lc_map.main.add(
                          yylineno,
                          make_stringf("colour(\"%s\")",
                              quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                  }
    break;

  case 126:

/* Line 1455 of yacc.c  */
#line 485 "levcomp.ypp"
    { }
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 488 "levcomp.ypp"
    { }
    break;

  case 128:

/* Line 1455 of yacc.c  */
#line 489 "levcomp.ypp"
    { }
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 493 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("nsubst(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 130:

/* Line 1455 of yacc.c  */
#line 501 "levcomp.ypp"
    { }
    break;

  case 133:

/* Line 1455 of yacc.c  */
#line 509 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("subst(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 134:

/* Line 1455 of yacc.c  */
#line 517 "levcomp.ypp"
    {}
    break;

  case 135:

/* Line 1455 of yacc.c  */
#line 518 "levcomp.ypp"
    {}
    break;

  case 138:

/* Line 1455 of yacc.c  */
#line 526 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("item(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 139:

/* Line 1455 of yacc.c  */
#line 533 "levcomp.ypp"
    {}
    break;

  case 140:

/* Line 1455 of yacc.c  */
#line 534 "levcomp.ypp"
    {}
    break;

  case 143:

/* Line 1455 of yacc.c  */
#line 542 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("mons(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 144:

/* Line 1455 of yacc.c  */
#line 551 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("place(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 145:

/* Line 1455 of yacc.c  */
#line 560 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("desc(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 146:

/* Line 1455 of yacc.c  */
#line 569 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("order(%d)", int((yyvsp[(2) - (2)].f))));
                }
    break;

  case 147:

/* Line 1455 of yacc.c  */
#line 576 "levcomp.ypp"
    {}
    break;

  case 148:

/* Line 1455 of yacc.c  */
#line 578 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 149:

/* Line 1455 of yacc.c  */
#line 586 "levcomp.ypp"
    { }
    break;

  case 150:

/* Line 1455 of yacc.c  */
#line 587 "levcomp.ypp"
    { }
    break;

  case 153:

/* Line 1455 of yacc.c  */
#line 594 "levcomp.ypp"
    {
                    (yyval.i) = (yyvsp[(1) - (2)].f) * 100;
                }
    break;

  case 154:

/* Line 1455 of yacc.c  */
#line 598 "levcomp.ypp"
    {
                    (yyval.i) = (yyvsp[(1) - (1)].f);
                }
    break;

  case 155:

/* Line 1455 of yacc.c  */
#line 603 "levcomp.ypp"
    {
                    (yyval.chance).priority = (yyvsp[(1) - (3)].f);
                    (yyval.chance).chance   = (yyvsp[(3) - (3)].i);
                }
    break;

  case 156:

/* Line 1455 of yacc.c  */
#line 608 "levcomp.ypp"
    {
                    (yyval.chance).priority = DEFAULT_CHANCE_PRIORITY;
                    (yyval.chance).chance   = (yyvsp[(1) - (1)].i);
                }
    break;

  case 157:

/* Line 1455 of yacc.c  */
#line 615 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth_chance(\"%s\", %d, %d)",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str(),
                                     (yyvsp[(1) - (2)].chance).priority, (yyvsp[(1) - (2)].chance).chance));
                }
    break;

  case 158:

/* Line 1455 of yacc.c  */
#line 623 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("chance(%d, %d)",
                                     (yyvsp[(1) - (1)].chance).priority, (yyvsp[(1) - (1)].chance).chance));
                }
    break;

  case 162:

/* Line 1455 of yacc.c  */
#line 637 "levcomp.ypp"
    {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("depth_weight(\"%s\", %d)",
                                      quote_lua_string((yyvsp[(2) - (2)].text)).c_str(), (int)(yyvsp[(1) - (2)].f)));
                 }
    break;

  case 163:

/* Line 1455 of yacc.c  */
#line 644 "levcomp.ypp"
    {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("weight(%d)", (int)(yyvsp[(1) - (1)].f)));
                 }
    break;

  case 164:

/* Line 1455 of yacc.c  */
#line 651 "levcomp.ypp"
    {}
    break;

  case 165:

/* Line 1455 of yacc.c  */
#line 653 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("orient(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 166:

/* Line 1455 of yacc.c  */
#line 662 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("welcome(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 170:

/* Line 1455 of yacc.c  */
#line 678 "levcomp.ypp"
    {
                    lc_map.mapchunk.add(
                        yylineno,
                        make_stringf("map(\"%s\")",
                                     quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 174:

/* Line 1455 of yacc.c  */
#line 694 "levcomp.ypp"
    {
                       lc_map.main.add(
                           yylineno,
                           make_stringf("subvault(\"%s\")",
                               quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                   }
    break;



/* Line 1455 of yacc.c  */
#line 2676 "levcomp.tab.cc"
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
      /* If just tried and failed to reuse lookahead token after an
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

  /* Else will try to reuse lookahead token after shifting the error
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

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
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



/* Line 1675 of yacc.c  */
#line 702 "levcomp.ypp"


