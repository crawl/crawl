typedef union
{
    int i;
    const char *text;
} YYSTYPE;
#define	DEFAULT_DEPTH	257
#define	SYMBOL	258
#define	TAGS	259
#define	NAME	260
#define	DEPTH	261
#define	ORIENT	262
#define	PLACE	263
#define	CHANCE	264
#define	FLAGS	265
#define	MONS	266
#define	ENCOMPASS	267
#define	NORTH	268
#define	EAST	269
#define	SOUTH	270
#define	WEST	271
#define	NORTH_DIS	272
#define	NORTHEAST	273
#define	SOUTHEAST	274
#define	SOUTHWEST	275
#define	NORTHWEST	276
#define	BAD_CHARACTER	277
#define	NO_HMIRROR	278
#define	NO_VMIRROR	279
#define	PANDEMONIC	280
#define	DASH	281
#define	COMMA	282
#define	INTEGER	283
#define	STRING	284
#define	MAP_LINE	285
#define	MONSTER_NAME	286


extern YYSTYPE yylval;
