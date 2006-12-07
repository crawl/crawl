typedef union
{
    int i;
    const char *text;
} YYSTYPE;
#define	BRANCHDEF	257
#define	BRANCH	258
#define	DESC	259
#define	DEFAULT	260
#define	DEFAULT_DEPTH	261
#define	SYMBOL	262
#define	TAGS	263
#define	NAME	264
#define	DEPTH	265
#define	ORIENT	266
#define	PLACE	267
#define	CHANCE	268
#define	FLAGS	269
#define	MONS	270
#define	ROOT_DEPTH	271
#define	ENTRY_MSG	272
#define	EXIT_MSG	273
#define	ROCK_COLOUR	274
#define	FLOOR_COLOUR	275
#define	ENCOMPASS	276
#define	FLOAT	277
#define	NORTH	278
#define	EAST	279
#define	SOUTH	280
#define	WEST	281
#define	NORTHEAST	282
#define	SOUTHEAST	283
#define	SOUTHWEST	284
#define	NORTHWEST	285
#define	LEVEL	286
#define	END	287
#define	PVAULT	288
#define	PMINIVAULT	289
#define	MONSTERS	290
#define	ENDMONSTERS	291
#define	CHARACTER	292
#define	NO_HMIRROR	293
#define	NO_VMIRROR	294
#define	NO_ROTATE	295
#define	PANDEMONIC	296
#define	DASH	297
#define	COMMA	298
#define	QUOTE	299
#define	OPAREN	300
#define	CPAREN	301
#define	INTEGER	302
#define	STRING	303
#define	MAP_LINE	304
#define	MONSTER_NAME	305
#define	IDENTIFIER	306


extern YYSTYPE yylval;
