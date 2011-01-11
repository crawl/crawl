/* A Bison parser, made by GNU Bison 2.4.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
   2009, 2010 Free Software Foundation, Inc.
   
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
     KPROP = 266,
     NAME = 267,
     DEPTH = 268,
     ORIENT = 269,
     PLACE = 270,
     CHANCE = 271,
     WEIGHT = 272,
     MONS = 273,
     ITEM = 274,
     MARKER = 275,
     COLOUR = 276,
     PRELUDE = 277,
     MAIN = 278,
     VALIDATE = 279,
     VETO = 280,
     EPILOGUE = 281,
     NSUBST = 282,
     WELCOME = 283,
     LFLAGS = 284,
     BFLAGS = 285,
     LFLOORCOL = 286,
     LROCKCOL = 287,
     LFLOORTILE = 288,
     LROCKTILE = 289,
     FTILE = 290,
     RTILE = 291,
     TILE = 292,
     SUBVAULT = 293,
     FHEIGHT = 294,
     DESC = 295,
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
#define SUBST 260
#define TAGS 261
#define KFEAT 262
#define KITEM 263
#define KMONS 264
#define KMASK 265
#define KPROP 266
#define NAME 267
#define DEPTH 268
#define ORIENT 269
#define PLACE 270
#define CHANCE 271
#define WEIGHT 272
#define MONS 273
#define ITEM 274
#define MARKER 275
#define COLOUR 276
#define PRELUDE 277
#define MAIN 278
#define VALIDATE 279
#define VETO 280
#define EPILOGUE 281
#define NSUBST 282
#define WELCOME 283
#define LFLAGS 284
#define BFLAGS 285
#define LFLOORCOL 286
#define LROCKCOL 287
#define LFLOORTILE 288
#define LROCKTILE 289
#define FTILE 290
#define RTILE 291
#define TILE 292
#define SUBVAULT 293
#define FHEIGHT 294
#define DESC 295
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




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1685 of yacc.c  */
#line 34 "levcomp.ypp"

    int i;
    double f;
    const char *text;
    map_chance_pair chance;



/* Line 1685 of yacc.c  */
#line 162 "levcomp.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


