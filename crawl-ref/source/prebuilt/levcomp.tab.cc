
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


#include <map>
#include <algorithm>

#include "AppHdr.h"
#include "l_defs.h"
#include "libutil.h"
#include "mapdef.h"
#include "maps.h"
#include "stuff.h"

#define YYERROR_VERBOSE 1

int yylex();

extern int yylineno;

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



/* Line 189 of yacc.c  */
#line 117 "levcomp.tab.c"

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
     SUBST = 260,
     TAGS = 261,
     KFEAT = 262,
     KITEM = 263,
     KMONS = 264,
     KMASK = 265,
     KPROP = 266,
     NAME = 267,
     DEPTH = 268,
     ORIENT = 269,
     PLACE = 270,
     CHANCE = 271,
     WEIGHT = 272,
     MONS = 273,
     ITEM = 274,
     MARKER = 275,
     COLOUR = 276,
     PRELUDE = 277,
     MAIN = 278,
     VALIDATE = 279,
     VETO = 280,
     EPILOGUE = 281,
     NSUBST = 282,
     WELCOME = 283,
     LFLAGS = 284,
     BFLAGS = 285,
     LFLOORCOL = 286,
     LROCKCOL = 287,
     LFLOORTILE = 288,
     LROCKTILE = 289,
     FTILE = 290,
     RTILE = 291,
     TILE = 292,
     SUBVAULT = 293,
     FHEIGHT = 294,
     DESC = 295,
     COMMA = 296,
     COLON = 297,
     PERC = 298,
     INTEGER = 299,
     CHARACTER = 300,
     STRING = 301,
     MAP_LINE = 302,
     MONSTER_NAME = 303,
     ITEM_INFO = 304,
     LUA_LINE = 305
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
#define KPROP 266
#define NAME 267
#define DEPTH 268
#define ORIENT 269
#define PLACE 270
#define CHANCE 271
#define WEIGHT 272
#define MONS 273
#define ITEM 274
#define MARKER 275
#define COLOUR 276
#define PRELUDE 277
#define MAIN 278
#define VALIDATE 279
#define VETO 280
#define EPILOGUE 281
#define NSUBST 282
#define WELCOME 283
#define LFLAGS 284
#define BFLAGS 285
#define LFLOORCOL 286
#define LROCKCOL 287
#define LFLOORTILE 288
#define LROCKTILE 289
#define FTILE 290
#define RTILE 291
#define TILE 292
#define SUBVAULT 293
#define FHEIGHT 294
#define DESC 295
#define COMMA 296
#define COLON 297
#define PERC 298
#define INTEGER 299
#define CHARACTER 300
#define STRING 301
#define MAP_LINE 302
#define MONSTER_NAME 303
#define ITEM_INFO 304
#define LUA_LINE 305




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 45 "levcomp.ypp"

    int i;
    const char *text;
    raw_range range;



/* Line 214 of yacc.c  */
#line 261 "levcomp.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 273 "levcomp.tab.c"

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
#define YYLAST   115

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  51
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  88
/* YYNRULES -- Number of rules.  */
#define YYNRULES  164
/* YYNRULES -- Number of states.  */
#define YYNSTATES  196

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   305

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
      45,    46,    47,    48,    49,    50
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
     194,   198,   200,   203,   204,   207,   209,   212,   215,   218,
     221,   223,   226,   228,   231,   233,   236,   238,   241,   244,
     246,   250,   252,   255,   257,   261,   263,   266,   268,   272,
     274,   277,   279,   283,   285,   287,   291,   293,   296,   298,
     302,   304,   307,   309,   313,   315,   317,   320,   324,   326,
     328,   330,   333,   337,   339,   341,   344,   347,   349,   352,
     358,   363,   367,   370,   373,   375,   378,   381,   383,   386,
     388,   390,   393,   395,   399
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
      52,     0,    -1,    53,    -1,    -1,    53,    54,    -1,    56,
      -1,    58,    -1,    45,    -1,    57,    -1,    63,    -1,     3,
      46,    -1,    61,    59,    -1,    -1,    59,    60,    -1,    62,
      -1,   133,    -1,    12,    46,    -1,   126,    -1,   127,    -1,
     128,    -1,   129,    -1,   130,    -1,   131,    -1,   132,    -1,
     123,    -1,   120,    -1,    94,    -1,   117,    -1,   114,    -1,
      95,    -1,    96,    -1,    97,    -1,    98,    -1,    99,    -1,
     103,    -1,   106,    -1,   109,    -1,    86,    -1,    89,    -1,
      92,    -1,    93,    -1,    81,    -1,    83,    -1,    82,    -1,
      84,    -1,    85,    -1,   100,    -1,   136,    -1,    66,    -1,
      75,    -1,    69,    -1,    72,    -1,    78,    -1,    55,    -1,
      23,    64,    -1,    -1,    64,    65,    -1,    50,    -1,    23,
      67,    -1,    -1,    67,    68,    -1,    50,    -1,    24,    70,
      -1,    -1,    70,    71,    -1,    50,    -1,    25,    73,    -1,
      -1,    73,    74,    -1,    50,    -1,    22,    76,    -1,    -1,
      76,    77,    -1,    50,    -1,    26,    79,    -1,    -1,    79,
      80,    -1,    50,    -1,     7,    -1,     7,    46,    -1,     9,
      -1,     9,    46,    -1,     8,    -1,     8,    46,    -1,    10,
      -1,    10,    46,    -1,    11,    -1,    11,    46,    -1,     4,
      87,    -1,    88,    -1,    87,    41,    88,    -1,    49,    -1,
       6,    90,    -1,    -1,    90,    91,    -1,    46,    -1,    29,
      46,    -1,    30,    46,    -1,    20,    46,    -1,    21,   112,
      -1,    31,    -1,    31,    46,    -1,    32,    -1,    32,    46,
      -1,    33,    -1,    33,    46,    -1,    34,    -1,    34,    46,
      -1,    39,   101,    -1,   102,    -1,   101,    41,   102,    -1,
      49,    -1,    35,   104,    -1,   105,    -1,   104,    41,   105,
      -1,    49,    -1,    36,   107,    -1,   108,    -1,   107,    41,
     108,    -1,    49,    -1,    37,   110,    -1,   111,    -1,   110,
      41,   111,    -1,    49,    -1,   113,    -1,   112,    41,   113,
      -1,    49,    -1,    27,   115,    -1,   116,    -1,   115,    41,
     116,    -1,    49,    -1,     5,   118,    -1,   119,    -1,   119,
      41,   118,    -1,    49,    -1,    19,    -1,    19,   121,    -1,
     121,    41,   122,    -1,   122,    -1,    49,    -1,    18,    -1,
      18,   124,    -1,   125,    41,   124,    -1,   125,    -1,    48,
      -1,    15,    46,    -1,    40,    46,    -1,    13,    -1,    13,
      46,    -1,    16,    44,    42,    44,    43,    -1,    16,    44,
      42,    44,    -1,    16,    44,    43,    -1,    16,    44,    -1,
      17,    44,    -1,    14,    -1,    14,    46,    -1,    28,    46,
      -1,   134,    -1,   134,   135,    -1,   135,    -1,    47,    -1,
      38,   137,    -1,   138,    -1,   137,    41,   138,    -1,    46,
      -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    67,    67,    70,    71,    74,    75,    78,    84,    85,
      88,    98,   118,   119,   122,   123,   126,   150,   151,   152,
     153,   154,   155,   156,   157,   158,   159,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,   181,   182,
     183,   184,   185,   186,   189,   191,   192,   195,   200,   202,
     203,   206,   211,   213,   214,   217,   222,   224,   225,   228,
     233,   235,   236,   239,   244,   246,   247,   250,   255,   256,
     264,   265,   273,   274,   282,   283,   291,   292,   300,   303,
     304,   307,   315,   318,   319,   322,   331,   340,   349,   382,
     385,   386,   394,   395,   403,   404,   413,   414,   423,   425,
     426,   429,   438,   441,   442,   445,   454,   457,   458,   461,
     470,   473,   474,   477,   487,   488,   491,   500,   503,   504,
     507,   516,   519,   520,   523,   532,   533,   536,   537,   540,
     548,   549,   552,   553,   556,   565,   574,   583,   584,   593,
     600,   607,   614,   622,   630,   631,   640,   649,   652,   653,
     656,   665,   668,   669,   672
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "DEFAULT_DEPTH", "SHUFFLE", "SUBST",
  "TAGS", "KFEAT", "KITEM", "KMONS", "KMASK", "KPROP", "NAME", "DEPTH",
  "ORIENT", "PLACE", "CHANCE", "WEIGHT", "MONS", "ITEM", "MARKER",
  "COLOUR", "PRELUDE", "MAIN", "VALIDATE", "VETO", "EPILOGUE", "NSUBST",
  "WELCOME", "LFLAGS", "BFLAGS", "LFLOORCOL", "LROCKCOL", "LFLOORTILE",
  "LROCKTILE", "FTILE", "RTILE", "TILE", "SUBVAULT", "FHEIGHT", "DESC",
  "COMMA", "COLON", "PERC", "INTEGER", "CHARACTER", "STRING", "MAP_LINE",
  "MONSTER_NAME", "ITEM_INFO", "LUA_LINE", "$accept", "file",
  "definitions", "definition", "error_seq", "def", "defdepth", "level",
  "map_specs", "map_spec", "name", "metaline", "global_lua",
  "global_lua_lines", "global_lua_line", "main_lua", "main_lua_lines",
  "main_lua_line", "validate_lua", "validate_lua_lines",
  "validate_lua_line", "veto_lua", "veto_lua_lines", "veto_lua_line",
  "prelude_lua", "prelude_lua_lines", "prelude_lua_line", "epilogue_lua",
  "epilogue_lua_lines", "epilogue_lua_line", "kfeat", "kmons", "kitem",
  "kmask", "kprop", "shuffle", "shuffle_specifiers", "shuffle_spec",
  "tags", "tagstrings", "tagstring", "lflags", "bflags", "marker",
  "colour", "lfloorcol", "lrockcol", "lfloortile", "lrocktile", "fheight",
  "fheight_specifiers", "fheight_specifier", "ftile", "ftile_specifiers",
  "ftile_specifier", "rtile", "rtile_specifiers", "rtile_specifier",
  "tile", "tile_specifiers", "tile_specifier", "colour_specifiers",
  "colour_specifier", "nsubst", "nsubst_specifiers", "nsubst_spec",
  "subst", "subst_specifiers", "subst_spec", "items", "item_specifiers",
  "item_specifier", "mons", "mnames", "mname", "place", "desc", "depth",
  "chance", "weight", "orientation", "welcome", "map_def", "map_lines",
  "map_line", "subvault", "subvault_specifiers", "subvault_specifier", 0
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
     305
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    51,    52,    53,    53,    54,    54,    55,    56,    56,
      57,    58,    59,    59,    60,    60,    61,    62,    62,    62,
      62,    62,    62,    62,    62,    62,    62,    62,    62,    62,
      62,    62,    62,    62,    62,    62,    62,    62,    62,    62,
      62,    62,    62,    62,    62,    62,    62,    62,    62,    62,
      62,    62,    62,    62,    63,    64,    64,    65,    66,    67,
      67,    68,    69,    70,    70,    71,    72,    73,    73,    74,
      75,    76,    76,    77,    78,    79,    79,    80,    81,    81,
      82,    82,    83,    83,    84,    84,    85,    85,    86,    87,
      87,    88,    89,    90,    90,    91,    92,    93,    94,    95,
      96,    96,    97,    97,    98,    98,    99,    99,   100,   101,
     101,   102,   103,   104,   104,   105,   106,   107,   107,   108,
     109,   110,   110,   111,   112,   112,   113,   114,   115,   115,
     116,   117,   118,   118,   119,   120,   120,   121,   121,   122,
     123,   123,   124,   124,   125,   126,   127,   128,   128,   129,
     129,   129,   129,   130,   131,   131,   132,   133,   134,   134,
     135,   136,   137,   137,   138
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
       3,     1,     2,     0,     2,     1,     2,     2,     2,     2,
       1,     2,     1,     2,     1,     2,     1,     2,     2,     1,
       3,     1,     2,     1,     3,     1,     2,     1,     3,     1,
       2,     1,     3,     1,     1,     3,     1,     2,     1,     3,
       1,     2,     1,     3,     1,     1,     2,     3,     1,     1,
       1,     2,     3,     1,     1,     2,     2,     1,     2,     5,
       4,     3,     2,     2,     1,     2,     2,     1,     2,     1,
       1,     2,     1,     3,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,     0,     0,    55,     4,     5,     8,
       6,    12,     9,    10,    16,    54,    11,    57,    56,     0,
       0,    93,    78,    82,    80,    84,    86,   147,   154,     0,
       0,     0,   140,   135,     0,     0,    71,    59,    63,    67,
      75,     0,     0,     0,     0,   100,   102,   104,   106,     0,
       0,     0,     0,     0,     0,     7,   160,    53,    13,    14,
      48,    50,    51,    49,    52,    41,    43,    42,    44,    45,
      37,    38,    39,    40,    26,    29,    30,    31,    32,    33,
      46,    34,    35,    36,    28,    27,    25,    24,    17,    18,
      19,    20,    21,    22,    23,    15,   157,   159,    47,    91,
      88,    89,   134,   131,   132,    92,    79,    83,    81,    85,
      87,   148,   155,   145,   152,   153,   144,   141,   143,   139,
     136,   138,    98,   126,    99,   124,    70,    58,    62,    66,
      74,   130,   127,   128,   156,    96,    97,   101,   103,   105,
     107,   115,   112,   113,   119,   116,   117,   123,   120,   121,
     164,   161,   162,   111,   108,   109,   146,   158,     0,     0,
      95,    94,     0,   151,     0,     0,     0,    73,    72,    61,
      60,    65,    64,    69,    68,    77,    76,     0,     0,     0,
       0,     0,     0,    90,   133,   150,   142,   137,   125,   129,
     114,   118,   122,   163,   110,   149
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     7,    57,     8,     9,    10,    16,    58,
      11,    59,    12,    15,    18,    60,   127,   170,    61,   128,
     172,    62,   129,   174,    63,   126,   168,    64,   130,   176,
      65,    66,    67,    68,    69,    70,   100,   101,    71,   105,
     161,    72,    73,    74,    75,    76,    77,    78,    79,    80,
     154,   155,    81,   142,   143,    82,   145,   146,    83,   148,
     149,   124,   125,    84,   132,   133,    85,   103,   104,    86,
     120,   121,    87,   117,   118,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,   151,   152
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -108
static const yytype_int8 yypact[] =
{
    -108,     8,    34,  -108,    -8,    -2,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,    -5,    -4,  -108,  -108,    -7,
      -1,  -108,     1,     3,     4,     5,     6,     7,     9,    10,
      14,    15,    12,    13,    17,    16,  -108,  -108,  -108,  -108,
    -108,    18,    20,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    35,    31,    36,  -108,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,  -108,    37,  -108,  -108,  -108,
      33,  -108,  -108,  -108,    42,    39,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,    -3,  -108,  -108,  -108,    45,  -108,
      46,  -108,  -108,  -108,    47,  -108,    11,    40,    41,    43,
      44,  -108,    48,  -108,  -108,  -108,  -108,  -108,  -108,  -108,
    -108,  -108,    51,  -108,  -108,    54,  -108,  -108,    55,  -108,
    -108,    56,  -108,  -108,    57,  -108,  -108,  -108,    -7,    -1,
    -108,  -108,    32,  -108,    12,    13,    16,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,  -108,  -108,    18,    28,    29,
      30,    35,    31,  -108,  -108,    21,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,  -108
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
    -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,  -108,  -108,  -104,  -108,  -108,
    -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,  -108,
    -108,  -107,  -108,  -108,   -79,  -108,  -108,   -78,  -108,  -108,
     -80,  -108,   -64,  -108,  -108,   -74,  -108,   -55,  -108,  -108,
    -108,   -60,  -108,   -58,  -108,  -108,  -108,  -108,  -108,  -108,
    -108,  -108,  -108,  -108,    19,  -108,  -108,   -73
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      19,    20,    21,    22,    23,    24,    25,    26,     3,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,     4,    13,   162,
     163,    55,    99,    56,    14,    17,     5,   106,   102,   107,
     108,   109,   110,   111,   183,   112,   113,     6,   114,   115,
     116,   167,   119,   122,   195,   123,   134,   131,   135,   136,
     137,   138,   139,   140,   158,   194,   185,   141,   144,   147,
     153,   150,   156,   159,    56,   160,   164,   165,   166,   177,
     169,   171,   178,   173,   175,   179,   180,   181,   182,   190,
     192,   191,   188,   189,   184,   187,   186,     0,   193,     0,
       0,     0,     0,     0,     0,   157
};

static const yytype_int16 yycheck[] =
{
       4,     5,     6,     7,     8,     9,    10,    11,     0,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,     3,    46,    42,
      43,    45,    49,    47,    46,    50,    12,    46,    49,    46,
      46,    46,    46,    46,   158,    46,    46,    23,    44,    44,
      48,    50,    49,    46,    43,    49,    46,    49,    46,    46,
      46,    46,    46,    46,    41,   182,    44,    49,    49,    49,
      49,    46,    46,    41,    47,    46,    41,    41,    41,    41,
      50,    50,    41,    50,    50,    41,    41,    41,    41,   178,
     180,   179,   166,   177,   159,   165,   164,    -1,   181,    -1,
      -1,    -1,    -1,    -1,    -1,    96
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    52,    53,     0,     3,    12,    23,    54,    56,    57,
      58,    61,    63,    46,    46,    64,    59,    50,    65,     4,
       5,     6,     7,     8,     9,    10,    11,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    45,    47,    55,    60,    62,
      66,    69,    72,    75,    78,    81,    82,    83,    84,    85,
      86,    89,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   103,   106,   109,   114,   117,   120,   123,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,    49,
      87,    88,    49,   118,   119,    90,    46,    46,    46,    46,
      46,    46,    46,    46,    44,    44,    48,   124,   125,    49,
     121,   122,    46,    49,   112,   113,    76,    67,    70,    73,
      79,    49,   115,   116,    46,    46,    46,    46,    46,    46,
      46,    49,   104,   105,    49,   107,   108,    49,   110,   111,
      46,   137,   138,    49,   101,   102,    46,   135,    41,    41,
      46,    91,    42,    43,    41,    41,    41,    50,    77,    50,
      68,    50,    71,    50,    74,    50,    80,    41,    41,    41,
      41,    41,    41,    88,   118,    44,   124,   122,   113,   116,
     105,   108,   111,   138,   102,    43
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
#line 67 "levcomp.ypp"
    { }
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 70 "levcomp.ypp"
    {}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 71 "levcomp.ypp"
    {}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 74 "levcomp.ypp"
    {}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 75 "levcomp.ypp"
    {}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 79 "levcomp.ypp"
    {
                    yyerror("Unexpected character sequence.");
                }
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 84 "levcomp.ypp"
    {}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 85 "levcomp.ypp"
    {}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 89 "levcomp.ypp"
    {
                    dgn_reset_default_depth();
                    std::string err = dgn_set_default_depth((yyvsp[(2) - (2)].text));
                    if (!err.empty())
                        yyerror(make_stringf("Bad default-depth: %s (%s)",
                                (yyvsp[(2) - (2)].text), err.c_str()).c_str());
                }
    break;

  case 11:

/* Line 1455 of yacc.c  */
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

                    std::string err =
                        lc_map.validate_map_def(lc_default_depths);
                    if (!err.empty())
                        yyerror(err.c_str());
                    add_parsed_map(lc_map);
                }
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 118 "levcomp.ypp"
    { }
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 119 "levcomp.ypp"
    { }
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 122 "levcomp.ypp"
    { }
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 123 "levcomp.ypp"
    { }
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 127 "levcomp.ypp"
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
#line 186 "levcomp.ypp"
    {}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 189 "levcomp.ypp"
    { }
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 191 "levcomp.ypp"
    { }
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 192 "levcomp.ypp"
    { }
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 196 "levcomp.ypp"
    {
                    lc_global_prelude.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 200 "levcomp.ypp"
    { }
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 202 "levcomp.ypp"
    { }
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 203 "levcomp.ypp"
    { }
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 207 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 211 "levcomp.ypp"
    { }
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 213 "levcomp.ypp"
    { }
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 214 "levcomp.ypp"
    { }
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 218 "levcomp.ypp"
    {
                    lc_map.validate.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 222 "levcomp.ypp"
    { }
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 224 "levcomp.ypp"
    { }
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 225 "levcomp.ypp"
    { }
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 229 "levcomp.ypp"
    {
                    lc_map.veto.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 233 "levcomp.ypp"
    { }
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 235 "levcomp.ypp"
    { }
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 236 "levcomp.ypp"
    { }
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 240 "levcomp.ypp"
    {
                    lc_map.prelude.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 244 "levcomp.ypp"
    { }
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 246 "levcomp.ypp"
    { }
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 247 "levcomp.ypp"
    { }
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 251 "levcomp.ypp"
    {
                    lc_map.epilogue.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 255 "levcomp.ypp"
    { }
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 257 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kfeat(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 264 "levcomp.ypp"
    { }
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 266 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmons(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 273 "levcomp.ypp"
    { }
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 275 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kitem(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 282 "levcomp.ypp"
    { }
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 284 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmask(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 291 "levcomp.ypp"
    { }
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 293 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kprop(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 300 "levcomp.ypp"
    {}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 308 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("shuffle(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 315 "levcomp.ypp"
    {}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 323 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tags(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 332 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lflags(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 341 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("bflags(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 350 "levcomp.ypp"
    {
                    std::string key, arg;
                    int sep(0);

                    const std::string err =
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

  case 99:

/* Line 1455 of yacc.c  */
#line 382 "levcomp.ypp"
    { }
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 385 "levcomp.ypp"
    { }
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 387 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloorcol(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 394 "levcomp.ypp"
    { }
    break;

  case 103:

/* Line 1455 of yacc.c  */
#line 396 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrockcol(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 403 "levcomp.ypp"
    { }
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 405 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloortile(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 413 "levcomp.ypp"
    { }
    break;

  case 107:

/* Line 1455 of yacc.c  */
#line 415 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrocktile(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 111:

/* Line 1455 of yacc.c  */
#line 430 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("fheight(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 115:

/* Line 1455 of yacc.c  */
#line 446 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("ftile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 119:

/* Line 1455 of yacc.c  */
#line 462 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("rtile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 123:

/* Line 1455 of yacc.c  */
#line 478 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 124:

/* Line 1455 of yacc.c  */
#line 487 "levcomp.ypp"
    { }
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 488 "levcomp.ypp"
    { }
    break;

  case 126:

/* Line 1455 of yacc.c  */
#line 492 "levcomp.ypp"
    {
                      lc_map.main.add(
                          yylineno,
                          make_stringf("colour(\"%s\")",
                              quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                  }
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 500 "levcomp.ypp"
    { }
    break;

  case 128:

/* Line 1455 of yacc.c  */
#line 503 "levcomp.ypp"
    { }
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 504 "levcomp.ypp"
    { }
    break;

  case 130:

/* Line 1455 of yacc.c  */
#line 508 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("nsubst(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 131:

/* Line 1455 of yacc.c  */
#line 516 "levcomp.ypp"
    { }
    break;

  case 134:

/* Line 1455 of yacc.c  */
#line 524 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("subst(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 135:

/* Line 1455 of yacc.c  */
#line 532 "levcomp.ypp"
    {}
    break;

  case 136:

/* Line 1455 of yacc.c  */
#line 533 "levcomp.ypp"
    {}
    break;

  case 139:

/* Line 1455 of yacc.c  */
#line 541 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("item(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 140:

/* Line 1455 of yacc.c  */
#line 548 "levcomp.ypp"
    {}
    break;

  case 141:

/* Line 1455 of yacc.c  */
#line 549 "levcomp.ypp"
    {}
    break;

  case 144:

/* Line 1455 of yacc.c  */
#line 557 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("mons(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 145:

/* Line 1455 of yacc.c  */
#line 566 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("place(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 146:

/* Line 1455 of yacc.c  */
#line 575 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("desc(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 147:

/* Line 1455 of yacc.c  */
#line 583 "levcomp.ypp"
    {}
    break;

  case 148:

/* Line 1455 of yacc.c  */
#line 585 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 149:

/* Line 1455 of yacc.c  */
#line 594 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("chance(%d, %d)", (yyvsp[(2) - (5)].i), (yyvsp[(4) - (5)].i) * 100));
                }
    break;

  case 150:

/* Line 1455 of yacc.c  */
#line 601 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("chance(%d, %d)", (yyvsp[(2) - (4)].i), (yyvsp[(4) - (4)].i)));
                }
    break;

  case 151:

/* Line 1455 of yacc.c  */
#line 608 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("chance(%d)", (yyvsp[(2) - (3)].i) * 100));
                }
    break;

  case 152:

/* Line 1455 of yacc.c  */
#line 615 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("chance(%d)", (yyvsp[(2) - (2)].i)));
                }
    break;

  case 153:

/* Line 1455 of yacc.c  */
#line 623 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("weight(%d)", (yyvsp[(2) - (2)].i)));
                }
    break;

  case 154:

/* Line 1455 of yacc.c  */
#line 630 "levcomp.ypp"
    {}
    break;

  case 155:

/* Line 1455 of yacc.c  */
#line 632 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("orient(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 156:

/* Line 1455 of yacc.c  */
#line 641 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("welcome(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 160:

/* Line 1455 of yacc.c  */
#line 657 "levcomp.ypp"
    {
                    lc_map.mapchunk.add(
                        yylineno,
                        make_stringf("map(\"%s\")",
                                     quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 164:

/* Line 1455 of yacc.c  */
#line 673 "levcomp.ypp"
    {
                       lc_map.main.add(
                           yylineno,
                           make_stringf("subvault(\"%s\")",
                               quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                   }
    break;



/* Line 1455 of yacc.c  */
#line 2626 "levcomp.tab.c"
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
#line 681 "levcomp.ypp"


