/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

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
     WEIGHT = 271,
     MONS = 272,
     ITEM = 273,
     MARKER = 274,
     COLOUR = 275,
     PRELUDE = 276,
     MAIN = 277,
     VALIDATE = 278,
     VETO = 279,
     NSUBST = 280,
     WELCOME = 281,
     LFLAGS = 282,
     BFLAGS = 283,
     LFLOORCOL = 284,
     LROCKCOL = 285,
     LFLOORTILE = 286,
     LROCKTILE = 287,
     FTILE = 288,
     RTILE = 289,
     COMMA = 290,
     COLON = 291,
     PERC = 292,
     INTEGER = 293,
     CHARACTER = 294,
     STRING = 295,
     MAP_LINE = 296,
     MONSTER_NAME = 297,
     ITEM_INFO = 298,
     LUA_LINE = 299
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
#define WEIGHT 271
#define MONS 272
#define ITEM 273
#define MARKER 274
#define COLOUR 275
#define PRELUDE 276
#define MAIN 277
#define VALIDATE 278
#define VETO 279
#define NSUBST 280
#define WELCOME 281
#define LFLAGS 282
#define BFLAGS 283
#define LFLOORCOL 284
#define LROCKCOL 285
#define LFLOORTILE 286
#define LROCKTILE 287
#define FTILE 288
#define RTILE 289
#define COMMA 290
#define COLON 291
#define PERC 292
#define INTEGER 293
#define CHARACTER 294
#define STRING 295
#define MAP_LINE 296
#define MONSTER_NAME 297
#define ITEM_INFO 298
#define LUA_LINE 299




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 46 "levcomp.ypp"
{
    int i;
    const char *text;
    raw_range range;
}
/* Line 1529 of yacc.c.  */
#line 143 "levcomp.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

