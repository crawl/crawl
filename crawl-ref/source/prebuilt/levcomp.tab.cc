/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

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
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 1 "levcomp.ypp" /* yacc.c:339  */



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


#line 98 "levcomp.tab.cc" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "levcomp.tab.h".  */
#ifndef YY_YY_LEVCOMP_TAB_H_INCLUDED
# define YY_YY_LEVCOMP_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
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

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 34 "levcomp.ypp" /* yacc.c:355  */

    int i;
    double f;
    const char *text;
    map_chance_pair chance;

#line 247 "levcomp.tab.cc" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_LEVCOMP_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 262 "levcomp.tab.cc" /* yacc.c:358  */

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
#else
typedef signed char yytype_int8;
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
# elif ! defined YYSIZE_T
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
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
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
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

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
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  208

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   306

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
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
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
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

#if YYDEBUG || YYERROR_VERBOSE || 0
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
  "subvault_specifiers", "subvault_specifier", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
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

#define YYPACT_NINF -128

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-128)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
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

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
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

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
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

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
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


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
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
#ifndef YYINITDEPTH
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
static YYSIZE_T
yystrlen (const char *yystr)
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
static char *
yystpcpy (char *yydest, const char *yysrc)
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
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
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
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
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

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

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

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

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
  int yytoken = 0;
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

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
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
      yychar = yylex ();
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
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

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
     '$$ = $1'.

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
#line 60 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1514 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 3:
#line 63 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1520 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 4:
#line 64 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1526 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 5:
#line 67 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1532 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 6:
#line 68 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1538 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 7:
#line 72 "levcomp.ypp" /* yacc.c:1646  */
    {
                    yyerror("Unexpected character sequence.");
                }
#line 1546 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 8:
#line 77 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1552 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 9:
#line 78 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1558 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 10:
#line 82 "levcomp.ypp" /* yacc.c:1646  */
    {
                    dgn_reset_default_depth();
                    string err = dgn_set_default_depth((yyvsp[0].text));
                    if (!err.empty())
                        yyerror(make_stringf("Bad default-depth: %s (%s)",
                                (yyvsp[0].text), err.c_str()).c_str());
                }
#line 1570 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 11:
#line 92 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.set_file(lc_desfile);

                    if (lc_run_global_prelude && !lc_global_prelude.empty())
                    {
                        lc_global_prelude.set_file(lc_desfile);
                        lc_run_global_prelude = false;
                        if (lc_global_prelude.load_call(dlua, nullptr))
                            yyerror( lc_global_prelude.orig_error().c_str() );
                    }

                    string err =
                        lc_map.validate_map_def(lc_default_depths);
                    dump_map(lc_map);
                    if (!err.empty())
                        yyerror(err.c_str());
                    add_parsed_map(lc_map);
                }
#line 1593 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 12:
#line 112 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1599 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 13:
#line 113 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1605 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 14:
#line 116 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1611 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 15:
#line 117 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1617 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 16:
#line 121 "levcomp.ypp" /* yacc.c:1646  */
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
#line 1643 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 53:
#line 180 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1649 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 54:
#line 183 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1655 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 55:
#line 185 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1661 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 56:
#line 186 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1667 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 57:
#line 190 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_global_prelude.add(yylineno, (yyvsp[0].text));
                }
#line 1675 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 58:
#line 194 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1681 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 59:
#line 196 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1687 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 60:
#line 197 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1693 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 61:
#line 201 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(yylineno, (yyvsp[0].text));
                }
#line 1701 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 62:
#line 205 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1707 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 63:
#line 207 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1713 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 64:
#line 208 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1719 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 65:
#line 212 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.validate.add(yylineno, (yyvsp[0].text));
                }
#line 1727 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 66:
#line 216 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1733 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 67:
#line 218 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1739 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 68:
#line 219 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1745 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 69:
#line 223 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.veto.add(yylineno, (yyvsp[0].text));
                }
#line 1753 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 70:
#line 227 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1759 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 71:
#line 229 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1765 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 72:
#line 230 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1771 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 73:
#line 234 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.prelude.add(yylineno, (yyvsp[0].text));
                }
#line 1779 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 74:
#line 238 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1785 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 75:
#line 240 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1791 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 76:
#line 241 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1797 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 77:
#line 245 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.epilogue.add(yylineno, (yyvsp[0].text));
                }
#line 1805 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 78:
#line 249 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1811 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 79:
#line 251 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kfeat(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1822 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 80:
#line 258 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1828 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 81:
#line 260 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmons(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1839 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 82:
#line 267 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1845 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 83:
#line 269 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kitem(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1856 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 84:
#line 276 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1862 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 85:
#line 278 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmask(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1873 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 86:
#line 285 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1879 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 87:
#line 287 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kprop(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1890 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 88:
#line 294 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1896 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 91:
#line 302 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("shuffle(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1907 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 92:
#line 310 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("clear(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1918 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 93:
#line 318 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1924 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 96:
#line 326 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tags(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1935 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 97:
#line 335 "levcomp.ypp" /* yacc.c:1646  */
    {
                    string key, arg;
                    int sep(0);

                    const string err =
                      mapdef_split_key_item((yyvsp[0].text), &key, &sep, &arg);

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
                                      quote_lua_string((yyvsp[0].text)).c_str()));
                    }
                }
#line 1970 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 98:
#line 367 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1976 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 99:
#line 370 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1982 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 100:
#line 372 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloorcol(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1993 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 101:
#line 379 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1999 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 102:
#line 381 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrockcol(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2010 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 103:
#line 388 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2016 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 104:
#line 390 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloortile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2027 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 105:
#line 398 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2033 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 106:
#line 400 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrocktile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2044 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 110:
#line 415 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("fheight(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2055 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 114:
#line 431 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("ftile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2066 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 118:
#line 447 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("rtile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2077 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 122:
#line 463 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2088 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 123:
#line 472 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2094 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 124:
#line 473 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2100 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 125:
#line 477 "levcomp.ypp" /* yacc.c:1646  */
    {
                      lc_map.main.add(
                          yylineno,
                          make_stringf("colour(\"%s\")",
                              quote_lua_string((yyvsp[0].text)).c_str()));
                  }
#line 2111 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 126:
#line 485 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2117 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 127:
#line 488 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2123 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 128:
#line 489 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2129 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 129:
#line 493 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("nsubst(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2140 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 130:
#line 501 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2146 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 133:
#line 509 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("subst(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2157 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 134:
#line 517 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2163 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 135:
#line 518 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2169 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 138:
#line 526 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("item(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2180 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 139:
#line 533 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2186 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 140:
#line 534 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2192 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 143:
#line 542 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("mons(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2203 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 144:
#line 551 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("place(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2214 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 145:
#line 560 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("desc(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2225 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 146:
#line 569 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("order(%d)", int((yyvsp[0].f))));
                }
#line 2235 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 147:
#line 576 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2241 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 148:
#line 578 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2252 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 149:
#line 586 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2258 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 150:
#line 587 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2264 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 153:
#line 594 "levcomp.ypp" /* yacc.c:1646  */
    {
                    (yyval.i) = (yyvsp[-1].f) * 100;
                }
#line 2272 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 154:
#line 598 "levcomp.ypp" /* yacc.c:1646  */
    {
                    (yyval.i) = (yyvsp[0].f);
                }
#line 2280 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 155:
#line 603 "levcomp.ypp" /* yacc.c:1646  */
    {
                    (yyval.chance).priority = (yyvsp[-2].f);
                    (yyval.chance).chance   = (yyvsp[0].i);
                }
#line 2289 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 156:
#line 608 "levcomp.ypp" /* yacc.c:1646  */
    {
                    (yyval.chance).priority = DEFAULT_CHANCE_PRIORITY;
                    (yyval.chance).chance   = (yyvsp[0].i);
                }
#line 2298 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 157:
#line 615 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth_chance(\"%s\", %d, %d)",
                                     quote_lua_string((yyvsp[0].text)).c_str(),
                                     (yyvsp[-1].chance).priority, (yyvsp[-1].chance).chance));
                }
#line 2310 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 158:
#line 623 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("chance(%d, %d)",
                                     (yyvsp[0].chance).priority, (yyvsp[0].chance).chance));
                }
#line 2321 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 162:
#line 637 "levcomp.ypp" /* yacc.c:1646  */
    {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("depth_weight(\"%s\", %d)",
                                      quote_lua_string((yyvsp[0].text)).c_str(), (int)(yyvsp[-1].f)));
                 }
#line 2332 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 163:
#line 644 "levcomp.ypp" /* yacc.c:1646  */
    {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("weight(%d)", (int)(yyvsp[0].f)));
                 }
#line 2342 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 164:
#line 651 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2348 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 165:
#line 653 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("orient(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2359 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 166:
#line 662 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("welcome(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2370 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 170:
#line 678 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.mapchunk.add(
                        yylineno,
                        make_stringf("map(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2381 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 174:
#line 694 "levcomp.ypp" /* yacc.c:1646  */
    {
                       lc_map.main.add(
                           yylineno,
                           make_stringf("subvault(\"%s\")",
                               quote_lua_string((yyvsp[0].text)).c_str()));
                   }
#line 2392 "levcomp.tab.cc" /* yacc.c:1646  */
    break;


#line 2396 "levcomp.tab.cc" /* yacc.c:1646  */
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

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
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

  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

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

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


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

#if !defined yyoverflow || YYERROR_VERBOSE
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
  /* Do not reclaim the symbols of the rule whose action triggered
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
  return yyresult;
}
#line 702 "levcomp.ypp" /* yacc.c:1906  */

