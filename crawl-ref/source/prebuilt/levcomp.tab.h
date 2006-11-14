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
     SYMBOL = 259,
     TAGS = 260,
     NAME = 261,
     DEPTH = 262,
     ORIENT = 263,
     PLACE = 264,
     CHANCE = 265,
     FLAGS = 266,
     MONS = 267,
     ENCOMPASS = 268,
     NORTH = 269,
     EAST = 270,
     SOUTH = 271,
     WEST = 272,
     NORTH_DIS = 273,
     NORTHEAST = 274,
     SOUTHEAST = 275,
     SOUTHWEST = 276,
     NORTHWEST = 277,
     BAD_CHARACTER = 278,
     NO_HMIRROR = 279,
     NO_VMIRROR = 280,
     NO_ROTATE = 281,
     PANDEMONIC = 282,
     DASH = 283,
     COMMA = 284,
     INTEGER = 285,
     STRING = 286,
     MAP_LINE = 287,
     MONSTER_NAME = 288
   };
#endif
/* Tokens.  */
#define DEFAULT_DEPTH 258
#define SYMBOL 259
#define TAGS 260
#define NAME 261
#define DEPTH 262
#define ORIENT 263
#define PLACE 264
#define CHANCE 265
#define FLAGS 266
#define MONS 267
#define ENCOMPASS 268
#define NORTH 269
#define EAST 270
#define SOUTH 271
#define WEST 272
#define NORTH_DIS 273
#define NORTHEAST 274
#define SOUTHEAST 275
#define SOUTHWEST 276
#define NORTHWEST 277
#define BAD_CHARACTER 278
#define NO_HMIRROR 279
#define NO_VMIRROR 280
#define NO_ROTATE 281
#define PANDEMONIC 282
#define DASH 283
#define COMMA 284
#define INTEGER 285
#define STRING 286
#define MAP_LINE 287
#define MONSTER_NAME 288




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 21 "levcomp.ypp"
{
    int i;
    const char *text;
}
/* Line 1529 of yacc.c.  */
#line 120 "levcomp.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

