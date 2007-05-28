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
     BRANCHDEF = 258,
     BRANCH = 259,
     DESC = 260,
     DEFAULT = 261,
     DEFAULT_DEPTH = 262,
     SHUFFLE = 263,
     SUBST = 264,
     TAGS = 265,
     KFEAT = 266,
     KITEM = 267,
     KMONS = 268,
     NAME = 269,
     DEPTH = 270,
     ORIENT = 271,
     PLACE = 272,
     CHANCE = 273,
     FLAGS = 274,
     MONS = 275,
     ITEM = 276,
     ROOT_DEPTH = 277,
     ENTRY_MSG = 278,
     EXIT_MSG = 279,
     ROCK_COLOUR = 280,
     FLOOR_COLOUR = 281,
     ENCOMPASS = 282,
     FLOAT = 283,
     NORTH = 284,
     EAST = 285,
     SOUTH = 286,
     WEST = 287,
     NORTHEAST = 288,
     SOUTHEAST = 289,
     SOUTHWEST = 290,
     NORTHWEST = 291,
     LEVEL = 292,
     END = 293,
     PVAULT = 294,
     PMINIVAULT = 295,
     MONSTERS = 296,
     ENDMONSTERS = 297,
     CHARACTER = 298,
     NO_HMIRROR = 299,
     NO_VMIRROR = 300,
     NO_ROTATE = 301,
     PANDEMONIC = 302,
     DASH = 303,
     COMMA = 304,
     QUOTE = 305,
     OPAREN = 306,
     CPAREN = 307,
     COLON = 308,
     STAR = 309,
     NOT = 310,
     INTEGER = 311,
     STRING = 312,
     MAP_LINE = 313,
     MONSTER_NAME = 314,
     ITEM_INFO = 315,
     IDENTIFIER = 316
   };
#endif
/* Tokens.  */
#define BRANCHDEF 258
#define BRANCH 259
#define DESC 260
#define DEFAULT 261
#define DEFAULT_DEPTH 262
#define SHUFFLE 263
#define SUBST 264
#define TAGS 265
#define KFEAT 266
#define KITEM 267
#define KMONS 268
#define NAME 269
#define DEPTH 270
#define ORIENT 271
#define PLACE 272
#define CHANCE 273
#define FLAGS 274
#define MONS 275
#define ITEM 276
#define ROOT_DEPTH 277
#define ENTRY_MSG 278
#define EXIT_MSG 279
#define ROCK_COLOUR 280
#define FLOOR_COLOUR 281
#define ENCOMPASS 282
#define FLOAT 283
#define NORTH 284
#define EAST 285
#define SOUTH 286
#define WEST 287
#define NORTHEAST 288
#define SOUTHEAST 289
#define SOUTHWEST 290
#define NORTHWEST 291
#define LEVEL 292
#define END 293
#define PVAULT 294
#define PMINIVAULT 295
#define MONSTERS 296
#define ENDMONSTERS 297
#define CHARACTER 298
#define NO_HMIRROR 299
#define NO_VMIRROR 300
#define NO_ROTATE 301
#define PANDEMONIC 302
#define DASH 303
#define COMMA 304
#define QUOTE 305
#define OPAREN 306
#define CPAREN 307
#define COLON 308
#define STAR 309
#define NOT 310
#define INTEGER 311
#define STRING 312
#define MAP_LINE 313
#define MONSTER_NAME 314
#define ITEM_INFO 315
#define IDENTIFIER 316




/* Copy the first part of user declarations.  */
#line 1 "levcomp.ypp"


#include "AppHdr.h"
#include "libutil.h"
#include "levcomp.h"
#include "mapdef.h"
#include "stuff.h"
#include <map>

#define YYERROR_VERBOSE 1

int yylex();

extern int yylineno;

struct map_file_place
{
    std::string filename;
    int lineno;

    map_file_place(const std::string &s = "", int line = 0)
        : filename(s), lineno(line)
    {
    }
};

typedef std::map<std::string, map_file_place> map_load_info_t;

static map_load_info_t loaded_maps;

void yyerror(const char *e)
{
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
#line 54 "levcomp.ypp"
{
    int i;
    const char *text;
    raw_range range;
}
/* Line 193 of yacc.c.  */
#line 276 "levcomp.tab.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 289 "levcomp.tab.c"

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
#define YYLAST   83

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  62
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  43
/* YYNRULES -- Number of rules.  */
#define YYNRULES  91
/* YYNRULES -- Number of states.  */
#define YYNSTATES  113

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   316

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
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     5,     6,     9,    11,    13,    15,    16,
      20,    25,    28,    29,    32,    34,    36,    38,    40,    42,
      44,    46,    48,    50,    52,    54,    56,    58,    60,    63,
      65,    68,    70,    73,    76,    78,    82,    84,    87,    88,
      91,    94,    96,   100,   102,   104,   107,   111,   113,   115,
     117,   120,   124,   126,   128,   131,   133,   136,   138,   142,
     144,   148,   150,   153,   155,   159,   165,   169,   173,   175,
     178,   180,   183,   185,   187,   189,   191,   193,   195,   197,
     199,   201,   203,   206,   207,   210,   212,   214,   216,   218,
     220,   223
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      63,     0,    -1,    64,    -1,    -1,    64,    65,    -1,    66,
      -1,    69,    -1,    67,    -1,    -1,     7,    68,    92,    -1,
      70,    71,   102,    71,    -1,    14,    57,    -1,    -1,    72,
      71,    -1,    90,    -1,    91,    -1,    96,    -1,    97,    -1,
      99,    -1,    87,    -1,    84,    -1,    81,    -1,    79,    -1,
      76,    -1,    73,    -1,    75,    -1,    74,    -1,    11,    -1,
      11,    57,    -1,    13,    -1,    13,    57,    -1,    12,    -1,
      12,    57,    -1,     8,    77,    -1,    78,    -1,    77,    49,
      78,    -1,    60,    -1,    10,    80,    -1,    -1,    57,    80,
      -1,     9,    82,    -1,    83,    -1,    83,    49,    82,    -1,
      60,    -1,    21,    -1,    21,    85,    -1,    86,    49,    85,
      -1,    86,    -1,    60,    -1,    20,    -1,    20,    88,    -1,
      89,    49,    88,    -1,    89,    -1,    59,    -1,    17,    57,
      -1,    15,    -1,    15,    93,    -1,    94,    -1,    92,    49,
      94,    -1,    94,    -1,    93,    49,    94,    -1,    95,    -1,
      55,    95,    -1,    61,    -1,    61,    53,    54,    -1,    61,
      53,    56,    48,    56,    -1,    61,    53,    56,    -1,    56,
      48,    56,    -1,    56,    -1,    18,    56,    -1,    16,    -1,
      16,    98,    -1,    27,    -1,    29,    -1,    30,    -1,    31,
      -1,    32,    -1,    33,    -1,    34,    -1,    35,    -1,    36,
      -1,    28,    -1,    19,   100,    -1,    -1,   101,   100,    -1,
      44,    -1,    45,    -1,    46,    -1,   103,    -1,   104,    -1,
     104,   103,    -1,    58,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    87,    87,    90,    91,    94,    95,    98,   102,   101,
     106,   149,   171,   172,   175,   176,   177,   178,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   190,   191,   200,
     201,   210,   211,   220,   223,   224,   227,   237,   240,   241,
     249,   252,   253,   256,   267,   268,   271,   272,   275,   290,
     291,   294,   295,   298,   314,   320,   321,   325,   330,   337,
     342,   348,   349,   352,   357,   362,   367,   372,   377,   383,
     389,   390,   396,   397,   398,   399,   400,   401,   402,   403,
     404,   405,   408,   411,   412,   428,   429,   430,   433,   436,
     437,   440
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "BRANCHDEF", "BRANCH", "DESC", "DEFAULT",
  "DEFAULT_DEPTH", "SHUFFLE", "SUBST", "TAGS", "KFEAT", "KITEM", "KMONS",
  "NAME", "DEPTH", "ORIENT", "PLACE", "CHANCE", "FLAGS", "MONS", "ITEM",
  "ROOT_DEPTH", "ENTRY_MSG", "EXIT_MSG", "ROCK_COLOUR", "FLOOR_COLOUR",
  "ENCOMPASS", "FLOAT", "NORTH", "EAST", "SOUTH", "WEST", "NORTHEAST",
  "SOUTHEAST", "SOUTHWEST", "NORTHWEST", "LEVEL", "END", "PVAULT",
  "PMINIVAULT", "MONSTERS", "ENDMONSTERS", "CHARACTER", "NO_HMIRROR",
  "NO_VMIRROR", "NO_ROTATE", "PANDEMONIC", "DASH", "COMMA", "QUOTE",
  "OPAREN", "CPAREN", "COLON", "STAR", "NOT", "INTEGER", "STRING",
  "MAP_LINE", "MONSTER_NAME", "ITEM_INFO", "IDENTIFIER", "$accept", "file",
  "definitions", "definition", "def", "defdepth", "@1", "level", "name",
  "metalines", "metaline", "kfeat", "kmons", "kitem", "shuffle",
  "shuffle_specifiers", "shuffle_spec", "tags", "tagstrings", "subst",
  "subst_specifiers", "subst_spec", "items", "item_specifiers",
  "item_specifier", "mons", "mnames", "mname", "place", "depth",
  "default_depth_ranges", "extended_depth_ranges", "ext_range",
  "lev_range", "chance", "orientation", "orient_name", "flags",
  "flagnames", "flagname", "map_def", "map_lines", "map_line", 0
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
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    62,    63,    64,    64,    65,    65,    66,    68,    67,
      69,    70,    71,    71,    72,    72,    72,    72,    72,    72,
      72,    72,    72,    72,    72,    72,    72,    73,    73,    74,
      74,    75,    75,    76,    77,    77,    78,    79,    80,    80,
      81,    82,    82,    83,    84,    84,    85,    85,    86,    87,
      87,    88,    88,    89,    90,    91,    91,    92,    92,    93,
      93,    94,    94,    95,    95,    95,    95,    95,    95,    96,
      97,    97,    98,    98,    98,    98,    98,    98,    98,    98,
      98,    98,    99,   100,   100,   101,   101,   101,   102,   103,
     103,   104
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     1,     1,     0,     3,
       4,     2,     0,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     1,
       2,     1,     2,     2,     1,     3,     1,     2,     0,     2,
       2,     1,     3,     1,     1,     2,     3,     1,     1,     1,
       2,     3,     1,     1,     2,     1,     2,     1,     3,     1,
       3,     1,     2,     1,     3,     5,     3,     3,     1,     2,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     0,     2,     1,     1,     1,     1,     1,
       2,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,     8,     0,     4,     5,     7,     6,
      12,     0,    11,     0,     0,    38,    27,    31,    29,    55,
      70,     0,     0,    83,    49,    44,     0,    12,    24,    26,
      25,    23,    22,    21,    20,    19,    14,    15,    16,    17,
      18,     0,    68,    63,     9,    57,    61,    36,    33,    34,
      43,    40,    41,    38,    37,    28,    32,    30,    56,    59,
      72,    81,    73,    74,    75,    76,    77,    78,    79,    80,
      71,    54,    69,    85,    86,    87,    82,    83,    53,    50,
      52,    48,    45,    47,    91,    12,    88,    89,    13,    62,
       0,     0,     0,     0,     0,    39,     0,    84,     0,     0,
      10,    90,    67,    64,    66,    58,    35,    42,    60,    51,
      46,     0,    65
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     1,     2,     6,     7,     8,    11,     9,    10,    26,
      27,    28,    29,    30,    31,    48,    49,    32,    54,    33,
      51,    52,    34,    82,    83,    35,    79,    80,    36,    37,
      44,    58,    45,    46,    38,    39,    70,    40,    76,    77,
      85,    86,    87
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -53
static const yytype_int8 yypact[] =
{
     -53,     8,    19,   -53,   -53,   -18,   -53,   -53,   -53,   -53,
      -6,   -27,   -53,   -30,   -17,   -15,   -13,   -12,   -10,   -27,
     -11,    -8,   -25,    -9,    -7,   -14,    -5,    -6,   -53,   -53,
     -53,   -53,   -53,   -53,   -53,   -53,   -53,   -53,   -53,   -53,
     -53,   -29,     0,    -3,     2,   -53,   -53,   -53,     5,   -53,
     -53,   -53,     6,   -15,   -53,   -53,   -53,   -53,     7,   -53,
     -53,   -53,   -53,   -53,   -53,   -53,   -53,   -53,   -53,   -53,
     -53,   -53,   -53,   -53,   -53,   -53,   -53,    -9,   -53,   -53,
       9,   -53,   -53,    11,   -53,    -6,   -53,    -5,   -53,   -53,
       1,   -16,   -27,   -30,   -17,   -53,   -27,   -53,    -7,   -14,
     -53,   -53,   -53,   -53,    13,   -53,   -53,   -53,   -53,   -53,
     -53,    10,   -53
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -53,   -53,   -53,   -53,   -53,   -53,   -53,   -53,   -53,   -26,
     -53,   -53,   -53,   -53,   -53,   -53,   -52,   -53,    12,   -53,
     -32,   -53,   -53,   -36,   -53,   -53,   -34,   -53,   -53,   -53,
     -53,   -53,   -19,    26,   -53,   -53,   -53,   -53,    -2,   -53,
     -53,    -4,   -53
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      59,    88,    13,    14,    15,    16,    17,    18,     3,    19,
      20,    21,    22,    23,    24,    25,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,     4,    42,    41,    42,
      47,    72,    43,     5,    43,    73,    74,    75,   103,    12,
     104,   106,    53,    50,    55,    56,    81,    57,    90,    71,
      91,    92,    78,    84,    93,    94,    96,   102,    98,   100,
      99,   111,   107,   110,   109,    95,   112,    89,     0,     0,
       0,     0,     0,   105,     0,    97,     0,   108,     0,     0,
       0,     0,     0,   101
};

static const yytype_int8 yycheck[] =
{
      19,    27,     8,     9,    10,    11,    12,    13,     0,    15,
      16,    17,    18,    19,    20,    21,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,     7,    56,    55,    56,
      60,    56,    61,    14,    61,    44,    45,    46,    54,    57,
      56,    93,    57,    60,    57,    57,    60,    57,    48,    57,
      53,    49,    59,    58,    49,    49,    49,    56,    49,    85,
      49,    48,    94,    99,    98,    53,    56,    41,    -1,    -1,
      -1,    -1,    -1,    92,    -1,    77,    -1,    96,    -1,    -1,
      -1,    -1,    -1,    87
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    63,    64,     0,     7,    14,    65,    66,    67,    69,
      70,    68,    57,     8,     9,    10,    11,    12,    13,    15,
      16,    17,    18,    19,    20,    21,    71,    72,    73,    74,
      75,    76,    79,    81,    84,    87,    90,    91,    96,    97,
      99,    55,    56,    61,    92,    94,    95,    60,    77,    78,
      60,    82,    83,    57,    80,    57,    57,    57,    93,    94,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      98,    57,    56,    44,    45,    46,   100,   101,    59,    88,
      89,    60,    85,    86,    58,   102,   103,   104,    71,    95,
      48,    53,    49,    49,    49,    80,    49,   100,    49,    49,
      71,   103,    56,    54,    56,    94,    78,    82,    94,    88,
      85,    48,    56
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
#line 87 "levcomp.ypp"
    { }
    break;

  case 3:
#line 90 "levcomp.ypp"
    {}
    break;

  case 4:
#line 91 "levcomp.ypp"
    {}
    break;

  case 5:
#line 94 "levcomp.ypp"
    {}
    break;

  case 6:
#line 95 "levcomp.ypp"
    {}
    break;

  case 7:
#line 98 "levcomp.ypp"
    {}
    break;

  case 8:
#line 102 "levcomp.ypp"
    { lc_default_depths.clear(); }
    break;

  case 10:
#line 107 "levcomp.ypp"
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

                    if (!lc_map.has_depth() && !lc_default_depths.empty())
                        lc_map.add_depths(lc_default_depths.begin(),
                                          lc_default_depths.end());

                    add_parsed_map( lc_map );
                }
    break;

  case 11:
#line 150 "levcomp.ypp"
    {
                    lc_map.init();
                    lc_map.name = (yyvsp[(2) - (2)].text);

                    map_load_info_t::const_iterator i = 
                        loaded_maps.find((yyvsp[(2) - (2)].text));

                    if (i != loaded_maps.end())
                    {
                        yyerror(
                            make_stringf(
                                "Map named '%s' already loaded at %s:%d",
                                (yyvsp[(2) - (2)].text),
                                i->second.filename.c_str(),
                                i->second.lineno).c_str() );
                    }

                    loaded_maps[(yyvsp[(2) - (2)].text)] = map_file_place(lc_desfile, yylineno);
                }
    break;

  case 27:
#line 190 "levcomp.ypp"
    { }
    break;

  case 28:
#line 192 "levcomp.ypp"
    {
                    std::string err = lc_map.add_key_feat((yyvsp[(2) - (2)].text));
                    if (!err.empty())
                        yyerror(
                            make_stringf("Bad arg to KFEAT: '%s' (%s)",
                                (yyvsp[(2) - (2)].text), err.c_str()).c_str());
                }
    break;

  case 29:
#line 200 "levcomp.ypp"
    { }
    break;

  case 30:
#line 202 "levcomp.ypp"
    {
                    std::string err = lc_map.add_key_mons((yyvsp[(2) - (2)].text));
                    if (!err.empty())
                        yyerror(
                            make_stringf("Bad arg to KMONS: '%s' (%s)",
                                (yyvsp[(2) - (2)].text), err.c_str()).c_str());
                }
    break;

  case 31:
#line 210 "levcomp.ypp"
    { }
    break;

  case 32:
#line 212 "levcomp.ypp"
    {
                    std::string err = lc_map.add_key_item((yyvsp[(2) - (2)].text));
                    if (!err.empty())
                        yyerror(
                            make_stringf("Bad arg to KITEM: '%s' (%s)",
                                (yyvsp[(2) - (2)].text), err.c_str()).c_str());
                }
    break;

  case 33:
#line 220 "levcomp.ypp"
    {}
    break;

  case 36:
#line 228 "levcomp.ypp"
    {
                    std::string err = lc_map.map.add_shuffle((yyvsp[(1) - (1)].text));
                    if (!err.empty())
                        yyerror(
                            make_stringf(
                                "Bad shuffle argument: '%s' (%s)",
                                (yyvsp[(1) - (1)].text), err.c_str() ).c_str() );
                }
    break;

  case 37:
#line 237 "levcomp.ypp"
    {}
    break;

  case 39:
#line 242 "levcomp.ypp"
    {
                    lc_map.tags += " ";
                    lc_map.tags += (yyvsp[(1) - (2)].text);
                    lc_map.tags += " ";
                }
    break;

  case 40:
#line 249 "levcomp.ypp"
    { }
    break;

  case 43:
#line 257 "levcomp.ypp"
    {
                    std::string err = lc_map.map.add_subst((yyvsp[(1) - (1)].text));
                    if (!err.empty())
                        yyerror(
                            make_stringf(
                                "Bad SUBST argument: '%s' (%s)",
                                (yyvsp[(1) - (1)].text), err.c_str() ).c_str() );
                }
    break;

  case 44:
#line 267 "levcomp.ypp"
    {}
    break;

  case 45:
#line 268 "levcomp.ypp"
    {}
    break;

  case 48:
#line 276 "levcomp.ypp"
    {
                    std::string error = lc_map.items.add_item((yyvsp[(1) - (1)].text));
                    if (error.size())
                    {
                        char errbuf[300];
                        snprintf(errbuf, sizeof errbuf,
                            "Invalid item descriptor: '%s' (%s)",
                                (yyvsp[(1) - (1)].text), error.c_str());
                        yyerror(errbuf);
                    }
                    if (lc_map.items.size() > 8)
                        yyerror("Too many items specified (max 8)");
                }
    break;

  case 49:
#line 290 "levcomp.ypp"
    {}
    break;

  case 50:
#line 291 "levcomp.ypp"
    {}
    break;

  case 53:
#line 299 "levcomp.ypp"
    {
                    std::string err = lc_map.mons.add_mons((yyvsp[(1) - (1)].text));
                    if (!err.empty())
                    {
                        char buf[300];
                        snprintf(buf, sizeof buf, 
                                 "bad monster spec '%s' (%s)",
                                 (yyvsp[(1) - (1)].text), err.c_str());
                        yyerror(buf);
                    }
                    if (lc_map.mons.size() > 7)
                        yyerror("Too many monsters specified (max 7)");
                }
    break;

  case 54:
#line 315 "levcomp.ypp"
    {
                    lc_map.place = (yyvsp[(2) - (2)].text);
                }
    break;

  case 55:
#line 320 "levcomp.ypp"
    {}
    break;

  case 56:
#line 321 "levcomp.ypp"
    {}
    break;

  case 57:
#line 326 "levcomp.ypp"
    {
                    lc_default_depths.push_back((yyvsp[(1) - (1)].range));
                }
    break;

  case 58:
#line 331 "levcomp.ypp"
    {
                    lc_default_depths.push_back((yyvsp[(3) - (3)].range));
                }
    break;

  case 59:
#line 338 "levcomp.ypp"
    {
                    lc_map.add_depth((yyvsp[(1) - (1)].range));
                }
    break;

  case 60:
#line 343 "levcomp.ypp"
    {
                    lc_map.add_depth((yyvsp[(3) - (3)].range));
                }
    break;

  case 61:
#line 348 "levcomp.ypp"
    { (yyval.range) = (yyvsp[(1) - (1)].range); }
    break;

  case 62:
#line 349 "levcomp.ypp"
    { (yyval.range) = (yyvsp[(2) - (2)].range); (yyval.range).deny = true; }
    break;

  case 63:
#line 353 "levcomp.ypp"
    {
                    (yyval.range) = set_range((yyvsp[(1) - (1)].text), 1, 100);
                }
    break;

  case 64:
#line 358 "levcomp.ypp"
    {
                    (yyval.range) = set_range((yyvsp[(1) - (3)].text), 1, 100);
                }
    break;

  case 65:
#line 363 "levcomp.ypp"
    {
                    (yyval.range) = set_range((yyvsp[(1) - (5)].text), (yyvsp[(3) - (5)].i), (yyvsp[(5) - (5)].i));
                }
    break;

  case 66:
#line 368 "levcomp.ypp"
    {
                    (yyval.range) = set_range((yyvsp[(1) - (3)].text), (yyvsp[(3) - (3)].i), (yyvsp[(3) - (3)].i));
                }
    break;

  case 67:
#line 373 "levcomp.ypp"
    {
                    (yyval.range) = set_range("Any", (yyvsp[(1) - (3)].i), (yyvsp[(3) - (3)].i));
                }
    break;

  case 68:
#line 378 "levcomp.ypp"
    {
                    (yyval.range) = set_range("Any", (yyvsp[(1) - (1)].i), (yyvsp[(1) - (1)].i));
                }
    break;

  case 69:
#line 384 "levcomp.ypp"
    {
                    lc_map.chance = (yyvsp[(2) - (2)].i);
                }
    break;

  case 70:
#line 389 "levcomp.ypp"
    {}
    break;

  case 71:
#line 391 "levcomp.ypp"
    {
                    lc_map.orient = (map_section_type) (yyvsp[(2) - (2)].i);
                }
    break;

  case 72:
#line 396 "levcomp.ypp"
    { (yyval.i) = MAP_ENCOMPASS; }
    break;

  case 73:
#line 397 "levcomp.ypp"
    { (yyval.i) = MAP_NORTH; }
    break;

  case 74:
#line 398 "levcomp.ypp"
    { (yyval.i) = MAP_EAST; }
    break;

  case 75:
#line 399 "levcomp.ypp"
    { (yyval.i) = MAP_SOUTH; }
    break;

  case 76:
#line 400 "levcomp.ypp"
    { (yyval.i) = MAP_WEST; }
    break;

  case 77:
#line 401 "levcomp.ypp"
    { (yyval.i) = MAP_NORTHEAST; }
    break;

  case 78:
#line 402 "levcomp.ypp"
    { (yyval.i) = MAP_SOUTHEAST; }
    break;

  case 79:
#line 403 "levcomp.ypp"
    { (yyval.i) = MAP_SOUTHWEST; }
    break;

  case 80:
#line 404 "levcomp.ypp"
    { (yyval.i) = MAP_NORTHWEST; }
    break;

  case 81:
#line 405 "levcomp.ypp"
    { (yyval.i) = MAP_FLOAT; }
    break;

  case 82:
#line 408 "levcomp.ypp"
    {}
    break;

  case 84:
#line 413 "levcomp.ypp"
    {
                    switch ((yyvsp[(1) - (2)].i)) {
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
                }
    break;

  case 85:
#line 428 "levcomp.ypp"
    { (yyval.i) = NO_HMIRROR; }
    break;

  case 86:
#line 429 "levcomp.ypp"
    { (yyval.i) = NO_VMIRROR; }
    break;

  case 87:
#line 430 "levcomp.ypp"
    { (yyval.i) = NO_ROTATE;  }
    break;

  case 91:
#line 441 "levcomp.ypp"
    {
                    lc_map.map.add_line((yyvsp[(1) - (1)].text));
                }
    break;


/* Line 1267 of yacc.c.  */
#line 2073 "levcomp.tab.c"
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


#line 446 "levcomp.ypp"


