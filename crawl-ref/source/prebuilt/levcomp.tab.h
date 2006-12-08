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
#define	ITEM	271
#define	ROOT_DEPTH	272
#define	ENTRY_MSG	273
#define	EXIT_MSG	274
#define	ROCK_COLOUR	275
#define	FLOOR_COLOUR	276
#define	ENCOMPASS	277
#define	FLOAT	278
#define	NORTH	279
#define	EAST	280
#define	SOUTH	281
#define	WEST	282
#define	NORTHEAST	283
#define	SOUTHEAST	284
#define	SOUTHWEST	285
#define	NORTHWEST	286
#define	LEVEL	287
#define	END	288
#define	PVAULT	289
#define	PMINIVAULT	290
#define	MONSTERS	291
#define	ENDMONSTERS	292
#define	CHARACTER	293
#define	NO_HMIRROR	294
#define	NO_VMIRROR	295
#define	NO_ROTATE	296
#define	PANDEMONIC	297
#define	DASH	298
#define	COMMA	299
#define	QUOTE	300
#define	OPAREN	301
#define	CPAREN	302
#define	INTEGER	303
#define	STRING	304
#define	MAP_LINE	305
#define	MONSTER_NAME	306
#define	ITEM_INFO	307
#define	IDENTIFIER	308


extern YYSTYPE yylval;
