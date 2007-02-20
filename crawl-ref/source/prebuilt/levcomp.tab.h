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
     NAME = 266,
     DEPTH = 267,
     ORIENT = 268,
     PLACE = 269,
     CHANCE = 270,
     FLAGS = 271,
     MONS = 272,
     ITEM = 273,
     ROOT_DEPTH = 274,
     ENTRY_MSG = 275,
     EXIT_MSG = 276,
     ROCK_COLOUR = 277,
     FLOOR_COLOUR = 278,
     ENCOMPASS = 279,
     FLOAT = 280,
     NORTH = 281,
     EAST = 282,
     SOUTH = 283,
     WEST = 284,
     NORTHEAST = 285,
     SOUTHEAST = 286,
     SOUTHWEST = 287,
     NORTHWEST = 288,
     LEVEL = 289,
     END = 290,
     PVAULT = 291,
     PMINIVAULT = 292,
     MONSTERS = 293,
     ENDMONSTERS = 294,
     CHARACTER = 295,
     NO_HMIRROR = 296,
     NO_VMIRROR = 297,
     NO_ROTATE = 298,
     PANDEMONIC = 299,
     DASH = 300,
     COMMA = 301,
     QUOTE = 302,
     OPAREN = 303,
     CPAREN = 304,
     INTEGER = 305,
     STRING = 306,
     MAP_LINE = 307,
     MONSTER_NAME = 308,
     ITEM_INFO = 309,
     IDENTIFIER = 310
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
#define NAME 266
#define DEPTH 267
#define ORIENT 268
#define PLACE 269
#define CHANCE 270
#define FLAGS 271
#define MONS 272
#define ITEM 273
#define ROOT_DEPTH 274
#define ENTRY_MSG 275
#define EXIT_MSG 276
#define ROCK_COLOUR 277
#define FLOOR_COLOUR 278
#define ENCOMPASS 279
#define FLOAT 280
#define NORTH 281
#define EAST 282
#define SOUTH 283
#define WEST 284
#define NORTHEAST 285
#define SOUTHEAST 286
#define SOUTHWEST 287
#define NORTHWEST 288
#define LEVEL 289
#define END 290
#define PVAULT 291
#define PMINIVAULT 292
#define MONSTERS 293
#define ENDMONSTERS 294
#define CHARACTER 295
#define NO_HMIRROR 296
#define NO_VMIRROR 297
#define NO_ROTATE 298
#define PANDEMONIC 299
#define DASH 300
#define COMMA 301
#define QUOTE 302
#define OPAREN 303
#define CPAREN 304
#define INTEGER 305
#define STRING 306
#define MAP_LINE 307
#define MONSTER_NAME 308
#define ITEM_INFO 309
#define IDENTIFIER 310




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 38 "levcomp.ypp"
{
    int i;
    const char *text;
}
/* Line 1529 of yacc.c.  */
#line 164 "levcomp.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

