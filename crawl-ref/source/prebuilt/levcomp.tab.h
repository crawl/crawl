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
     FLOORCOL = 284,
     ROCKCOL = 285,
     FLOORTILE = 286,
     ROCKTILE = 287,
     COMMA = 288,
     COLON = 289,
     PERC = 290,
     INTEGER = 291,
     CHARACTER = 292,
     STRING = 293,
     MAP_LINE = 294,
     MONSTER_NAME = 295,
     ITEM_INFO = 296,
     LUA_LINE = 297
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
#define FLOORCOL 284
#define ROCKCOL 285
#define FLOORTILE 286
#define ROCKTILE 287
#define COMMA 288
#define COLON 289
#define PERC 290
#define INTEGER 291
#define CHARACTER 292
#define STRING 293
#define MAP_LINE 294
#define MONSTER_NAME 295
#define ITEM_INFO 296
#define LUA_LINE 297




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 46 "levcomp.ypp"
{
    int i;
    const char *text;
    raw_range range;
}
/* Line 1529 of yacc.c.  */
#line 139 "levcomp.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

