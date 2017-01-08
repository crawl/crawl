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
#include "l-defs.h"
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

#line 246 "levcomp.tab.cc" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_LEVCOMP_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 261 "levcomp.tab.cc" /* yacc.c:358  */

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
#define YYLAST   113

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  52
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  93
/* YYNRULES -- Number of rules.  */
#define YYNRULES  172
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  204

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
       0,    58,    58,    61,    62,    65,    66,    69,    75,    76,
      79,    89,   110,   111,   114,   115,   118,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   181,   183,   184,   187,   192,   194,
     195,   198,   203,   205,   206,   209,   214,   216,   217,   220,
     225,   227,   228,   231,   236,   238,   239,   242,   247,   248,
     256,   257,   265,   266,   274,   275,   283,   284,   292,   295,
     296,   299,   307,   316,   319,   320,   323,   332,   365,   368,
     369,   377,   378,   386,   387,   396,   397,   406,   408,   409,
     412,   421,   424,   425,   428,   437,   440,   441,   444,   453,
     456,   457,   460,   470,   471,   474,   483,   486,   487,   490,
     499,   502,   503,   506,   515,   516,   519,   520,   523,   531,
     532,   535,   536,   539,   548,   557,   566,   574,   575,   584,
     585,   588,   589,   591,   595,   600,   607,   615,   617,   618,
     620,   627,   635,   636,   645,   654,   657,   658,   661,   670,
     673,   674,   677
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
  "depth", "chance", "chance_specifiers", "chance_roll",
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

#define YYPACT_NINF -120

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-120)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
    -120,     9,    34,  -120,    -9,    -8,  -120,  -120,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,   -11,    -4,  -120,  -120,    -7,
      -5,    -2,  -120,    -1,     2,     3,     4,     5,     6,     7,
       8,    10,    11,    12,    13,    15,    14,  -120,  -120,  -120,
    -120,  -120,    16,    18,    20,    21,    22,    23,    24,    25,
      26,    30,    28,    32,    27,  -120,  -120,  -120,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,  -120,    -3,  -120,  -120,  -120,
      19,  -120,  -120,  -120,  -120,    31,    33,  -120,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,    38,    41,    36,  -120,    37,
      44,  -120,  -120,  -120,    45,  -120,    46,  -120,  -120,  -120,
      47,  -120,    39,    40,    42,    43,    48,  -120,    51,  -120,
    -120,  -120,  -120,  -120,  -120,  -120,    54,  -120,  -120,    55,
    -120,  -120,    56,  -120,  -120,    57,  -120,  -120,    59,  -120,
    -120,  -120,  -120,    -7,    -2,  -120,  -120,  -120,    10,  -120,
    -120,    11,    12,    13,    14,  -120,  -120,  -120,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,    16,    24,    25,    26,    30,
      28,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,  -120,  -120,  -120
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,     0,     0,    55,     4,     5,     8,
       6,    12,     9,    10,    16,    54,    11,    57,    56,     0,
       0,     0,    94,    78,    82,    80,    84,    86,   147,   162,
       0,   150,     0,   139,   134,     0,     0,    71,    59,    63,
      67,    75,     0,     0,    99,   101,   103,   105,     0,     0,
       0,     0,     0,     0,     0,     7,   168,    53,    13,    14,
      48,    50,    51,    49,    52,    41,    43,    42,    44,    45,
      38,    39,    40,    27,    30,    31,    32,    33,    34,    46,
      35,    36,    37,    29,    28,    26,    25,    17,    18,    19,
      20,    21,    22,    23,    24,    15,   165,   167,    47,    91,
      88,    89,    92,   133,   130,   131,    93,    79,    83,    81,
      85,    87,   148,   163,   144,   154,   149,   156,   152,   161,
     157,   159,   143,   140,   142,   138,   135,   137,    97,   125,
      98,   123,    70,    58,    62,    66,    74,   129,   126,   127,
     164,   100,   102,   104,   106,   114,   111,   112,   118,   115,
     116,   122,   119,   120,   172,   169,   170,   110,   107,   108,
     145,   146,   166,     0,     0,    96,    95,   153,     0,   155,
     160,     0,     0,     0,     0,    73,    72,    61,    60,    65,
      64,    69,    68,    77,    76,     0,     0,     0,     0,     0,
       0,    90,   132,   151,   158,   141,   136,   124,   128,   113,
     117,   121,   171,   109
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
    -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,  -120,  -120,  -104,  -120,  -120,
    -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,
    -119,  -120,  -120,   -97,  -120,  -120,   -86,  -120,  -120,   -85,
    -120,   -72,  -120,  -120,   -81,  -120,   -59,  -120,  -120,  -120,
     -67,  -120,   -65,  -120,  -120,  -120,  -120,  -120,  -120,  -120,
    -120,   -60,  -120,  -120,   -62,  -120,  -120,  -120,  -120,    17,
    -120,  -120,   -79
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     7,    57,     8,     9,    10,    16,    58,
      11,    59,    12,    15,    18,    60,   133,   178,    61,   134,
     180,    62,   135,   182,    63,   132,   176,    64,   136,   184,
      65,    66,    67,    68,    69,    70,   100,   101,    71,    72,
     106,   166,    73,    74,    75,    76,    77,    78,    79,   158,
     159,    80,   146,   147,    81,   149,   150,    82,   152,   153,
     130,   131,    83,   138,   139,    84,   104,   105,    85,   126,
     127,    86,   123,   124,    87,    88,    89,    90,    91,   116,
     117,   118,    92,   120,   121,    93,    94,    95,    96,    97,
      98,   155,   156
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      19,    20,    21,    22,    23,    24,    25,    26,    27,     3,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,     4,    13,    14,
      17,    55,   102,    99,    56,    56,   107,     5,   103,   108,
     109,   110,   111,   112,   113,   114,   115,   119,     6,   191,
     163,   122,   128,   125,   129,   140,   137,   141,   142,   143,
     144,   203,   164,   161,   145,   148,   151,   154,   157,   160,
     165,   167,   168,   169,   170,   171,   172,   173,   174,   199,
     175,   177,   185,   179,   181,   186,   187,   188,   189,   183,
     190,   200,   197,   201,   198,   192,   196,   195,   193,   194,
     202,     0,     0,   162
};

static const yytype_int16 yycheck[] =
{
       4,     5,     6,     7,     8,     9,    10,    11,    12,     0,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,     3,    47,    47,
      51,    45,    47,    50,    48,    48,    47,    13,    50,    47,
      47,    47,    47,    47,    47,    47,    46,    46,    24,   163,
      41,    49,    47,    50,    50,    47,    50,    47,    47,    47,
      47,   190,    41,    46,    50,    50,    50,    47,    50,    47,
      47,    43,    41,    47,    47,    41,    41,    41,    41,   186,
      51,    51,    41,    51,    51,    41,    41,    41,    41,    51,
      41,   187,   174,   188,   185,   164,   173,   172,   168,   171,
     189,    -1,    -1,    96
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
     129,   130,   134,   137,   138,   139,   140,   141,   142,    50,
      88,    89,    47,    50,   118,   119,    92,    47,    47,    47,
      47,    47,    47,    47,    47,    46,   131,   132,   133,    46,
     135,   136,    49,   124,   125,    50,   121,   122,    47,    50,
     112,   113,    77,    68,    71,    74,    80,    50,   115,   116,
      47,    47,    47,    47,    47,    50,   104,   105,    50,   107,
     108,    50,   110,   111,    47,   143,   144,    50,   101,   102,
      47,    46,   141,    41,    41,    47,    93,    43,    41,    47,
      47,    41,    41,    41,    41,    51,    78,    51,    69,    51,
      72,    51,    75,    51,    81,    41,    41,    41,    41,    41,
      41,    89,   118,   133,   136,   124,   122,   113,   116,   105,
     108,   111,   144,   102
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
     130,   131,   131,   132,   132,   133,   133,   134,   135,   135,
     136,   136,   137,   137,   138,   139,   140,   140,   141,   142,
     143,   143,   144
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
       1,     3,     1,     2,     1,     2,     1,     2,     3,     1,
       2,     1,     1,     2,     2,     1,     2,     1,     1,     2,
       1,     3,     1
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
#line 58 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1513 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 3:
#line 61 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1519 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 4:
#line 62 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1525 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 5:
#line 65 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1531 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 6:
#line 66 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1537 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 7:
#line 70 "levcomp.ypp" /* yacc.c:1646  */
    {
                    yyerror("Unexpected character sequence.");
                }
#line 1545 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 8:
#line 75 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1551 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 9:
#line 76 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1557 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 10:
#line 80 "levcomp.ypp" /* yacc.c:1646  */
    {
                    dgn_reset_default_depth();
                    string err = dgn_set_default_depth((yyvsp[0].text));
                    if (!err.empty())
                        yyerror(make_stringf("Bad default-depth: %s (%s)",
                                (yyvsp[0].text), err.c_str()).c_str());
                }
#line 1569 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 11:
#line 90 "levcomp.ypp" /* yacc.c:1646  */
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
#line 1592 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 12:
#line 110 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1598 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 13:
#line 111 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1604 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 14:
#line 114 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1610 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 15:
#line 115 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1616 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 16:
#line 119 "levcomp.ypp" /* yacc.c:1646  */
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
#line 1642 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 53:
#line 178 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1648 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 54:
#line 181 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1654 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 55:
#line 183 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1660 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 56:
#line 184 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1666 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 57:
#line 188 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_global_prelude.add(yylineno, (yyvsp[0].text));
                }
#line 1674 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 58:
#line 192 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1680 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 59:
#line 194 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1686 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 60:
#line 195 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1692 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 61:
#line 199 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(yylineno, (yyvsp[0].text));
                }
#line 1700 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 62:
#line 203 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1706 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 63:
#line 205 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1712 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 64:
#line 206 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1718 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 65:
#line 210 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.validate.add(yylineno, (yyvsp[0].text));
                }
#line 1726 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 66:
#line 214 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1732 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 67:
#line 216 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1738 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 68:
#line 217 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1744 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 69:
#line 221 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.veto.add(yylineno, (yyvsp[0].text));
                }
#line 1752 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 70:
#line 225 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1758 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 71:
#line 227 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1764 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 72:
#line 228 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1770 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 73:
#line 232 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.prelude.add(yylineno, (yyvsp[0].text));
                }
#line 1778 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 74:
#line 236 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1784 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 75:
#line 238 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1790 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 76:
#line 239 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1796 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 77:
#line 243 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.epilogue.add(yylineno, (yyvsp[0].text));
                }
#line 1804 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 78:
#line 247 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1810 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 79:
#line 249 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kfeat(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1821 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 80:
#line 256 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1827 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 81:
#line 258 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmons(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1838 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 82:
#line 265 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1844 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 83:
#line 267 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kitem(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1855 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 84:
#line 274 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1861 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 85:
#line 276 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmask(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1872 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 86:
#line 283 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1878 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 87:
#line 285 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kprop(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1889 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 88:
#line 292 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1895 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 91:
#line 300 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("shuffle(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1906 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 92:
#line 308 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("clear(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1917 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 93:
#line 316 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 1923 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 96:
#line 324 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tags(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1934 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 97:
#line 333 "levcomp.ypp" /* yacc.c:1646  */
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
#line 1969 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 98:
#line 365 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1975 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 99:
#line 368 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1981 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 100:
#line 370 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloorcol(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1992 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 101:
#line 377 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 1998 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 102:
#line 379 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrockcol(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2009 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 103:
#line 386 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2015 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 104:
#line 388 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloortile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2026 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 105:
#line 396 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2032 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 106:
#line 398 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrocktile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2043 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 110:
#line 413 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("fheight(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2054 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 114:
#line 429 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("ftile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2065 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 118:
#line 445 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("rtile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2076 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 122:
#line 461 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2087 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 123:
#line 470 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2093 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 124:
#line 471 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2099 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 125:
#line 475 "levcomp.ypp" /* yacc.c:1646  */
    {
                      lc_map.main.add(
                          yylineno,
                          make_stringf("colour(\"%s\")",
                              quote_lua_string((yyvsp[0].text)).c_str()));
                  }
#line 2110 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 126:
#line 483 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2116 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 127:
#line 486 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2122 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 128:
#line 487 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2128 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 129:
#line 491 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("nsubst(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2139 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 130:
#line 499 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2145 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 133:
#line 507 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("subst(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2156 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 134:
#line 515 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2162 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 135:
#line 516 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2168 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 138:
#line 524 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("item(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2179 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 139:
#line 531 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2185 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 140:
#line 532 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2191 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 143:
#line 540 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("mons(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2202 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 144:
#line 549 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("place(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2213 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 145:
#line 558 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("desc(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2224 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 146:
#line 567 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("order(%d)", int((yyvsp[0].f))));
                }
#line 2234 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 147:
#line 574 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2240 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 148:
#line 576 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2251 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 149:
#line 584 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2257 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 150:
#line 585 "levcomp.ypp" /* yacc.c:1646  */
    { }
#line 2263 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 153:
#line 592 "levcomp.ypp" /* yacc.c:1646  */
    {
                    (yyval.i) = (yyvsp[-1].f) * 100;
                }
#line 2271 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 154:
#line 596 "levcomp.ypp" /* yacc.c:1646  */
    {
                    (yyval.i) = (yyvsp[0].f);
                }
#line 2279 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 155:
#line 601 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth_chance(\"%s\", %d)",
                                     quote_lua_string((yyvsp[0].text)).c_str(), (int)(yyvsp[-1].i)));
                }
#line 2290 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 156:
#line 608 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("chance(%d)", (int)(yyvsp[0].i)));
                }
#line 2300 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 160:
#line 621 "levcomp.ypp" /* yacc.c:1646  */
    {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("depth_weight(\"%s\", %d)",
                                      quote_lua_string((yyvsp[0].text)).c_str(), (int)(yyvsp[-1].f)));
                 }
#line 2311 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 161:
#line 628 "levcomp.ypp" /* yacc.c:1646  */
    {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("weight(%d)", (int)(yyvsp[0].f)));
                 }
#line 2321 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 162:
#line 635 "levcomp.ypp" /* yacc.c:1646  */
    {}
#line 2327 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 163:
#line 637 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("orient(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2338 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 164:
#line 646 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("welcome(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2349 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 168:
#line 662 "levcomp.ypp" /* yacc.c:1646  */
    {
                    lc_map.mapchunk.add(
                        yylineno,
                        make_stringf("map(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2360 "levcomp.tab.cc" /* yacc.c:1646  */
    break;

  case 172:
#line 678 "levcomp.ypp" /* yacc.c:1646  */
    {
                       lc_map.main.add(
                           yylineno,
                           make_stringf("subvault(\"%s\")",
                               quote_lua_string((yyvsp[0].text)).c_str()));
                   }
#line 2371 "levcomp.tab.cc" /* yacc.c:1646  */
    break;


#line 2375 "levcomp.tab.cc" /* yacc.c:1646  */
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
#line 686 "levcomp.ypp" /* yacc.c:1906  */

