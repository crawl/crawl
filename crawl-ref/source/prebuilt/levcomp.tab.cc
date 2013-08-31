
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
#line 106 "levcomp.tab.c"

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
     LFLAGS = 285,
     BFLAGS = 286,
     LFLOORCOL = 287,
     LROCKCOL = 288,
     LFLOORTILE = 289,
     LROCKTILE = 290,
     FTILE = 291,
     RTILE = 292,
     TILE = 293,
     SUBVAULT = 294,
     FHEIGHT = 295,
     DESC = 296,
     COMMA = 297,
     COLON = 298,
     PERC = 299,
     DASH = 300,
     CHARACTER = 301,
     NUMBER = 302,
     STRING = 303,
     MAP_LINE = 304,
     MONSTER_NAME = 305,
     ITEM_INFO = 306,
     LUA_LINE = 307
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
#define LFLAGS 285
#define BFLAGS 286
#define LFLOORCOL 287
#define LROCKCOL 288
#define LFLOORTILE 289
#define LROCKTILE 290
#define FTILE 291
#define RTILE 292
#define TILE 293
#define SUBVAULT 294
#define FHEIGHT 295
#define DESC 296
#define COMMA 297
#define COLON 298
#define PERC 299
#define DASH 300
#define CHARACTER 301
#define NUMBER 302
#define STRING 303
#define MAP_LINE 304
#define MONSTER_NAME 305
#define ITEM_INFO 306
#define LUA_LINE 307




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
#line 255 "levcomp.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 267 "levcomp.tab.c"

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
#define YYLAST   119

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  53
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  95
/* YYNRULES -- Number of rules.  */
#define YYNRULES  176
/* YYNRULES -- Number of states.  */
#define YYNSTATES  211

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   307

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
      45,    46,    47,    48,    49,    50,    51,    52
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
     102,   104,   106,   108,   110,   112,   115,   116,   119,   121,
     124,   125,   128,   130,   133,   134,   137,   139,   142,   143,
     146,   148,   151,   152,   155,   157,   160,   161,   164,   166,
     168,   171,   173,   176,   178,   181,   183,   186,   188,   191,
     194,   196,   200,   202,   205,   208,   209,   212,   214,   217,
     220,   223,   226,   228,   231,   233,   236,   238,   241,   243,
     246,   249,   251,   255,   257,   260,   262,   266,   268,   271,
     273,   277,   279,   282,   284,   288,   290,   292,   296,   298,
     301,   303,   307,   309,   312,   314,   318,   320,   322,   325,
     329,   331,   333,   335,   338,   342,   344,   346,   349,   352,
     354,   357,   360,   362,   366,   368,   371,   373,   377,   379,
     382,   384,   387,   391,   393,   396,   398,   400,   403,   406,
     408,   411,   413,   415,   418,   420,   424
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
      54,     0,    -1,    55,    -1,    -1,    55,    56,    -1,    58,
      -1,    60,    -1,    46,    -1,    59,    -1,    65,    -1,     3,
      48,    -1,    63,    61,    -1,    -1,    61,    62,    -1,    64,
      -1,   142,    -1,    13,    48,    -1,   129,    -1,   130,    -1,
     131,    -1,   132,    -1,   137,    -1,   140,    -1,   141,    -1,
     126,    -1,   123,    -1,    97,    -1,   120,    -1,   117,    -1,
      98,    -1,    99,    -1,   100,    -1,   101,    -1,   102,    -1,
     106,    -1,   109,    -1,   112,    -1,    88,    -1,    91,    -1,
      92,    -1,    95,    -1,    96,    -1,    83,    -1,    85,    -1,
      84,    -1,    86,    -1,    87,    -1,   103,    -1,   145,    -1,
      68,    -1,    77,    -1,    71,    -1,    74,    -1,    80,    -1,
      57,    -1,    24,    66,    -1,    -1,    66,    67,    -1,    52,
      -1,    24,    69,    -1,    -1,    69,    70,    -1,    52,    -1,
      25,    72,    -1,    -1,    72,    73,    -1,    52,    -1,    26,
      75,    -1,    -1,    75,    76,    -1,    52,    -1,    23,    78,
      -1,    -1,    78,    79,    -1,    52,    -1,    27,    81,    -1,
      -1,    81,    82,    -1,    52,    -1,     8,    -1,     8,    48,
      -1,    10,    -1,    10,    48,    -1,     9,    -1,     9,    48,
      -1,    11,    -1,    11,    48,    -1,    12,    -1,    12,    48,
      -1,     4,    89,    -1,    90,    -1,    89,    42,    90,    -1,
      51,    -1,     5,    48,    -1,     7,    93,    -1,    -1,    93,
      94,    -1,    48,    -1,    30,    48,    -1,    31,    48,    -1,
      21,    48,    -1,    22,   115,    -1,    32,    -1,    32,    48,
      -1,    33,    -1,    33,    48,    -1,    34,    -1,    34,    48,
      -1,    35,    -1,    35,    48,    -1,    40,   104,    -1,   105,
      -1,   104,    42,   105,    -1,    51,    -1,    36,   107,    -1,
     108,    -1,   107,    42,   108,    -1,    51,    -1,    37,   110,
      -1,   111,    -1,   110,    42,   111,    -1,    51,    -1,    38,
     113,    -1,   114,    -1,   113,    42,   114,    -1,    51,    -1,
     116,    -1,   115,    42,   116,    -1,    51,    -1,    28,   118,
      -1,   119,    -1,   118,    42,   119,    -1,    51,    -1,     6,
     121,    -1,   122,    -1,   122,    42,   121,    -1,    51,    -1,
      20,    -1,    20,   124,    -1,   124,    42,   125,    -1,   125,
      -1,    51,    -1,    19,    -1,    19,   127,    -1,   128,    42,
     127,    -1,   128,    -1,    50,    -1,    16,    48,    -1,    41,
      48,    -1,    14,    -1,    14,    48,    -1,    17,   133,    -1,
      17,    -1,   133,    42,   136,    -1,   136,    -1,    47,    44,
      -1,    47,    -1,    47,    43,   134,    -1,   134,    -1,   135,
      48,    -1,   135,    -1,    18,   138,    -1,   138,    42,   139,
      -1,   139,    -1,    47,    48,    -1,    47,    -1,    15,    -1,
      15,    48,    -1,    29,    48,    -1,   143,    -1,   143,   144,
      -1,   144,    -1,    49,    -1,    39,   146,    -1,   147,    -1,
     146,    42,   147,    -1,    48,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    60,    60,    63,    64,    67,    68,    71,    77,    78,
      81,    91,   112,   113,   116,   117,   120,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,   155,   156,
     157,   158,   159,   160,   161,   162,   163,   164,   165,   166,
     167,   168,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   181,   184,   186,   187,   190,   195,
     197,   198,   201,   206,   208,   209,   212,   217,   219,   220,
     223,   228,   230,   231,   234,   239,   241,   242,   245,   250,
     251,   259,   260,   268,   269,   277,   278,   286,   287,   295,
     298,   299,   302,   310,   319,   322,   323,   326,   335,   344,
     353,   386,   389,   390,   398,   399,   407,   408,   417,   418,
     427,   429,   430,   433,   442,   445,   446,   449,   458,   461,
     462,   465,   474,   477,   478,   481,   491,   492,   495,   504,
     507,   508,   511,   520,   523,   524,   527,   536,   537,   540,
     541,   544,   552,   553,   556,   557,   560,   569,   578,   587,
     588,   597,   598,   601,   602,   604,   608,   613,   618,   625,
     633,   642,   644,   645,   647,   654,   662,   663,   672,   681,
     684,   685,   688,   697,   700,   701,   704
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
  "clear", "tags", "tagstrings", "tagstring", "lflags", "bflags", "marker",
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
     305,   306,   307
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    53,    54,    55,    55,    56,    56,    57,    58,    58,
      59,    60,    61,    61,    62,    62,    63,    64,    64,    64,
      64,    64,    64,    64,    64,    64,    64,    64,    64,    64,
      64,    64,    64,    64,    64,    64,    64,    64,    64,    64,
      64,    64,    64,    64,    64,    64,    64,    64,    64,    64,
      64,    64,    64,    64,    64,    65,    66,    66,    67,    68,
      69,    69,    70,    71,    72,    72,    73,    74,    75,    75,
      76,    77,    78,    78,    79,    80,    81,    81,    82,    83,
      83,    84,    84,    85,    85,    86,    86,    87,    87,    88,
      89,    89,    90,    91,    92,    93,    93,    94,    95,    96,
      97,    98,    99,    99,   100,   100,   101,   101,   102,   102,
     103,   104,   104,   105,   106,   107,   107,   108,   109,   110,
     110,   111,   112,   113,   113,   114,   115,   115,   116,   117,
     118,   118,   119,   120,   121,   121,   122,   123,   123,   124,
     124,   125,   126,   126,   127,   127,   128,   129,   130,   131,
     131,   132,   132,   133,   133,   134,   134,   135,   135,   136,
     136,   137,   138,   138,   139,   139,   140,   140,   141,   142,
     143,   143,   144,   145,   146,   146,   147
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     1,     1,     1,     1,
       2,     2,     0,     2,     1,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     0,     2,     1,     2,
       0,     2,     1,     2,     0,     2,     1,     2,     0,     2,
       1,     2,     0,     2,     1,     2,     0,     2,     1,     1,
       2,     1,     2,     1,     2,     1,     2,     1,     2,     2,
       1,     3,     1,     2,     2,     0,     2,     1,     2,     2,
       2,     2,     1,     2,     1,     2,     1,     2,     1,     2,
       2,     1,     3,     1,     2,     1,     3,     1,     2,     1,
       3,     1,     2,     1,     3,     1,     1,     3,     1,     2,
       1,     3,     1,     2,     1,     3,     1,     1,     2,     3,
       1,     1,     1,     2,     3,     1,     1,     2,     2,     1,
       2,     2,     1,     3,     1,     2,     1,     3,     1,     2,
       1,     2,     3,     1,     2,     1,     1,     2,     2,     1,
       2,     1,     1,     2,     1,     3,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,     0,     0,    56,     4,     5,     8,
       6,    12,     9,    10,    16,    55,    11,    58,    57,     0,
       0,     0,    95,    79,    83,    81,    85,    87,   149,   166,
       0,   152,     0,   142,   137,     0,     0,    72,    60,    64,
      68,    76,     0,     0,     0,     0,   102,   104,   106,   108,
       0,     0,     0,     0,     0,     0,     7,   172,    54,    13,
      14,    49,    51,    52,    50,    53,    42,    44,    43,    45,
      46,    37,    38,    39,    40,    41,    26,    29,    30,    31,
      32,    33,    47,    34,    35,    36,    28,    27,    25,    24,
      17,    18,    19,    20,    21,    22,    23,    15,   169,   171,
      48,    92,    89,    90,    93,   136,   133,   134,    94,    80,
      84,    82,    86,    88,   150,   167,   147,   156,   151,   158,
     160,   154,   165,   161,   163,   146,   143,   145,   141,   138,
     140,   100,   128,   101,   126,    71,    59,    63,    67,    75,
     132,   129,   130,   168,    98,    99,   103,   105,   107,   109,
     117,   114,   115,   121,   118,   119,   125,   122,   123,   176,
     173,   174,   113,   110,   111,   148,   170,     0,     0,    97,
      96,     0,   155,     0,   159,   164,     0,     0,     0,     0,
      74,    73,    62,    61,    66,    65,    70,    69,    78,    77,
       0,     0,     0,     0,     0,     0,    91,   135,   156,   157,
     153,   162,   144,   139,   127,   131,   116,   120,   124,   175,
     112
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     7,    58,     8,     9,    10,    16,    59,
      11,    60,    12,    15,    18,    61,   136,   183,    62,   137,
     185,    63,   138,   187,    64,   135,   181,    65,   139,   189,
      66,    67,    68,    69,    70,    71,   102,   103,    72,    73,
     108,   170,    74,    75,    76,    77,    78,    79,    80,    81,
      82,   163,   164,    83,   151,   152,    84,   154,   155,    85,
     157,   158,   133,   134,    86,   141,   142,    87,   106,   107,
      88,   129,   130,    89,   126,   127,    90,    91,    92,    93,
     118,   119,   120,   121,    94,   123,   124,    95,    96,    97,
      98,    99,   100,   160,   161
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -130
static const yytype_int16 yypact[] =
{
    -130,     9,    35,  -130,    -9,    -5,  -130,  -130,  -130,  -130,
    -130,  -130,  -130,  -130,  -130,    -8,    -4,  -130,  -130,    -2,
      -1,     0,  -130,     2,     4,     5,     6,     7,     8,    10,
      12,    14,    15,    13,    16,    17,    18,  -130,  -130,  -130,
    -130,  -130,    19,    20,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    36,    32,    37,  -130,  -130,  -130,  -130,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,    38,  -130,
    -130,  -130,    22,  -130,  -130,  -130,  -130,    44,    40,  -130,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,    -3,    47,  -130,
      42,  -130,    43,    50,  -130,  -130,  -130,    51,  -130,    52,
    -130,  -130,  -130,    53,  -130,    -6,    45,    46,    48,    49,
    -130,    54,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,
    -130,    57,  -130,  -130,    60,  -130,  -130,    61,  -130,  -130,
      62,  -130,  -130,    63,  -130,  -130,  -130,    -2,     0,  -130,
    -130,    59,  -130,    14,  -130,  -130,    15,    13,    16,    18,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,
      19,    29,    30,    31,    36,    32,  -130,  -130,    33,  -130,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,
    -130
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -110,  -130,  -130,
    -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,  -130,
    -130,  -130,  -129,  -130,  -130,  -113,  -130,  -130,   -85,  -130,
    -130,  -114,  -130,   -71,  -130,  -130,   -81,  -130,   -58,  -130,
    -130,  -130,   -67,  -130,   -65,  -130,  -130,  -130,  -130,  -130,
    -130,   -57,  -130,   -60,  -130,  -130,   -61,  -130,  -130,  -130,
    -130,    21,  -130,  -130,   -78
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
      48,    49,    50,    51,    52,    53,    54,    55,     4,    13,
     171,   172,    56,    14,    17,    57,   180,   104,     5,   101,
     109,   105,   110,   111,   112,   113,   114,   196,   115,     6,
     116,   117,   122,   125,   167,   131,   210,   128,   143,   132,
     140,   144,   145,   146,   147,   148,   149,   172,   206,   208,
     150,   153,   156,   162,   159,   165,   168,    57,   169,   173,
     174,   175,   176,   177,   178,   179,   190,   182,   184,   191,
     186,   188,   192,   193,   194,   195,   198,   207,   204,   205,
     197,   203,   202,   200,   199,   201,   209,     0,     0,   166
};

static const yytype_int16 yycheck[] =
{
       4,     5,     6,     7,     8,     9,    10,    11,    12,     0,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     3,    48,
      43,    44,    46,    48,    52,    49,    52,    48,    13,    51,
      48,    51,    48,    48,    48,    48,    48,   167,    48,    24,
      48,    47,    47,    50,    42,    48,   195,    51,    48,    51,
      51,    48,    48,    48,    48,    48,    48,    44,   191,   193,
      51,    51,    51,    51,    48,    48,    42,    49,    48,    42,
      48,    48,    42,    42,    42,    42,    42,    52,    52,    42,
      52,    52,    42,    42,    42,    42,    47,   192,   179,   190,
     168,   178,   177,   173,   171,   176,   194,    -1,    -1,    98
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    54,    55,     0,     3,    13,    24,    56,    58,    59,
      60,    63,    65,    48,    48,    66,    61,    52,    67,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    46,    49,    57,    62,
      64,    68,    71,    74,    77,    80,    83,    84,    85,    86,
      87,    88,    91,    92,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   106,   109,   112,   117,   120,   123,   126,
     129,   130,   131,   132,   137,   140,   141,   142,   143,   144,
     145,    51,    89,    90,    48,    51,   121,   122,    93,    48,
      48,    48,    48,    48,    48,    48,    48,    47,   133,   134,
     135,   136,    47,   138,   139,    50,   127,   128,    51,   124,
     125,    48,    51,   115,   116,    78,    69,    72,    75,    81,
      51,   118,   119,    48,    48,    48,    48,    48,    48,    48,
      51,   107,   108,    51,   110,   111,    51,   113,   114,    48,
     146,   147,    51,   104,   105,    48,   144,    42,    42,    48,
      94,    43,    44,    42,    48,    48,    42,    42,    42,    42,
      52,    79,    52,    70,    52,    73,    52,    76,    52,    82,
      42,    42,    42,    42,    42,    42,    90,   121,    47,   134,
     136,   139,   127,   125,   116,   119,   108,   111,   114,   147,
     105
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

       Refer to the stacks through separate pointers, to allow yyoverflow
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

  case 54:

/* Line 1455 of yacc.c  */
#line 181 "levcomp.ypp"
    {}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 184 "levcomp.ypp"
    { }
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 186 "levcomp.ypp"
    { }
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 187 "levcomp.ypp"
    { }
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 191 "levcomp.ypp"
    {
                    lc_global_prelude.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 195 "levcomp.ypp"
    { }
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 197 "levcomp.ypp"
    { }
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 198 "levcomp.ypp"
    { }
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 202 "levcomp.ypp"
    {
                    lc_map.main.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 206 "levcomp.ypp"
    { }
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 208 "levcomp.ypp"
    { }
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 209 "levcomp.ypp"
    { }
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 213 "levcomp.ypp"
    {
                    lc_map.validate.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 217 "levcomp.ypp"
    { }
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 219 "levcomp.ypp"
    { }
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 220 "levcomp.ypp"
    { }
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 224 "levcomp.ypp"
    {
                    lc_map.veto.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 228 "levcomp.ypp"
    { }
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 230 "levcomp.ypp"
    { }
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 231 "levcomp.ypp"
    { }
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 235 "levcomp.ypp"
    {
                    lc_map.prelude.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 239 "levcomp.ypp"
    { }
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 241 "levcomp.ypp"
    { }
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 242 "levcomp.ypp"
    { }
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 246 "levcomp.ypp"
    {
                    lc_map.epilogue.add(yylineno, (yyvsp[(1) - (1)].text));
                }
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 250 "levcomp.ypp"
    { }
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 252 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kfeat(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 259 "levcomp.ypp"
    { }
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 261 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmons(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 268 "levcomp.ypp"
    { }
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 270 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kitem(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 277 "levcomp.ypp"
    { }
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 279 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmask(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 286 "levcomp.ypp"
    { }
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 288 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kprop(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 295 "levcomp.ypp"
    {}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 303 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("shuffle(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 311 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("clear(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 319 "levcomp.ypp"
    {}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 327 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tags(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 336 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lflags(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 345 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("bflags(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 354 "levcomp.ypp"
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

  case 101:

/* Line 1455 of yacc.c  */
#line 386 "levcomp.ypp"
    { }
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 389 "levcomp.ypp"
    { }
    break;

  case 103:

/* Line 1455 of yacc.c  */
#line 391 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloorcol(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 398 "levcomp.ypp"
    { }
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 400 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrockcol(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 407 "levcomp.ypp"
    { }
    break;

  case 107:

/* Line 1455 of yacc.c  */
#line 409 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloortile(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 108:

/* Line 1455 of yacc.c  */
#line 417 "levcomp.ypp"
    { }
    break;

  case 109:

/* Line 1455 of yacc.c  */
#line 419 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrocktile(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 113:

/* Line 1455 of yacc.c  */
#line 434 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("fheight(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 117:

/* Line 1455 of yacc.c  */
#line 450 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("ftile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 121:

/* Line 1455 of yacc.c  */
#line 466 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("rtile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 482 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tile(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 126:

/* Line 1455 of yacc.c  */
#line 491 "levcomp.ypp"
    { }
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 492 "levcomp.ypp"
    { }
    break;

  case 128:

/* Line 1455 of yacc.c  */
#line 496 "levcomp.ypp"
    {
                      lc_map.main.add(
                          yylineno,
                          make_stringf("colour(\"%s\")",
                              quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                  }
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 504 "levcomp.ypp"
    { }
    break;

  case 130:

/* Line 1455 of yacc.c  */
#line 507 "levcomp.ypp"
    { }
    break;

  case 131:

/* Line 1455 of yacc.c  */
#line 508 "levcomp.ypp"
    { }
    break;

  case 132:

/* Line 1455 of yacc.c  */
#line 512 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("nsubst(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 133:

/* Line 1455 of yacc.c  */
#line 520 "levcomp.ypp"
    { }
    break;

  case 136:

/* Line 1455 of yacc.c  */
#line 528 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("subst(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 137:

/* Line 1455 of yacc.c  */
#line 536 "levcomp.ypp"
    {}
    break;

  case 138:

/* Line 1455 of yacc.c  */
#line 537 "levcomp.ypp"
    {}
    break;

  case 141:

/* Line 1455 of yacc.c  */
#line 545 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("item(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 142:

/* Line 1455 of yacc.c  */
#line 552 "levcomp.ypp"
    {}
    break;

  case 143:

/* Line 1455 of yacc.c  */
#line 553 "levcomp.ypp"
    {}
    break;

  case 146:

/* Line 1455 of yacc.c  */
#line 561 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("mons(\"%s\")",
                            quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 147:

/* Line 1455 of yacc.c  */
#line 570 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("place(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 148:

/* Line 1455 of yacc.c  */
#line 579 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("desc(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 149:

/* Line 1455 of yacc.c  */
#line 587 "levcomp.ypp"
    {}
    break;

  case 150:

/* Line 1455 of yacc.c  */
#line 589 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth(\"%s\")",
                            quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 151:

/* Line 1455 of yacc.c  */
#line 597 "levcomp.ypp"
    { }
    break;

  case 152:

/* Line 1455 of yacc.c  */
#line 598 "levcomp.ypp"
    { }
    break;

  case 155:

/* Line 1455 of yacc.c  */
#line 605 "levcomp.ypp"
    {
                    (yyval.i) = (yyvsp[(1) - (2)].f) * 100;
                }
    break;

  case 156:

/* Line 1455 of yacc.c  */
#line 609 "levcomp.ypp"
    {
                    (yyval.i) = (yyvsp[(1) - (1)].f);
                }
    break;

  case 157:

/* Line 1455 of yacc.c  */
#line 614 "levcomp.ypp"
    {
                    (yyval.chance).priority = (yyvsp[(1) - (3)].f);
                    (yyval.chance).chance   = (yyvsp[(3) - (3)].i);
                }
    break;

  case 158:

/* Line 1455 of yacc.c  */
#line 619 "levcomp.ypp"
    {
                    (yyval.chance).priority = DEFAULT_CHANCE_PRIORITY;
                    (yyval.chance).chance   = (yyvsp[(1) - (1)].i);
                }
    break;

  case 159:

/* Line 1455 of yacc.c  */
#line 626 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth_chance(\"%s\", %d, %d)",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str(),
                                     (yyvsp[(1) - (2)].chance).priority, (yyvsp[(1) - (2)].chance).chance));
                }
    break;

  case 160:

/* Line 1455 of yacc.c  */
#line 634 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("chance(%d, %d)",
                                     (yyvsp[(1) - (1)].chance).priority, (yyvsp[(1) - (1)].chance).chance));
                }
    break;

  case 164:

/* Line 1455 of yacc.c  */
#line 648 "levcomp.ypp"
    {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("depth_weight(\"%s\", %d)",
                                      quote_lua_string((yyvsp[(2) - (2)].text)).c_str(), (int)(yyvsp[(1) - (2)].f)));
                 }
    break;

  case 165:

/* Line 1455 of yacc.c  */
#line 655 "levcomp.ypp"
    {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("weight(%d)", (int)(yyvsp[(1) - (1)].f)));
                 }
    break;

  case 166:

/* Line 1455 of yacc.c  */
#line 662 "levcomp.ypp"
    {}
    break;

  case 167:

/* Line 1455 of yacc.c  */
#line 664 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("orient(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 168:

/* Line 1455 of yacc.c  */
#line 673 "levcomp.ypp"
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("welcome(\"%s\")",
                                     quote_lua_string((yyvsp[(2) - (2)].text)).c_str()));
                }
    break;

  case 172:

/* Line 1455 of yacc.c  */
#line 689 "levcomp.ypp"
    {
                    lc_map.mapchunk.add(
                        yylineno,
                        make_stringf("map(\"%s\")",
                                     quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                }
    break;

  case 176:

/* Line 1455 of yacc.c  */
#line 705 "levcomp.ypp"
    {
                       lc_map.main.add(
                           yylineno,
                           make_stringf("subvault(\"%s\")",
                               quote_lua_string((yyvsp[(1) - (1)].text)).c_str()));
                   }
    break;



/* Line 1455 of yacc.c  */
#line 2694 "levcomp.tab.c"
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
#line 713 "levcomp.ypp"


