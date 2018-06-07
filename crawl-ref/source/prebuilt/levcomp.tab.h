/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison interface for Yacc-like parsers in C

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
#line 34 "levcomp.ypp" /* yacc.c:1909  */

    int i;
    double f;
    const char *text;

#line 162 "levcomp.tab.h" /* yacc.c:1909  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_LEVCOMP_TAB_H_INCLUDED  */
