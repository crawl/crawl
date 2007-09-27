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
     COLOUR = 273,
     PRELUDE = 274,
     MAIN = 275,
     VALIDATE = 276,
     VETO = 277,
     NSUBST = 278,
     WELCOME = 279,
     COMMA = 280,
     INTEGER = 281,
     CHARACTER = 282,
     STRING = 283,
     MAP_LINE = 284,
     MONSTER_NAME = 285,
     ITEM_INFO = 286,
     LUA_LINE = 287
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
#define COLOUR 273
#define PRELUDE 274
#define MAIN 275
#define VALIDATE 276
#define VETO 277
#define NSUBST 278
#define WELCOME 279
#define COMMA 280
#define INTEGER 281
#define CHARACTER 282
#define STRING 283
#define MAP_LINE 284
#define MONSTER_NAME 285
#define ITEM_INFO 286
#define LUA_LINE 287




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 46 "levcomp.ypp"
{
    int i;
    const char *text;
    raw_range range;
}
/* Line 1529 of yacc.c.  */
#line 119 "levcomp.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

