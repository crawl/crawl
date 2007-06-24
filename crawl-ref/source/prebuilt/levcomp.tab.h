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
     MONS = 274,
     ITEM = 275,
     PRELUDE = 276,
     MAIN = 277,
     CHARACTER = 278,
     DASH = 279,
     COMMA = 280,
     QUOTE = 281,
     OPAREN = 282,
     CPAREN = 283,
     INTEGER = 284,
     STRING = 285,
     MAP_LINE = 286,
     MONSTER_NAME = 287,
     ITEM_INFO = 288,
     IDENTIFIER = 289,
     LUA_LINE = 290
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
#define MONS 274
#define ITEM 275
#define PRELUDE 276
#define MAIN 277
#define CHARACTER 278
#define DASH 279
#define COMMA 280
#define QUOTE 281
#define OPAREN 282
#define CPAREN 283
#define INTEGER 284
#define STRING 285
#define MAP_LINE 286
#define MONSTER_NAME 287
#define ITEM_INFO 288
#define IDENTIFIER 289
#define LUA_LINE 290




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 56 "levcomp.ypp"
{
    int i;
    const char *text;
    raw_range range;
}
/* Line 1529 of yacc.c.  */
#line 125 "levcomp.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

