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
     FLAGS = 274,
     MONS = 275,
     ITEM = 276,
     ROOT_DEPTH = 277,
     ENTRY_MSG = 278,
     EXIT_MSG = 279,
     ROCK_COLOUR = 280,
     FLOOR_COLOUR = 281,
     ENCOMPASS = 282,
     FLOAT = 283,
     NORTH = 284,
     EAST = 285,
     SOUTH = 286,
     WEST = 287,
     NORTHEAST = 288,
     SOUTHEAST = 289,
     SOUTHWEST = 290,
     NORTHWEST = 291,
     LEVEL = 292,
     END = 293,
     PVAULT = 294,
     PMINIVAULT = 295,
     MONSTERS = 296,
     ENDMONSTERS = 297,
     CHARACTER = 298,
     NO_HMIRROR = 299,
     NO_VMIRROR = 300,
     NO_ROTATE = 301,
     PANDEMONIC = 302,
     DASH = 303,
     COMMA = 304,
     QUOTE = 305,
     OPAREN = 306,
     CPAREN = 307,
     INTEGER = 308,
     STRING = 309,
     MAP_LINE = 310,
     MONSTER_NAME = 311,
     ITEM_INFO = 312,
     IDENTIFIER = 313
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
#define FLAGS 274
#define MONS 275
#define ITEM 276
#define ROOT_DEPTH 277
#define ENTRY_MSG 278
#define EXIT_MSG 279
#define ROCK_COLOUR 280
#define FLOOR_COLOUR 281
#define ENCOMPASS 282
#define FLOAT 283
#define NORTH 284
#define EAST 285
#define SOUTH 286
#define WEST 287
#define NORTHEAST 288
#define SOUTHEAST 289
#define SOUTHWEST 290
#define NORTHWEST 291
#define LEVEL 292
#define END 293
#define PVAULT 294
#define PMINIVAULT 295
#define MONSTERS 296
#define ENDMONSTERS 297
#define CHARACTER 298
#define NO_HMIRROR 299
#define NO_VMIRROR 300
#define NO_ROTATE 301
#define PANDEMONIC 302
#define DASH 303
#define COMMA 304
#define QUOTE 305
#define OPAREN 306
#define CPAREN 307
#define INTEGER 308
#define STRING 309
#define MAP_LINE 310
#define MONSTER_NAME 311
#define ITEM_INFO 312
#define IDENTIFIER 313




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 40 "levcomp.ypp"
{
    int i;
    const char *text;
}
/* Line 1529 of yacc.c.  */
#line 170 "levcomp.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

