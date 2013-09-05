/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
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
     LFLAGS = 285,
     BFLAGS = 286,
     LFLOORCOL = 287,
     LROCKCOL = 288,
     LFLOORTILE = 289,
     LROCKTILE = 290,
     FTILE = 291,
     RTILE = 292,
     TILE = 293,
     SUBVAULT = 294,
     FHEIGHT = 295,
     DESC = 296,
     ORDER = 297,
     COMMA = 298,
     COLON = 299,
     PERC = 300,
     DASH = 301,
     CHARACTER = 302,
     NUMBER = 303,
     STRING = 304,
     MAP_LINE = 305,
     MONSTER_NAME = 306,
     ITEM_INFO = 307,
     LUA_LINE = 308
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
#define LFLAGS 285
#define BFLAGS 286
#define LFLOORCOL 287
#define LROCKCOL 288
#define LFLOORTILE 289
#define LROCKTILE 290
#define FTILE 291
#define RTILE 292
#define TILE 293
#define SUBVAULT 294
#define FHEIGHT 295
#define DESC 296
#define ORDER 297
#define COMMA 298
#define COLON 299
#define PERC 300
#define DASH 301
#define CHARACTER 302
#define NUMBER 303
#define STRING 304
#define MAP_LINE 305
#define MONSTER_NAME 306
#define ITEM_INFO 307
#define LUA_LINE 308




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 34 "levcomp.ypp"

    int i;
    double f;
    const char *text;
    map_chance_pair chance;



/* Line 2068 of yacc.c  */
#line 165 "levcomp.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


