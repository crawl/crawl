typedef union
{
    int i;
    const char *text;
    raw_range range;
} YYSTYPE;
#define	BRANCHDEF	257
#define	BRANCH	258
#define	DESC	259
#define	DEFAULT	260
#define	DEFAULT_DEPTH	261
#define	SHUFFLE	262
#define	SUBST	263
#define	TAGS	264
#define	KFEAT	265
#define	KITEM	266
#define	KMONS	267
#define	NAME	268
#define	DEPTH	269
#define	ORIENT	270
#define	PLACE	271
#define	CHANCE	272
#define	FLAGS	273
#define	MONS	274
#define	ITEM	275
#define	ROOT_DEPTH	276
#define	ENTRY_MSG	277
#define	EXIT_MSG	278
#define	ROCK_COLOUR	279
#define	FLOOR_COLOUR	280
#define	ENCOMPASS	281
#define	FLOAT	282
#define	NORTH	283
#define	EAST	284
#define	SOUTH	285
#define	WEST	286
#define	NORTHEAST	287
#define	SOUTHEAST	288
#define	SOUTHWEST	289
#define	NORTHWEST	290
#define	LEVEL	291
#define	END	292
#define	PVAULT	293
#define	PMINIVAULT	294
#define	MONSTERS	295
#define	ENDMONSTERS	296
#define	CHARACTER	297
#define	NO_HMIRROR	298
#define	NO_VMIRROR	299
#define	NO_ROTATE	300
#define	PANDEMONIC	301
#define	DASH	302
#define	COMMA	303
#define	QUOTE	304
#define	OPAREN	305
#define	CPAREN	306
#define	COLON	307
#define	STAR	308
#define	NOT	309
#define	INTEGER	310
#define	STRING	311
#define	MAP_LINE	312
#define	MONSTER_NAME	313
#define	ITEM_INFO	314
#define	IDENTIFIER	315


extern YYSTYPE yylval;
