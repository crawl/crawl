/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "levcomp.ypp"



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

static NORETURN void yyerror(const char *e, bool dlua_error = false)
{
    if (strstr(e, lc_desfile.c_str()) == e)
        fprintf(stderr, "%s\n", e);
    else
        fprintf(stderr, "%s:%d: %s\n", lc_desfile.c_str(), yylineno, e);
    if (dlua_error && !dlua_errors.empty())
        fprintf(stderr, "%s\n", dlua_errors.back().stack_trace.c_str());
    // Bail bail bail.
    end(1);
}


#line 105 "levcomp.tab.cc"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "levcomp.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_DEFAULT_DEPTH = 3,              /* DEFAULT_DEPTH  */
  YYSYMBOL_SHUFFLE = 4,                    /* SHUFFLE  */
  YYSYMBOL_CLEAR = 5,                      /* CLEAR  */
  YYSYMBOL_SUBST = 6,                      /* SUBST  */
  YYSYMBOL_TAGS = 7,                       /* TAGS  */
  YYSYMBOL_KFEAT = 8,                      /* KFEAT  */
  YYSYMBOL_KITEM = 9,                      /* KITEM  */
  YYSYMBOL_KMONS = 10,                     /* KMONS  */
  YYSYMBOL_KMASK = 11,                     /* KMASK  */
  YYSYMBOL_KPROP = 12,                     /* KPROP  */
  YYSYMBOL_NAME = 13,                      /* NAME  */
  YYSYMBOL_DEPTH = 14,                     /* DEPTH  */
  YYSYMBOL_ORIENT = 15,                    /* ORIENT  */
  YYSYMBOL_PLACE = 16,                     /* PLACE  */
  YYSYMBOL_CHANCE = 17,                    /* CHANCE  */
  YYSYMBOL_WEIGHT = 18,                    /* WEIGHT  */
  YYSYMBOL_MONS = 19,                      /* MONS  */
  YYSYMBOL_ITEM = 20,                      /* ITEM  */
  YYSYMBOL_MARKER = 21,                    /* MARKER  */
  YYSYMBOL_COLOUR = 22,                    /* COLOUR  */
  YYSYMBOL_PRELUDE = 23,                   /* PRELUDE  */
  YYSYMBOL_MAIN = 24,                      /* MAIN  */
  YYSYMBOL_VALIDATE = 25,                  /* VALIDATE  */
  YYSYMBOL_VETO = 26,                      /* VETO  */
  YYSYMBOL_EPILOGUE = 27,                  /* EPILOGUE  */
  YYSYMBOL_NSUBST = 28,                    /* NSUBST  */
  YYSYMBOL_WELCOME = 29,                   /* WELCOME  */
  YYSYMBOL_LFLOORCOL = 30,                 /* LFLOORCOL  */
  YYSYMBOL_LROCKCOL = 31,                  /* LROCKCOL  */
  YYSYMBOL_LFLOORTILE = 32,                /* LFLOORTILE  */
  YYSYMBOL_LROCKTILE = 33,                 /* LROCKTILE  */
  YYSYMBOL_FTILE = 34,                     /* FTILE  */
  YYSYMBOL_RTILE = 35,                     /* RTILE  */
  YYSYMBOL_TILE = 36,                      /* TILE  */
  YYSYMBOL_SUBVAULT = 37,                  /* SUBVAULT  */
  YYSYMBOL_FHEIGHT = 38,                   /* FHEIGHT  */
  YYSYMBOL_DESC = 39,                      /* DESC  */
  YYSYMBOL_ORDER = 40,                     /* ORDER  */
  YYSYMBOL_COMMA = 41,                     /* COMMA  */
  YYSYMBOL_COLON = 42,                     /* COLON  */
  YYSYMBOL_PERC = 43,                      /* PERC  */
  YYSYMBOL_DASH = 44,                      /* DASH  */
  YYSYMBOL_CHARACTER = 45,                 /* CHARACTER  */
  YYSYMBOL_NUMBER = 46,                    /* NUMBER  */
  YYSYMBOL_STRING = 47,                    /* STRING  */
  YYSYMBOL_MAP_LINE = 48,                  /* MAP_LINE  */
  YYSYMBOL_MONSTER_NAME = 49,              /* MONSTER_NAME  */
  YYSYMBOL_ITEM_INFO = 50,                 /* ITEM_INFO  */
  YYSYMBOL_LUA_LINE = 51,                  /* LUA_LINE  */
  YYSYMBOL_YYACCEPT = 52,                  /* $accept  */
  YYSYMBOL_file = 53,                      /* file  */
  YYSYMBOL_definitions = 54,               /* definitions  */
  YYSYMBOL_definition = 55,                /* definition  */
  YYSYMBOL_error_seq = 56,                 /* error_seq  */
  YYSYMBOL_def = 57,                       /* def  */
  YYSYMBOL_defdepth = 58,                  /* defdepth  */
  YYSYMBOL_level = 59,                     /* level  */
  YYSYMBOL_map_specs = 60,                 /* map_specs  */
  YYSYMBOL_map_spec = 61,                  /* map_spec  */
  YYSYMBOL_name = 62,                      /* name  */
  YYSYMBOL_metaline = 63,                  /* metaline  */
  YYSYMBOL_global_lua = 64,                /* global_lua  */
  YYSYMBOL_global_lua_lines = 65,          /* global_lua_lines  */
  YYSYMBOL_global_lua_line = 66,           /* global_lua_line  */
  YYSYMBOL_main_lua = 67,                  /* main_lua  */
  YYSYMBOL_main_lua_lines = 68,            /* main_lua_lines  */
  YYSYMBOL_main_lua_line = 69,             /* main_lua_line  */
  YYSYMBOL_validate_lua = 70,              /* validate_lua  */
  YYSYMBOL_validate_lua_lines = 71,        /* validate_lua_lines  */
  YYSYMBOL_validate_lua_line = 72,         /* validate_lua_line  */
  YYSYMBOL_veto_lua = 73,                  /* veto_lua  */
  YYSYMBOL_veto_lua_lines = 74,            /* veto_lua_lines  */
  YYSYMBOL_veto_lua_line = 75,             /* veto_lua_line  */
  YYSYMBOL_prelude_lua = 76,               /* prelude_lua  */
  YYSYMBOL_prelude_lua_lines = 77,         /* prelude_lua_lines  */
  YYSYMBOL_prelude_lua_line = 78,          /* prelude_lua_line  */
  YYSYMBOL_epilogue_lua = 79,              /* epilogue_lua  */
  YYSYMBOL_epilogue_lua_lines = 80,        /* epilogue_lua_lines  */
  YYSYMBOL_epilogue_lua_line = 81,         /* epilogue_lua_line  */
  YYSYMBOL_kfeat = 82,                     /* kfeat  */
  YYSYMBOL_kmons = 83,                     /* kmons  */
  YYSYMBOL_kitem = 84,                     /* kitem  */
  YYSYMBOL_kmask = 85,                     /* kmask  */
  YYSYMBOL_kprop = 86,                     /* kprop  */
  YYSYMBOL_shuffle = 87,                   /* shuffle  */
  YYSYMBOL_shuffle_specifiers = 88,        /* shuffle_specifiers  */
  YYSYMBOL_shuffle_spec = 89,              /* shuffle_spec  */
  YYSYMBOL_clear = 90,                     /* clear  */
  YYSYMBOL_tags = 91,                      /* tags  */
  YYSYMBOL_tagstrings = 92,                /* tagstrings  */
  YYSYMBOL_tagstring = 93,                 /* tagstring  */
  YYSYMBOL_marker = 94,                    /* marker  */
  YYSYMBOL_colour = 95,                    /* colour  */
  YYSYMBOL_lfloorcol = 96,                 /* lfloorcol  */
  YYSYMBOL_lrockcol = 97,                  /* lrockcol  */
  YYSYMBOL_lfloortile = 98,                /* lfloortile  */
  YYSYMBOL_lrocktile = 99,                 /* lrocktile  */
  YYSYMBOL_fheight = 100,                  /* fheight  */
  YYSYMBOL_fheight_specifiers = 101,       /* fheight_specifiers  */
  YYSYMBOL_fheight_specifier = 102,        /* fheight_specifier  */
  YYSYMBOL_ftile = 103,                    /* ftile  */
  YYSYMBOL_ftile_specifiers = 104,         /* ftile_specifiers  */
  YYSYMBOL_ftile_specifier = 105,          /* ftile_specifier  */
  YYSYMBOL_rtile = 106,                    /* rtile  */
  YYSYMBOL_rtile_specifiers = 107,         /* rtile_specifiers  */
  YYSYMBOL_rtile_specifier = 108,          /* rtile_specifier  */
  YYSYMBOL_tile = 109,                     /* tile  */
  YYSYMBOL_tile_specifiers = 110,          /* tile_specifiers  */
  YYSYMBOL_tile_specifier = 111,           /* tile_specifier  */
  YYSYMBOL_colour_specifiers = 112,        /* colour_specifiers  */
  YYSYMBOL_colour_specifier = 113,         /* colour_specifier  */
  YYSYMBOL_nsubst = 114,                   /* nsubst  */
  YYSYMBOL_nsubst_specifiers = 115,        /* nsubst_specifiers  */
  YYSYMBOL_nsubst_spec = 116,              /* nsubst_spec  */
  YYSYMBOL_subst = 117,                    /* subst  */
  YYSYMBOL_subst_specifiers = 118,         /* subst_specifiers  */
  YYSYMBOL_subst_spec = 119,               /* subst_spec  */
  YYSYMBOL_items = 120,                    /* items  */
  YYSYMBOL_item_specifiers = 121,          /* item_specifiers  */
  YYSYMBOL_item_specifier = 122,           /* item_specifier  */
  YYSYMBOL_mons = 123,                     /* mons  */
  YYSYMBOL_mnames = 124,                   /* mnames  */
  YYSYMBOL_mname = 125,                    /* mname  */
  YYSYMBOL_place = 126,                    /* place  */
  YYSYMBOL_desc = 127,                     /* desc  */
  YYSYMBOL_order = 128,                    /* order  */
  YYSYMBOL_depth = 129,                    /* depth  */
  YYSYMBOL_chance = 130,                   /* chance  */
  YYSYMBOL_chance_specifiers = 131,        /* chance_specifiers  */
  YYSYMBOL_chance_roll = 132,              /* chance_roll  */
  YYSYMBOL_chance_specifier = 133,         /* chance_specifier  */
  YYSYMBOL_weight = 134,                   /* weight  */
  YYSYMBOL_weight_specifiers = 135,        /* weight_specifiers  */
  YYSYMBOL_weight_specifier = 136,         /* weight_specifier  */
  YYSYMBOL_orientation = 137,              /* orientation  */
  YYSYMBOL_welcome = 138,                  /* welcome  */
  YYSYMBOL_map_def = 139,                  /* map_def  */
  YYSYMBOL_map_lines = 140,                /* map_lines  */
  YYSYMBOL_map_line = 141,                 /* map_line  */
  YYSYMBOL_subvault = 142,                 /* subvault  */
  YYSYMBOL_subvault_specifiers = 143,      /* subvault_specifiers  */
  YYSYMBOL_subvault_specifier = 144        /* subvault_specifier  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

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


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
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

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

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
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
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
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
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

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   306


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
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
static const yytype_int16 yyrline[] =
{
       0,    60,    60,    63,    64,    67,    68,    71,    77,    78,
      81,    91,   111,   112,   115,   116,   119,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   182,   184,   185,   188,   193,   195,
     196,   199,   204,   206,   207,   210,   215,   217,   218,   221,
     226,   228,   229,   232,   237,   239,   240,   243,   248,   249,
     257,   258,   266,   267,   275,   276,   284,   285,   293,   296,
     297,   300,   308,   317,   320,   321,   324,   333,   366,   369,
     370,   378,   379,   387,   388,   397,   398,   407,   409,   410,
     413,   422,   425,   426,   429,   438,   441,   442,   445,   454,
     457,   458,   461,   471,   472,   475,   484,   487,   488,   491,
     500,   503,   504,   507,   516,   517,   520,   521,   524,   532,
     533,   536,   537,   540,   549,   558,   567,   575,   576,   585,
     586,   589,   590,   592,   596,   601,   608,   616,   618,   619,
     621,   628,   636,   637,   646,   655,   658,   659,   662,   671,
     674,   675,   678
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "DEFAULT_DEPTH",
  "SHUFFLE", "CLEAR", "SUBST", "TAGS", "KFEAT", "KITEM", "KMONS", "KMASK",
  "KPROP", "NAME", "DEPTH", "ORIENT", "PLACE", "CHANCE", "WEIGHT", "MONS",
  "ITEM", "MARKER", "COLOUR", "PRELUDE", "MAIN", "VALIDATE", "VETO",
  "EPILOGUE", "NSUBST", "WELCOME", "LFLOORCOL", "LROCKCOL", "LFLOORTILE",
  "LROCKTILE", "FTILE", "RTILE", "TILE", "SUBVAULT", "FHEIGHT", "DESC",
  "ORDER", "COMMA", "COLON", "PERC", "DASH", "CHARACTER", "NUMBER",
  "STRING", "MAP_LINE", "MONSTER_NAME", "ITEM_INFO", "LUA_LINE", "$accept",
  "file", "definitions", "definition", "error_seq", "def", "defdepth",
  "level", "map_specs", "map_spec", "name", "metaline", "global_lua",
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

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-120)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
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
static const yytype_uint8 yydefgoto[] =
{
       0,     1,     2,     7,    57,     8,     9,    10,    16,    58,
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

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
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

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
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

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
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


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
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

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


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




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
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
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
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
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
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






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
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
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


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

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
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
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
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
| yyreduce -- do a reduction.  |
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
  case 2: /* file: definitions  */
#line 60 "levcomp.ypp"
                              { }
#line 1404 "levcomp.tab.cc"
    break;

  case 3: /* definitions: %empty  */
#line 63 "levcomp.ypp"
                                            {}
#line 1410 "levcomp.tab.cc"
    break;

  case 4: /* definitions: definitions definition  */
#line 64 "levcomp.ypp"
                                            {}
#line 1416 "levcomp.tab.cc"
    break;

  case 5: /* definition: def  */
#line 67 "levcomp.ypp"
                            {}
#line 1422 "levcomp.tab.cc"
    break;

  case 6: /* definition: level  */
#line 68 "levcomp.ypp"
                            {}
#line 1428 "levcomp.tab.cc"
    break;

  case 7: /* error_seq: CHARACTER  */
#line 72 "levcomp.ypp"
                {
                    yyerror("Unexpected character sequence.");
                }
#line 1436 "levcomp.tab.cc"
    break;

  case 8: /* def: defdepth  */
#line 77 "levcomp.ypp"
                            {}
#line 1442 "levcomp.tab.cc"
    break;

  case 9: /* def: global_lua  */
#line 78 "levcomp.ypp"
                              {}
#line 1448 "levcomp.tab.cc"
    break;

  case 10: /* defdepth: DEFAULT_DEPTH STRING  */
#line 82 "levcomp.ypp"
                {
                    dgn_reset_default_depth();
                    string err = dgn_set_default_depth((yyvsp[0].text));
                    if (!err.empty())
                        yyerror(make_stringf("Bad default-depth: %s (%s)",
                                (yyvsp[0].text), err.c_str()).c_str());
                }
#line 1460 "levcomp.tab.cc"
    break;

  case 11: /* level: name map_specs  */
#line 92 "levcomp.ypp"
                {
                    lc_map.set_file(lc_desfile);

                    if (lc_run_global_prelude && !lc_global_prelude.empty())
                    {
                        lc_global_prelude.set_file(lc_desfile);
                        lc_run_global_prelude = false;
                        if (lc_global_prelude.load_call(dlua, nullptr))
                            yyerror( lc_global_prelude.orig_error().c_str() );
                    }

                    string err = lc_map.validate_map_def();
                    dump_map(lc_map);
                    if (!err.empty())
                        yyerror(err.c_str(), true);
                    add_parsed_map(lc_map);
                }
#line 1482 "levcomp.tab.cc"
    break;

  case 12: /* map_specs: %empty  */
#line 111 "levcomp.ypp"
                                     { }
#line 1488 "levcomp.tab.cc"
    break;

  case 13: /* map_specs: map_specs map_spec  */
#line 112 "levcomp.ypp"
                                     { }
#line 1494 "levcomp.tab.cc"
    break;

  case 14: /* map_spec: metaline  */
#line 115 "levcomp.ypp"
                           { }
#line 1500 "levcomp.tab.cc"
    break;

  case 15: /* map_spec: map_def  */
#line 116 "levcomp.ypp"
                           { }
#line 1506 "levcomp.tab.cc"
    break;

  case 16: /* name: NAME STRING  */
#line 120 "levcomp.ypp"
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
#line 1532 "levcomp.tab.cc"
    break;

  case 53: /* metaline: error_seq  */
#line 179 "levcomp.ypp"
                            {}
#line 1538 "levcomp.tab.cc"
    break;

  case 54: /* global_lua: MAIN global_lua_lines  */
#line 182 "levcomp.ypp"
                                        { }
#line 1544 "levcomp.tab.cc"
    break;

  case 55: /* global_lua_lines: %empty  */
#line 184 "levcomp.ypp"
                                { }
#line 1550 "levcomp.tab.cc"
    break;

  case 56: /* global_lua_lines: global_lua_lines global_lua_line  */
#line 185 "levcomp.ypp"
                                                   { }
#line 1556 "levcomp.tab.cc"
    break;

  case 57: /* global_lua_line: LUA_LINE  */
#line 189 "levcomp.ypp"
                {
                    lc_global_prelude.add(yylineno, (yyvsp[0].text));
                }
#line 1564 "levcomp.tab.cc"
    break;

  case 58: /* main_lua: MAIN main_lua_lines  */
#line 193 "levcomp.ypp"
                                      { }
#line 1570 "levcomp.tab.cc"
    break;

  case 59: /* main_lua_lines: %empty  */
#line 195 "levcomp.ypp"
                              { }
#line 1576 "levcomp.tab.cc"
    break;

  case 60: /* main_lua_lines: main_lua_lines main_lua_line  */
#line 196 "levcomp.ypp"
                                               { }
#line 1582 "levcomp.tab.cc"
    break;

  case 61: /* main_lua_line: LUA_LINE  */
#line 200 "levcomp.ypp"
                {
                    lc_map.main.add(yylineno, (yyvsp[0].text));
                }
#line 1590 "levcomp.tab.cc"
    break;

  case 62: /* validate_lua: VALIDATE validate_lua_lines  */
#line 204 "levcomp.ypp"
                                              { }
#line 1596 "levcomp.tab.cc"
    break;

  case 63: /* validate_lua_lines: %empty  */
#line 206 "levcomp.ypp"
                                  { }
#line 1602 "levcomp.tab.cc"
    break;

  case 64: /* validate_lua_lines: validate_lua_lines validate_lua_line  */
#line 207 "levcomp.ypp"
                                                       { }
#line 1608 "levcomp.tab.cc"
    break;

  case 65: /* validate_lua_line: LUA_LINE  */
#line 211 "levcomp.ypp"
                {
                    lc_map.validate.add(yylineno, (yyvsp[0].text));
                }
#line 1616 "levcomp.tab.cc"
    break;

  case 66: /* veto_lua: VETO veto_lua_lines  */
#line 215 "levcomp.ypp"
                                      { }
#line 1622 "levcomp.tab.cc"
    break;

  case 67: /* veto_lua_lines: %empty  */
#line 217 "levcomp.ypp"
                              { }
#line 1628 "levcomp.tab.cc"
    break;

  case 68: /* veto_lua_lines: veto_lua_lines veto_lua_line  */
#line 218 "levcomp.ypp"
                                               { }
#line 1634 "levcomp.tab.cc"
    break;

  case 69: /* veto_lua_line: LUA_LINE  */
#line 222 "levcomp.ypp"
                {
                    lc_map.veto.add(yylineno, (yyvsp[0].text));
                }
#line 1642 "levcomp.tab.cc"
    break;

  case 70: /* prelude_lua: PRELUDE prelude_lua_lines  */
#line 226 "levcomp.ypp"
                                            { }
#line 1648 "levcomp.tab.cc"
    break;

  case 71: /* prelude_lua_lines: %empty  */
#line 228 "levcomp.ypp"
                                { }
#line 1654 "levcomp.tab.cc"
    break;

  case 72: /* prelude_lua_lines: prelude_lua_lines prelude_lua_line  */
#line 229 "levcomp.ypp"
                                                     { }
#line 1660 "levcomp.tab.cc"
    break;

  case 73: /* prelude_lua_line: LUA_LINE  */
#line 233 "levcomp.ypp"
                {
                    lc_map.prelude.add(yylineno, (yyvsp[0].text));
                }
#line 1668 "levcomp.tab.cc"
    break;

  case 74: /* epilogue_lua: EPILOGUE epilogue_lua_lines  */
#line 237 "levcomp.ypp"
                                              { }
#line 1674 "levcomp.tab.cc"
    break;

  case 75: /* epilogue_lua_lines: %empty  */
#line 239 "levcomp.ypp"
                                 { }
#line 1680 "levcomp.tab.cc"
    break;

  case 76: /* epilogue_lua_lines: epilogue_lua_lines epilogue_lua_line  */
#line 240 "levcomp.ypp"
                                                       { }
#line 1686 "levcomp.tab.cc"
    break;

  case 77: /* epilogue_lua_line: LUA_LINE  */
#line 244 "levcomp.ypp"
                {
                    lc_map.epilogue.add(yylineno, (yyvsp[0].text));
                }
#line 1694 "levcomp.tab.cc"
    break;

  case 78: /* kfeat: KFEAT  */
#line 248 "levcomp.ypp"
                        { }
#line 1700 "levcomp.tab.cc"
    break;

  case 79: /* kfeat: KFEAT STRING  */
#line 250 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kfeat(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1711 "levcomp.tab.cc"
    break;

  case 80: /* kmons: KMONS  */
#line 257 "levcomp.ypp"
                        { }
#line 1717 "levcomp.tab.cc"
    break;

  case 81: /* kmons: KMONS STRING  */
#line 259 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmons(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1728 "levcomp.tab.cc"
    break;

  case 82: /* kitem: KITEM  */
#line 266 "levcomp.ypp"
                        { }
#line 1734 "levcomp.tab.cc"
    break;

  case 83: /* kitem: KITEM STRING  */
#line 268 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kitem(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1745 "levcomp.tab.cc"
    break;

  case 84: /* kmask: KMASK  */
#line 275 "levcomp.ypp"
                        { }
#line 1751 "levcomp.tab.cc"
    break;

  case 85: /* kmask: KMASK STRING  */
#line 277 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kmask(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1762 "levcomp.tab.cc"
    break;

  case 86: /* kprop: KPROP  */
#line 284 "levcomp.ypp"
                        { }
#line 1768 "levcomp.tab.cc"
    break;

  case 87: /* kprop: KPROP STRING  */
#line 286 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("kprop(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1779 "levcomp.tab.cc"
    break;

  case 88: /* shuffle: SHUFFLE shuffle_specifiers  */
#line 293 "levcomp.ypp"
                                             {}
#line 1785 "levcomp.tab.cc"
    break;

  case 91: /* shuffle_spec: ITEM_INFO  */
#line 301 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("shuffle(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1796 "levcomp.tab.cc"
    break;

  case 92: /* clear: CLEAR STRING  */
#line 309 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("clear(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1807 "levcomp.tab.cc"
    break;

  case 93: /* tags: TAGS tagstrings  */
#line 317 "levcomp.ypp"
                                  {}
#line 1813 "levcomp.tab.cc"
    break;

  case 96: /* tagstring: STRING  */
#line 325 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tags(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1824 "levcomp.tab.cc"
    break;

  case 97: /* marker: MARKER STRING  */
#line 334 "levcomp.ypp"
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
#line 1859 "levcomp.tab.cc"
    break;

  case 98: /* colour: COLOUR colour_specifiers  */
#line 366 "levcomp.ypp"
                                           { }
#line 1865 "levcomp.tab.cc"
    break;

  case 99: /* lfloorcol: LFLOORCOL  */
#line 369 "levcomp.ypp"
                            { }
#line 1871 "levcomp.tab.cc"
    break;

  case 100: /* lfloorcol: LFLOORCOL STRING  */
#line 371 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloorcol(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1882 "levcomp.tab.cc"
    break;

  case 101: /* lrockcol: LROCKCOL  */
#line 378 "levcomp.ypp"
                           { }
#line 1888 "levcomp.tab.cc"
    break;

  case 102: /* lrockcol: LROCKCOL STRING  */
#line 380 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrockcol(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1899 "levcomp.tab.cc"
    break;

  case 103: /* lfloortile: LFLOORTILE  */
#line 387 "levcomp.ypp"
                             { }
#line 1905 "levcomp.tab.cc"
    break;

  case 104: /* lfloortile: LFLOORTILE STRING  */
#line 389 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lfloortile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1916 "levcomp.tab.cc"
    break;

  case 105: /* lrocktile: LROCKTILE  */
#line 397 "levcomp.ypp"
                            { }
#line 1922 "levcomp.tab.cc"
    break;

  case 106: /* lrocktile: LROCKTILE STRING  */
#line 399 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("lrocktile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1933 "levcomp.tab.cc"
    break;

  case 110: /* fheight_specifier: ITEM_INFO  */
#line 414 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("fheight(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1944 "levcomp.tab.cc"
    break;

  case 114: /* ftile_specifier: ITEM_INFO  */
#line 430 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("ftile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1955 "levcomp.tab.cc"
    break;

  case 118: /* rtile_specifier: ITEM_INFO  */
#line 446 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("rtile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1966 "levcomp.tab.cc"
    break;

  case 122: /* tile_specifier: ITEM_INFO  */
#line 462 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("tile(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 1977 "levcomp.tab.cc"
    break;

  case 123: /* colour_specifiers: colour_specifier  */
#line 471 "levcomp.ypp"
                                      { }
#line 1983 "levcomp.tab.cc"
    break;

  case 124: /* colour_specifiers: colour_specifiers COMMA colour_specifier  */
#line 472 "levcomp.ypp"
                                                             { }
#line 1989 "levcomp.tab.cc"
    break;

  case 125: /* colour_specifier: ITEM_INFO  */
#line 476 "levcomp.ypp"
                  {
                      lc_map.main.add(
                          yylineno,
                          make_stringf("colour(\"%s\")",
                              quote_lua_string((yyvsp[0].text)).c_str()));
                  }
#line 2000 "levcomp.tab.cc"
    break;

  case 126: /* nsubst: NSUBST nsubst_specifiers  */
#line 484 "levcomp.ypp"
                                           { }
#line 2006 "levcomp.tab.cc"
    break;

  case 127: /* nsubst_specifiers: nsubst_spec  */
#line 487 "levcomp.ypp"
                                 { }
#line 2012 "levcomp.tab.cc"
    break;

  case 128: /* nsubst_specifiers: nsubst_specifiers COMMA nsubst_spec  */
#line 488 "levcomp.ypp"
                                                        { }
#line 2018 "levcomp.tab.cc"
    break;

  case 129: /* nsubst_spec: ITEM_INFO  */
#line 492 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("nsubst(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2029 "levcomp.tab.cc"
    break;

  case 130: /* subst: SUBST subst_specifiers  */
#line 500 "levcomp.ypp"
                                         { }
#line 2035 "levcomp.tab.cc"
    break;

  case 133: /* subst_spec: ITEM_INFO  */
#line 508 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("subst(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2046 "levcomp.tab.cc"
    break;

  case 134: /* items: ITEM  */
#line 516 "levcomp.ypp"
                       {}
#line 2052 "levcomp.tab.cc"
    break;

  case 135: /* items: ITEM item_specifiers  */
#line 517 "levcomp.ypp"
                                       {}
#line 2058 "levcomp.tab.cc"
    break;

  case 138: /* item_specifier: ITEM_INFO  */
#line 525 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("item(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2069 "levcomp.tab.cc"
    break;

  case 139: /* mons: MONS  */
#line 532 "levcomp.ypp"
                       {}
#line 2075 "levcomp.tab.cc"
    break;

  case 140: /* mons: MONS mnames  */
#line 533 "levcomp.ypp"
                              {}
#line 2081 "levcomp.tab.cc"
    break;

  case 143: /* mname: MONSTER_NAME  */
#line 541 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("mons(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2092 "levcomp.tab.cc"
    break;

  case 144: /* place: PLACE STRING  */
#line 550 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("place(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2103 "levcomp.tab.cc"
    break;

  case 145: /* desc: DESC STRING  */
#line 559 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("desc(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2114 "levcomp.tab.cc"
    break;

  case 146: /* order: ORDER NUMBER  */
#line 568 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("order(%d)", int((yyvsp[0].f))));
                }
#line 2124 "levcomp.tab.cc"
    break;

  case 147: /* depth: DEPTH  */
#line 575 "levcomp.ypp"
                        {}
#line 2130 "levcomp.tab.cc"
    break;

  case 148: /* depth: DEPTH STRING  */
#line 577 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth(\"%s\")",
                            quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2141 "levcomp.tab.cc"
    break;

  case 149: /* chance: CHANCE chance_specifiers  */
#line 585 "levcomp.ypp"
                                           { }
#line 2147 "levcomp.tab.cc"
    break;

  case 150: /* chance: CHANCE  */
#line 586 "levcomp.ypp"
                         { }
#line 2153 "levcomp.tab.cc"
    break;

  case 153: /* chance_roll: NUMBER PERC  */
#line 593 "levcomp.ypp"
                {
                    (yyval.i) = (yyvsp[-1].f) * 100;
                }
#line 2161 "levcomp.tab.cc"
    break;

  case 154: /* chance_roll: NUMBER  */
#line 597 "levcomp.ypp"
                {
                    (yyval.i) = (yyvsp[0].f);
                }
#line 2169 "levcomp.tab.cc"
    break;

  case 155: /* chance_specifier: chance_roll STRING  */
#line 602 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("depth_chance(\"%s\", %d)",
                                     quote_lua_string((yyvsp[0].text)).c_str(), (int)(yyvsp[-1].i)));
                }
#line 2180 "levcomp.tab.cc"
    break;

  case 156: /* chance_specifier: chance_roll  */
#line 609 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("chance(%d)", (int)(yyvsp[0].i)));
                }
#line 2190 "levcomp.tab.cc"
    break;

  case 160: /* weight_specifier: NUMBER STRING  */
#line 622 "levcomp.ypp"
                 {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("depth_weight(\"%s\", %d)",
                                      quote_lua_string((yyvsp[0].text)).c_str(), (int)(yyvsp[-1].f)));
                 }
#line 2201 "levcomp.tab.cc"
    break;

  case 161: /* weight_specifier: NUMBER  */
#line 629 "levcomp.ypp"
                 {
                     lc_map.main.add(
                         yylineno,
                         make_stringf("weight(%d)", (int)(yyvsp[0].f)));
                 }
#line 2211 "levcomp.tab.cc"
    break;

  case 162: /* orientation: ORIENT  */
#line 636 "levcomp.ypp"
                         {}
#line 2217 "levcomp.tab.cc"
    break;

  case 163: /* orientation: ORIENT STRING  */
#line 638 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("orient(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2228 "levcomp.tab.cc"
    break;

  case 164: /* welcome: WELCOME STRING  */
#line 647 "levcomp.ypp"
                {
                    lc_map.main.add(
                        yylineno,
                        make_stringf("welcome(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2239 "levcomp.tab.cc"
    break;

  case 168: /* map_line: MAP_LINE  */
#line 663 "levcomp.ypp"
                {
                    lc_map.mapchunk.add(
                        yylineno,
                        make_stringf("map(\"%s\")",
                                     quote_lua_string((yyvsp[0].text)).c_str()));
                }
#line 2250 "levcomp.tab.cc"
    break;

  case 172: /* subvault_specifier: STRING  */
#line 679 "levcomp.ypp"
                   {
                       lc_map.main.add(
                           yylineno,
                           make_stringf("subvault(\"%s\")",
                               quote_lua_string((yyvsp[0].text)).c_str()));
                   }
#line 2261 "levcomp.tab.cc"
    break;


#line 2265 "levcomp.tab.cc"

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
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
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
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

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

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
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
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
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
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 687 "levcomp.ypp"

