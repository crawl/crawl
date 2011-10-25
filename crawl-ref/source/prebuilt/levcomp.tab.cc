/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
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
#define YYBISON_VERSION "2.5"

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

/* Line 268 of yacc.c  */
#line 1 "levcomp.ypp"



#include "AppHdr.h"
#include <map>
#include <algorithm>
#include "l_defs.h"
#include "libutil.h"
#include "mapdef.h"
#include "maps.h"
#include "stuff.h"

#define YYERROR_VERBOSE 1
#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 0

int yylex();

extern int yylineno;

NORETURN void yyerror(const char *e)
{
    if (strstr(e, lc_desfile.c_str()) == e)
        fprintf(stderr, "%s\n", e);
    else
        fprintf(stderr, "%s:%d: %s\n", lc_desfile.c_str(), yylineno, e);
    // Bail bail bail.
    end(1);
}



/* Line 268 of yacc.c  */
#line 104 "levcomp.tab.c"

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

/* Line 293 of yacc.c  */
#line 34 "levcomp.ypp"

    int i;
    double f;
    const char *text;
    map_chance_pair chance;



/* Line 293 of yacc.c  */
#line 251 "levcomp.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 263 "levcomp.tab.c"

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
# if defined YYENABLE_NLS && YYENABLE_NLS
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
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

# define YYCOPY_NEEDED 1

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

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
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
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   116

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
     194,   198,   200,   203,   204,   207,   209,   212,   215,   218,
     221,   223,   226,   228,   231,   233,   236,   238,   241,   244,
     246,   250,   252,   255,   257,   261,   263,   266,   268,   272,
     274,   277,   279,   283,   285,   287,   291,   293,   296,   298,
     302,   304,   307,   309,   313,   315,   317,   320,   324,   326,
     328,   330,   333,   337,   339,   341,   344,   347,   349,   352,
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
      -1,   140,    -1,    12,    47,    -1,   127,    -1,   128,    -1,
     129,    -1,   130,    -1,   135,    -1,   138,    -1,   139,    -1,
     124,    -1,   121,    -1,    95,    -1,   118,    -1,   115,    -1,
      96,    -1,    97,    -1,    98,    -1,    99,    -1,   100,    -1,
     104,    -1,   107,    -1,   110,    -1,    87,    -1,    90,    -1,
      93,    -1,    94,    -1,    82,    -1,    84,    -1,    83,    -1,
      85,    -1,    86,    -1,   101,    -1,   143,    -1,    67,    -1,
      76,    -1,    70,    -1,    73,    -1,    79,    -1,    56,    -1,
      23,    65,    -1,    -1,    65,    66,    -1,    51,    -1,    23,
      68,    -1,    -1,    68,    69,    -1,    51,    -1,    24,    71,
      -1,    -1,    71,    72,    -1,    51,    -1,    25,    74,    -1,
      -1,    74,    75,    -1,    51,    -1,    22,    77,    -1,    -1,
      77,    78,    -1,    51,    -1,    26,    80,    -1,    -1,    80,
      81,    -1,    51,    -1,     7,    -1,     7,    47,    -1,     9,
      -1,     9,    47,    -1,     8,    -1,     8,    47,    -1,    10,
      -1,    10,    47,    -1,    11,    -1,    11,    47,    -1,     4,
      88,    -1,    89,    -1,    88,    41,    89,    -1,    50,    -1,
       6,    91,    -1,    -1,    91,    92,    -1,    47,    -1,    29,
      47,    -1,    30,    47,    -1,    20,    47,    -1,    21,   113,
      -1,    31,    -1,    31,    47,    -1,    32,    -1,    32,    47,
      -1,    33,    -1,    33,    47,    -1,    34,    -1,    34,    47,
      -1,    39,   102,    -1,   103,    -1,   102,    41,   103,    -1,
      50,    -1,    35,   105,    -1,   106,    -1,   105,    41,   106,
      -1,    50,    -1,    36,   108,    -1,   109,    -1,   108,    41,
     109,    -1,    50,    -1,    37,   111,    -1,   112,    -1,   111,
      41,   112,    -1,    50,    -1,   114,    -1,   113,    41,   114,
      -1,    50,    -1,    27,   116,    -1,   117,    -1,   116,    41,
     117,    -1,    50,    -1,     5,   119,    -1,   120,    -1,   120,
      41,   119,    -1,    50,    -1,    19,    -1,    19,   122,    -1,
     122,    41,   123,    -1,   123,    -1,    50,    -1,    18,    -1,
      18,   125,    -1,   126,    41,   125,    -1,   126,    -1,    49,
      -1,    15,    47,    -1,    40,    47,    -1,    13,    -1,    13,
      47,    -1,    16,   131,    -1,    16,    -1,   131,    41,   134,
      -1,   134,    -1,    46,    43,    -1,    46,    -1,    46,    42,
     132,    -1,   132,    -1,   133,    47,    -1,   133,    -1,    17,
     136,    -1,   136,    41,   137,    -1,   137,    -1,    46,    47,
      -1,    46,    -1,    14,    -1,    14,    47,    -1,    28,    47,
      -1,   141,    -1,   141,   142,    -1,   142,    -1,    48,    -1,
      38,   144,    -1,   145,    -1,   144,    41,   145,    -1,    47,
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
     298,   301,   309,   312,   313,   316,   325,   334,   343,   376,
     379,   380,   388,   389,   397,   398,   407,   408,   417,   419,
     420,   423,   432,   435,   436,   439,   448,   451,   452,   455,
     464,   467,   468,   471,   481,   482,   485,   494,   497,   498,
     501,   510,   513,   514,   517,   526,   527,   530,   531,   534,
     542,   543,   546,   547,   550,   559,   568,   577,   578,   587,
     588,   591,   592,   594,   598,   603,   608,   615,   623,   632,
     634,   635,   637,   644,   652,   653,   662,   671,   674,   675,
     678,   687,   690,   691,   694
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
  "COMMA", "COLON", "PERC", "DASH", "CHARACTER", "NUMBER", "STRING",
  "MAP_LINE", "MONSTER_NAME", "ITEM_INFO", "LUA_LINE", "$accept", "file",
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
  "chance", "chance_specifiers", "chance_roll", "chance_num",
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
      88,    89,    90,    91,    91,    92,    93,    94,    95,    96,
      97,    97,    98,    98,    99,    99,   100,   100,   101,   102,
     102,   103,   104,   105,   105,   106,   107,   108,   108,   109,
     110,   111,   111,   112,   113,   113,   114,   115,   116,   116,
     117,   118,   119,   119,   120,   121,   121,   122,   122,   123,
     124,   124,   125,   125,   126,   127,   128,   129,   129,   130,
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
       3,     1,     2,     0,     2,     1,     2,     2,     2,     2,
       1,     2,     1,     2,     1,     2,     1,     2,     2,     1,
       3,     1,     2,     1,     3,     1,     2,     1,     3,     1,
       2,     1,     3,     1,     1,     3,     1,     2,     1,     3,
       1,     2,     1,     3,     1,     1,     2,     3,     1,     1,
       1,     2,     3,     1,     1,     2,     2,     1,     2,     2,
       1,     3,     1,     2,     1,     3,     1,     2,     1,     2,
       3,     1,     2,     1,     1,     2,     2,     1,     2,     1,
       1,     2,     1,     3,     1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,     0,     0,    55,     4,     5,     8,
       6,    12,     9,    10,    16,    54,    11,    57,    56,     0,
       0,    93,    78,    82,    80,    84,    86,   147,   164,     0,
     150,     0,   140,   135,     0,     0,    71,    59,    63,    67,
      75,     0,     0,     0,     0,   100,   102,   104,   106,     0,
       0,     0,     0,     0,     0,     7,   170,    53,    13,    14,
      48,    50,    51,    49,    52,    41,    43,    42,    44,    45,
      37,    38,    39,    40,    26,    29,    30,    31,    32,    33,
      46,    34,    35,    36,    28,    27,    25,    24,    17,    18,
      19,    20,    21,    22,    23,    15,   167,   169,    47,    91,
      88,    89,   134,   131,   132,    92,    79,    83,    81,    85,
      87,   148,   165,   145,   154,   149,   156,   158,   152,   163,
     159,   161,   144,   141,   143,   139,   136,   138,    98,   126,
      99,   124,    70,    58,    62,    66,    74,   130,   127,   128,
     166,    96,    97,   101,   103,   105,   107,   115,   112,   113,
     119,   116,   117,   123,   120,   121,   174,   171,   172,   111,
     108,   109,   146,   168,     0,     0,    95,    94,     0,   153,
       0,   157,   162,     0,     0,     0,     0,    73,    72,    61,
      60,    65,    64,    69,    68,    77,    76,     0,     0,     0,
       0,     0,     0,    90,   133,   154,   155,   151,   160,   142,
     137,   125,   129,   114,   118,   122,   173,   110
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     7,    57,     8,     9,    10,    16,    58,
      11,    59,    12,    15,    18,    60,   133,   180,    61,   134,
     182,    62,   135,   184,    63,   132,   178,    64,   136,   186,
      65,    66,    67,    68,    69,    70,   100,   101,    71,   105,
     167,    72,    73,    74,    75,    76,    77,    78,    79,    80,
     160,   161,    81,   148,   149,    82,   151,   152,    83,   154,
     155,   130,   131,    84,   138,   139,    85,   103,   104,    86,
     126,   127,    87,   123,   124,    88,    89,    90,    91,   115,
     116,   117,   118,    92,   120,   121,    93,    94,    95,    96,
      97,    98,   157,   158
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -129
static const yytype_int16 yypact[] =
{
    -129,     8,    34,  -129,    -9,    -5,  -129,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,    -8,    -4,  -129,  -129,    -2,
      -1,  -129,     0,     3,     4,     5,     6,     7,     9,    11,
      13,    14,    12,    15,    16,    17,  -129,  -129,  -129,  -129,
    -129,    18,    19,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    35,    31,    36,  -129,  -129,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,    37,  -129,  -129,  -129,
      21,  -129,  -129,  -129,    43,    39,  -129,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,    -3,    46,  -129,    41,  -129,    42,
      49,  -129,  -129,  -129,    50,  -129,    51,  -129,  -129,  -129,
      52,  -129,    -6,    44,    45,    47,    48,  -129,    53,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,    56,  -129,
    -129,    59,  -129,  -129,    60,  -129,  -129,    61,  -129,  -129,
      62,  -129,  -129,  -129,    -2,    -1,  -129,  -129,    58,  -129,
      13,  -129,  -129,    14,    12,    15,    17,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,  -129,    18,    28,    29,
      30,    35,    31,  -129,  -129,    32,  -129,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,  -129,  -109,  -129,  -129,
    -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,  -129,
    -129,  -128,  -129,  -129,  -112,  -129,  -129,   -84,  -129,  -129,
    -113,  -129,   -70,  -129,  -129,   -80,  -129,   -57,  -129,  -129,
    -129,   -66,  -129,   -64,  -129,  -129,  -129,  -129,  -129,  -129,
     -56,  -129,   -59,  -129,  -129,   -60,  -129,  -129,  -129,  -129,
      20,  -129,  -129,   -77
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      19,    20,    21,    22,    23,    24,    25,    26,     3,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,     4,    13,   168,
     169,    55,    14,    17,    56,   177,     5,   106,    99,   102,
     107,   108,   109,   110,   111,   193,   112,     6,   113,   114,
     119,   122,   164,   128,   207,   125,   140,   129,   137,   141,
     142,   143,   144,   145,   146,   169,   203,   205,   147,   150,
     153,   159,   156,   162,   165,    56,   166,   170,   171,   172,
     173,   174,   175,   176,   187,   179,   181,   188,   183,   185,
     189,   190,   191,   192,   195,   204,   201,   202,   194,   200,
     199,   197,   196,   198,   206,     0,   163
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-129))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
       4,     5,     6,     7,     8,     9,    10,    11,     0,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,     3,    47,    42,
      43,    45,    47,    51,    48,    51,    12,    47,    50,    50,
      47,    47,    47,    47,    47,   164,    47,    23,    47,    46,
      46,    49,    41,    47,   192,    50,    47,    50,    50,    47,
      47,    47,    47,    47,    47,    43,   188,   190,    50,    50,
      50,    50,    47,    47,    41,    48,    47,    41,    47,    47,
      41,    41,    41,    41,    41,    51,    51,    41,    51,    51,
      41,    41,    41,    41,    46,   189,   176,   187,   165,   175,
     174,   170,   168,   173,   191,    -1,    96
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    53,    54,     0,     3,    12,    23,    55,    57,    58,
      59,    62,    64,    47,    47,    65,    60,    51,    66,     4,
       5,     6,     7,     8,     9,    10,    11,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    45,    48,    56,    61,    63,
      67,    70,    73,    76,    79,    82,    83,    84,    85,    86,
      87,    90,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   104,   107,   110,   115,   118,   121,   124,   127,   128,
     129,   130,   135,   138,   139,   140,   141,   142,   143,    50,
      88,    89,    50,   119,   120,    91,    47,    47,    47,    47,
      47,    47,    47,    47,    46,   131,   132,   133,   134,    46,
     136,   137,    49,   125,   126,    50,   122,   123,    47,    50,
     113,   114,    77,    68,    71,    74,    80,    50,   116,   117,
      47,    47,    47,    47,    47,    47,    47,    50,   105,   106,
      50,   108,   109,    50,   111,   112,    47,   144,   145,    50,
     102,   103,    47,   142,    41,    41,    47,    92,    42,    43,
      41,    47,    47,    41,    41,    41,    41,    51,    78,    51,
      69,    51,    72,    51,    75,    51,    81,    41,    41,    41,
      41,    41,    41,    89,   119,    46,   132,   134,   137,   125,
     123,   114,   117,   106,   109,   112,   145,   103
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
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
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


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
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

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
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
  if (yypact_value_is_default (yyn))
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
      if (yytable_value_is_error (yyn))
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

/* Line 1806 of yacc.c  */
#line 60 "levcomp.ypp"
    { }
    break;

  case 3:

/* Line 1806 of yacc.c  */
#line 63 "levcomp.ypp"
    {}
    break;

  case 4:

/* Line 1806 of yacc.c  */
#line 64 "levcomp.ypp"
    {}
    break;

  case 5:

/* Line 1806 of yacc.c  */
#line 67 "levcomp.ypp"
    {}
    break;

  case 6:

/* Line 1806 of yacc.c  */
#line 68 "levcomp.ypp"
    {}
    break;

  case 7:

/* Line 1806 of yacc.c  */
#line 72 "levcomp.ypp"
    {
                    yyerror("Unexpected character sequence.");
                }
    break;

  case 8:

/* Line 1806 of yacc.c  */
#line 77 "levcomp.ypp"
    {}
    break;

  case 9:

/* Line 1806 of yacc.c  */
#line 78 "levcomp.ypp"
    {}
    break;

  case 10:

/* Line 1806 of yacc.c  */
#line 82 "levcomp.ypp"
    {
                    dgn_reset_default_depth();
                    std::string err = dgn_set_default_depth((yyvsp[(2) - (2)].text));
                    if (!err.empty())
                        yyerror(make_stringf("Bad default-depth: %s (%s)",
                                (yyvsp[(2) - (2)].text), err.c_str()).c_str());
                }
    break;

  case 11:

/* Line 1806 of yacc.c  */
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

                    std::string err =
                        lc_map.validate_map_def(lc_default_depths);
                    dump_map(lc_map);
                    if (!err.empty())
                        yyerror(err.c_str());
                    add_parsed_map(lc_map);
                }
    break;

  case 12:

/* Line 1806 of yacc.c  */
#line 112 "levcomp.ypp"
    { }
    break;

  case 13:

/* Line 1806 of yacc.c  */
#line 113 "levcomp.ypp"
    { }
    break;

  case 14:

/* Line 1806 of yacc.c  */
#line 116 "levcomp.ypp"
    { }
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 117 "levcomp.ypp"
    { }
    break;

  case 16:

/* Line 1806 of yacc.c  */
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

/* Line 1806 of yacc.c  */
#line 180 "levcomp.ypp"
    {}
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 183 "levcomp.ypp"
    { }
    break;

  case 55:

/* Line 1806 of yacc.c  */
#line 185 "levcomp.ypp"
    { }
    break;

  case 56:

/* Line 1806 of yacc.c  */
#line 186 "levcomp.ypp"
    { }
    break;

  case 57:

/* Line 1806 of yacc.c  */
#line 190 "levcomp.ypp"
    {
                    lc_global_prelude.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 58:

/* Line 1806 of yacc.c  */
#line 194 "levcomp.ypp"
    { }
    break;

  case 59:

/* Line 1806 of yacc.c  */
#line 196 "levcomp.ypp"
    { }
    break;

  case 60:

/* Line 1806 of yacc.c  */
#line 197 "levcomp.ypp"
    { }
    break;

  case 61:

/* Line 1806 of yacc.c  */
#line 201 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 62:

/* Line 1806 of yacc.c  */
#line 205 "levcomp.ypp"
    { }
    break;

  case 63:

/* Line 1806 of yacc.c  */
#line 207 "levcomp.ypp"
    { }
    break;

  case 64:

/* Line 1806 of yacc.c  */
#line 208 "levcomp.ypp"
    { }
    break;

  case 65:

/* Line 1806 of yacc.c  */
#line 212 "levcomp.ypp"
    {
                    lc_map.validate.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 66:

/* Line 1806 of yacc.c  */
#line 216 "levcomp.ypp"
    { }
    break;

  case 67:

/* Line 1806 of yacc.c  */
#line 218 "levcomp.ypp"
    { }
    break;

  case 68:

/* Line 1806 of yacc.c  */
#line 219 "levcomp.ypp"
    { }
    break;

  case 69:

/* Line 1806 of yacc.c  */
#line 223 "levcomp.ypp"
    {
                    lc_map.veto.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 70:

/* Line 1806 of yacc.c  */
#line 227 "levcomp.ypp"
    { }
    break;

  case 71:

/* Line 1806 of yacc.c  */
#line 229 "levcomp.ypp"
    { }
    break;

  case 72:

/* Line 1806 of yacc.c  */
#line 230 "levcomp.ypp"
    { }
    break;

  case 73:

/* Line 1806 of yacc.c  */
#line 234 "levcomp.ypp"
    {
                    lc_map.prelude.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 74:

/* Line 1806 of yacc.c  */
#line 238 "levcomp.ypp"
    { }
    break;

  case 75:

/* Line 1806 of yacc.c  */
#line 240 "levcomp.ypp"
    { }
    break;

  case 76:

/* Line 1806 of yacc.c  */
#line 241 "levcomp.ypp"
    { }
    break;

  case 77:

/* Line 1806 of yacc.c  */
#line 245 "levcomp.ypp"
    {
                    lc_map.epilogue.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 78:

/* Line 1806 of yacc.c  */
#line 249 "levcomp.ypp"
    { }
    break;

  case 79:

/* Line 1806 of yacc.c  */
#line 251 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kfeat(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 80:

/* Line 1806 of yacc.c  */
#line 258 "levcomp.ypp"
    { }
    break;

  case 81:

/* Line 1806 of yacc.c  */
#line 260 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmons(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 82:

/* Line 1806 of yacc.c  */
#line 267 "levcomp.ypp"
    { }
    break;

  case 83:

/* Line 1806 of yacc.c  */
#line 269 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kitem(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 84:

/* Line 1806 of yacc.c  */
#line 276 "levcomp.ypp"
    { }
    break;

  case 85:

/* Line 1806 of yacc.c  */
#line 278 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmask(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 86:

/* Line 1806 of yacc.c  */
#line 285 "levcomp.ypp"
    { }
    break;

  case 87:

/* Line 1806 of yacc.c  */
#line 287 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kprop(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 88:

/* Line 1806 of yacc.c  */
#line 294 "levcomp.ypp"
    {}
    break;

  case 91:

/* Line 1806 of yacc.c  */
#line 302 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("shuffle(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 92:

/* Line 1806 of yacc.c  */
#line 309 "levcomp.ypp"
    {}
    break;

  case 95:

/* Line 1806 of yacc.c  */
#line 317 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tags(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 96:

/* Line 1806 of yacc.c  */
#line 326 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lflags(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 97:

/* Line 1806 of yacc.c  */
#line 335 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("bflags(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 98:

/* Line 1806 of yacc.c  */
#line 344 "levcomp.ypp"
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

/* Line 1806 of yacc.c  */
#line 376 "levcomp.ypp"
    { }
    break;

  case 100:

/* Line 1806 of yacc.c  */
#line 379 "levcomp.ypp"
    { }
    break;

  case 101:

/* Line 1806 of yacc.c  */
#line 381 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloorcol(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 102:

/* Line 1806 of yacc.c  */
#line 388 "levcomp.ypp"
    { }
    break;

  case 103:

/* Line 1806 of yacc.c  */
#line 390 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrockcol(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 104:

/* Line 1806 of yacc.c  */
#line 397 "levcomp.ypp"
    { }
    break;

  case 105:

/* Line 1806 of yacc.c  */
#line 399 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloortile(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 106:

/* Line 1806 of yacc.c  */
#line 407 "levcomp.ypp"
    { }
    break;

  case 107:

/* Line 1806 of yacc.c  */
#line 409 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrocktile(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 111:

/* Line 1806 of yacc.c  */
#line 424 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("fheight(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 115:

/* Line 1806 of yacc.c  */
#line 440 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("ftile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 119:

/* Line 1806 of yacc.c  */
#line 456 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("rtile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 123:

/* Line 1806 of yacc.c  */
#line 472 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 124:

/* Line 1806 of yacc.c  */
#line 481 "levcomp.ypp"
    { }
    break;

  case 125:

/* Line 1806 of yacc.c  */
#line 482 "levcomp.ypp"
    { }
    break;

  case 126:

/* Line 1806 of yacc.c  */
#line 486 "levcomp.ypp"
    {
                      lc_map.main.add(
                          yylineno,
                          make_stringf("colour(\"%s\")",
                              quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                  }
    break;

  case 127:

/* Line 1806 of yacc.c  */
#line 494 "levcomp.ypp"
    { }
    break;

  case 128:

/* Line 1806 of yacc.c  */
#line 497 "levcomp.ypp"
    { }
    break;

  case 129:

/* Line 1806 of yacc.c  */
#line 498 "levcomp.ypp"
    { }
    break;

  case 130:

/* Line 1806 of yacc.c  */
#line 502 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("nsubst(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 131:

/* Line 1806 of yacc.c  */
#line 510 "levcomp.ypp"
    { }
    break;

  case 134:

/* Line 1806 of yacc.c  */
#line 518 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("subst(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 135:

/* Line 1806 of yacc.c  */
#line 526 "levcomp.ypp"
    {}
    break;

  case 136:

/* Line 1806 of yacc.c  */
#line 527 "levcomp.ypp"
    {}
    break;

  case 139:

/* Line 1806 of yacc.c  */
#line 535 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("item(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 140:

/* Line 1806 of yacc.c  */
#line 542 "levcomp.ypp"
    {}
    break;

  case 141:

/* Line 1806 of yacc.c  */
#line 543 "levcomp.ypp"
    {}
    break;

  case 144:

/* Line 1806 of yacc.c  */
#line 551 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("mons(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 145:

/* Line 1806 of yacc.c  */
#line 560 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("place(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 146:

/* Line 1806 of yacc.c  */
#line 569 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("desc(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 147:

/* Line 1806 of yacc.c  */
#line 577 "levcomp.ypp"
    {}
    break;

  case 148:

/* Line 1806 of yacc.c  */
#line 579 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 149:

/* Line 1806 of yacc.c  */
#line 587 "levcomp.ypp"
    { }
    break;

  case 150:

/* Line 1806 of yacc.c  */
#line 588 "levcomp.ypp"
    { }
    break;

  case 153:

/* Line 1806 of yacc.c  */
#line 595 "levcomp.ypp"
    {
                    (yyval.i) = (yyvsp[(1) - (2)].f) * 100;
                }
    break;

  case 154:

/* Line 1806 of yacc.c  */
#line 599 "levcomp.ypp"
    {
                    (yyval.i) = (yyvsp[(1) - (1)].f);
                }
    break;

  case 155:

/* Line 1806 of yacc.c  */
#line 604 "levcomp.ypp"
    {
                    (yyval.chance).priority = (yyvsp[(1) - (3)].f);
                    (yyval.chance).chance   = (yyvsp[(3) - (3)].i);
                }
    break;

  case 156:

/* Line 1806 of yacc.c  */
#line 609 "levcomp.ypp"
    {
                    (yyval.chance).priority = DEFAULT_CHANCE_PRIORITY;
                    (yyval.chance).chance   = (yyvsp[(1) - (1)].i);
                }
    break;

  case 157:

/* Line 1806 of yacc.c  */
#line 616 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth_chance(\"%s\", %d, %d)",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str(),
                                     (yyvsp[(1) - (2)].chance).priority, (yyvsp[(1) - (2)].chance).chance));
                }
    break;

  case 158:

/* Line 1806 of yacc.c  */
#line 624 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("chance(%d, %d)",
                                     (yyvsp[(1) - (1)].chance).priority, (yyvsp[(1) - (1)].chance).chance));
                }
    break;

  case 162:

/* Line 1806 of yacc.c  */
#line 638 "levcomp.ypp"
    {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("depth_weight(\"%s\", %d)",
                                      quote_lua_string((yyvsp[(2) - (2)].text)).c_str(), (int)(yyvsp[(1) - (2)].f)));
                 }
    break;

  case 163:

/* Line 1806 of yacc.c  */
#line 645 "levcomp.ypp"
    {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("weight(%d)", (int)(yyvsp[(1) - (1)].f)));
                 }
    break;

  case 164:

/* Line 1806 of yacc.c  */
#line 652 "levcomp.ypp"
    {}
    break;

  case 165:

/* Line 1806 of yacc.c  */
#line 654 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("orient(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 166:

/* Line 1806 of yacc.c  */
#line 663 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("welcome(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 170:

/* Line 1806 of yacc.c  */
#line 679 "levcomp.ypp"
    {
                    lc_map.mapchunk.add(
                        yylineno,
                        make_stringf("map(\"%s\")",
                                     quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 174:

/* Line 1806 of yacc.c  */
#line 695 "levcomp.ypp"
    {
                       lc_map.main.add(
                           yylineno,
                           make_stringf("subvault(\"%s\")",
                               quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                   }
    break;



/* Line 1806 of yacc.c  */
#line 2707 "levcomp.tab.c"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
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
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
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
      if (!yypact_value_is_default (yyn))
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
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
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



/* Line 2067 of yacc.c  */
#line 703 "levcomp.ypp"


