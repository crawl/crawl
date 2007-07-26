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
     NAME = 265,
     DEPTH = 266,
     ORIENT = 267,
     PLACE = 268,
     CHANCE = 269,
     MONS = 270,
     ITEM = 271,
     MARKER = 272,
     PRELUDE = 273,
     MAIN = 274,
     VALIDATE = 275,
     VETO = 276,
     NSUBST = 277,
     COMMA = 278,
     INTEGER = 279,
     CHARACTER = 280,
     STRING = 281,
     MAP_LINE = 282,
     MONSTER_NAME = 283,
     ITEM_INFO = 284,
     LUA_LINE = 285
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
#define NAME 265
#define DEPTH 266
#define ORIENT 267
#define PLACE 268
#define CHANCE 269
#define MONS 270
#define ITEM 271
#define MARKER 272
#define PRELUDE 273
#define MAIN 274
#define VALIDATE 275
#define VETO 276
#define NSUBST 277
#define COMMA 278
#define INTEGER 279
#define CHARACTER 280
#define STRING 281
#define MAP_LINE 282
#define MONSTER_NAME 283
#define ITEM_INFO 284
#define LUA_LINE 285




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 46 "levcomp.ypp"
{
    int i;
    const char *text;
    raw_range range;
}
/* Line 1529 of yacc.c.  */
#line 115 "levcomp.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

