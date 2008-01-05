/*
**  bmp2png --- conversion from (Windows or OS/2 style) BMP to PNG
**
**  Copyright (C) 1999-2005 MIYASAKA Masaru
**
**  For conditions of distribution and use,
**  see copyright notice in common.h.
*/

#include "common.h"
#include "bmphed.h"

#define BMP2PNG_VERSION		"1.62 (Sep 4, 2005)"
#define BMP2PNG_COPYRIGHT	"Copyright (C) 1999-2005 MIYASAKA Masaru"

char outnam[FILENAME_MAX];
char outdir[FILENAME_MAX];
int  deletesrc = 0;
int  copytime  = 0;
int  complevel = 6;
int  interlace = 0;
int  filters   = 0;
int  alpha_bmp = 0;

#define B2P_TRANSPARENT_NONE	0
#define B2P_TRANSPARENT_RGB		1
#define B2P_TRANSPARENT_PALETTE	2

int trans_type = B2P_TRANSPARENT_NONE;
png_color_16 trans_values;

#if defined(WIN32) || defined(MSDOS)
const char errlogfile[] = ".\\B2PERROR.LOG";
#else
const char errlogfile[] = "./b2perror.log";
#endif

	/* error messages */
#ifdef JAPANESE /* ---------- */
const char wrn_invalidtrans[]   =
        "WARNING: 透明色指定の形式が正しくありません(無視されます) - '%s'\n";
const char wrn_notranscolor[]   =
        "WARNING: 指定された透明色に一致する色がありません - %s\n"
        "WARNING:   -> -P オプション での透明色指定は無視されます\n";
const char wrn_transtruecolor[] =
        "WARNING: 画像はフルカラー形式です - %s\n"
        "WARNING:   -> -P オプション での透明色指定は無視されます\n";
const char wrn_imagehasalpha[] =
        "WARNING: アルファチャネル付きの画像です - %s\n"
        "WARNING:   -> -P オプション での透明色指定は無視されます\n";
const char wrn_alphaallzero[] =
        "WARNING: ４番目のチャネルはアルファチャネルではないようです(すべて０) - %s\n"
        "WARNING:   -> ４番目のチャネル(アルファチャネル)は破棄されます\n";
const char wrn_mkdirfail[]   =
        "WARNING: 出力先ディレクトリを作れません - %s\n"
        "WARNING:   -> -%c オプション での出力先指定は無視されます\n";
const char err_ropenfail[]   = "SKIPPED: 該当するファイルがありません - %s\n";
const char err_wopenfail[]   = "SKIPPED: 出力ファイルを作成できません - %s\n";
const char err_outofmemory[] = "SKIPPED: 作業用メモリが確保できません - %s\n";
	/* -- */
const char err_readeof[]     = "SKIPPED: ファイルが途中で切れています - %s\n";
const char err_readerr[]     = "SKIPPED: 読み込みエラーが発生しました - %s\n";
const char err_not_a_bmp[]   = "SKIPPED: BMP ファイルではありません - %s\n";
const char err_invalid_hed[] =
        "SKIPPED: BMP ファイルのヘッダサイズが無効です - %s\n";
const char err_width_zero[]  = "SKIPPED: 画像の幅が０(または負)です - %s\n";
const char err_height_zero[] = "SKIPPED: 画像の高さが０(または負)です - %s\n";
const char err_compression[] = "SKIPPED: 不明な圧縮タイプです - %s\n";
const char err_invalid_bpp[] = "SKIPPED: 画像の色数が無効です - %s\n";
const char err_no_palette[]  = "SKIPPED: パレットが欠落しています - %s\n";
#else	/* ------------------- */
const char wrn_invalidtrans[]   =
        "WARNING: Invalid transparent color specifier - '%s'. ignored.\n";
const char wrn_notranscolor[]   =
        "WARNING: Specified transparent color is not present in palette - %s\n"
        "WARNING:   -> Transparent color specified by '-P' will be ignored.\n";
const char wrn_transtruecolor[] =
        "WARNING: Image is truecolor format - %s\n"
        "WARNING:   -> Transparent color specified by '-P' will be ignored.\n";
const char wrn_imagehasalpha[] =
        "WARNING: Image has an alpha channel - %s\n"
        "WARNING:   -> Transparent color specified by '-P' will be ignored.\n";
const char wrn_alphaallzero[] =
        "WARNING: The 4th channel doesn't seem to be an alpha channel (all zero) - %s\n"
        "WARNING:   -> The 4th channel (alpha channel) will be discarded.\n";
const char wrn_mkdirfail[]   =
        "WARNING: Cannot create a directory - %s\n"
        "WARNING:   -> Output directory specified by '-%c' will be ignored.\n";
const char err_ropenfail[]   = "SKIPPED: No such file or directory - %s\n";
const char err_wopenfail[]   = "SKIPPED: Cannot create - %s\n";
const char err_outofmemory[] = "SKIPPED: Out of memory - %s\n";
	/* -- */
const char err_readeof[]     = "SKIPPED: Premature end of BMP file - %s\n";
const char err_readerr[]     = "SKIPPED: Read operation failed - %s\n";
const char err_not_a_bmp[]   = "SKIPPED: Not a BMP file - %s\n";
const char err_invalid_hed[] = "SKIPPED: Invalid header size in BMP file - %s\n";
const char err_width_zero[]  = "SKIPPED: Invalid image width - %s\n";
const char err_height_zero[] = "SKIPPED: Invalid image height - %s\n";
const char err_compression[] = "SKIPPED: Unknown compression type - %s\n";
const char err_invalid_bpp[] = "SKIPPED: Invalid bit depth in BMP file - %s\n";
const char err_no_palette[]  = "SKIPPED: Palette is missing - %s\n";
#endif	/* ------------------- */

static int transparent_color(png_color_16p, const char *);
static int png_filters(const char *);
static BOOL read_bmp(char *, IMAGE *);
static BOOL is_4th_alpha(IMAGE *);
static const char *read_rgb_bits(IMAGE *, FILE *);
static const char *read_bitfield_bits(IMAGE *, FILE *, DWORD *, UINT);
static const char *decompress_rle_bits(IMAGE *, FILE *);
static unsigned long mgetdwl(void *);
static unsigned int mgetwl(void *);
static BOOL write_png(char *, IMAGE *);
static void usage_exit(char *, int);



/*
**		メイン
*/
int main(int argc, char *argv[])
{
	char outf[FILENAME_MAX];
	IMAGE image;
	int opt;
	char *arg;
	char *p, c;
	int r_stdin, w_stdout;
	int failure = 0, success = 0;

#ifdef __LCC__		/* lcc-win32 */
	char **envp;
	void _GetMainArgs(int *, char ***, char ***, int);
	_GetMainArgs(&argc, &argv, &envp, 1);
#endif
#ifdef __EMX__
	_wildcard(&argc, &argv);
#endif
	envargv(&argc, &argv, "B2P");

	r_stdin  = !isatty(fileno(stdin));
	w_stdout = !isatty(fileno(stdout));

	while (parsearg(&opt, &arg, argc, argv, "DdOoFfPp")) {
		if (isdigit(opt)) {		/* Zlib Compression Level (0-9) */
			complevel = opt - '0';
			continue;
		}
		switch (toupper(opt)) {
		case 'I':  interlace ^= 1;  break;
		case 'E':  deletesrc ^= 1;  break;
		case 'T':  copytime  ^= 1;  break;
		case 'Q':  quietmode ^= 1;  break;
		case 'L':  errorlog  ^= 1;  break;

		case 'X':
			r_stdin  = 0;
			w_stdout = 0;
			break;

		case 'A':
			alpha_bmp ^= 1;
			break;

		case 'B':
			alpha_bmp ^= 1;
			break;

		case 'R':
			/* '-R' option of png2bmp (ignored on bmp2png) */
			break;

		case 'F':				/* filter types to be used in libpng */
			filters = png_filters(arg);
			break;

		case 'P':				/* transparent color */
			trans_type = transparent_color(&trans_values, arg);
			break;

		case 'D':				/* output directory */
			if (*arg == '-') arg = NULL;
			if (arg == NULL) {
				outdir[0] = '\0';
			} else {
				strcpy(outdir, arg);
				addslash(outdir);
				if (makedir(outdir) != 0) {
					xxprintf(wrn_mkdirfail, outdir, 'D');
					outdir[0] = '\0';
				}
			}
			break;

		case 'O':				/* output filename */
			if (arg == NULL) {
				outnam[0] = '\0';
			} else {
				strcpy(outnam, arg);
				p = basname(outnam);
				c = *p;  *p = '\0';
				if (makedir(outnam) != 0) {
					xxprintf(wrn_mkdirfail, outnam, 'O');
					outnam[0] = '\0';
				} else {
					*p = c;
				}
			}
			break;

		case 0x00:				/* input file spec */
			if (outnam[0] != '\0') {
				strcpy(outf, outnam);
				outnam[0] = '\0';
			} else if (w_stdout) {
				if (!read_bmp(arg, &image)) return 1;
				if (!write_png(NULL, &image)) return 1;
				if (deletesrc) remove(arg);
				return 0;
			} else {
				if (outdir[0] != '\0') {
					strcat(strcpy(outf, outdir), basname(arg));
				} else {
					strcpy(outf, arg);
				}
#ifdef WIN32_LFN
				strcpy(suffix(outf), is_dos_filename(outf) ? ".PNG" : ".png");
#else
				strcpy(suffix(outf), ".png");
#endif
			}
			/* ---------------------- */
			if (!read_bmp(arg, &image)) {
				failure++;
				break;
			}
			renbak(outf);
			if (!write_png(outf, &image)) {
				failure++;
				break;
			}
			/* ---------------------- */
			if (copytime) cpyftime(arg, outf);
			if (deletesrc) remove(arg);
			/* ---------------------- */
			success++;
			break;

		default:
			;		/* Ignore unknown option */
		}
	}
	if (failure == 0 && success == 0) {
		if (!r_stdin) usage_exit(argv[0], 255);
		if (!read_bmp(NULL, &image)) return 1;
		if (outnam[0] != '\0') {
			renbak(outnam);
			return !write_png(outnam, &image);
		} else if (w_stdout) {
			return !write_png(NULL, &image);
		} else {
			strcat(strcpy(outf, outdir), "___stdin.png");
			renbak(outf);
			return !write_png(outf, &image);
		}
	}

	return (failure > 255) ? 255 : failure;
}


#define elemsof(a)	(sizeof(a) / sizeof((a)[0]))

/*
**		PNG のフィルタ種別指定を読む
*/
static int png_filters(const char *arg)
{
	static const struct { char name[8]; int flag; } filter[] = {
		{ "NONE", PNG_FILTER_NONE  },	{ "SUB"    , PNG_FILTER_SUB   },
		{ "UP"  , PNG_FILTER_UP    },	{ "AVERAGE", PNG_FILTER_AVG   },
		{ "AVG" , PNG_FILTER_AVG   },	{ "PAETH"  , PNG_FILTER_PAETH },
		{ "ALL" , PNG_ALL_FILTERS  },
		{ "AUTO", 0                },	{ "DEFAULT", 0                }
	};
	char c, buf[64];
	int i, flags = 0;

	if (arg == NULL) return 0;				/* auto/default */

	do {
		i = 0;
		while (c = *(arg++), c != ',' && c != '\0')
			if (i < sizeof(buf) - 1) buf[i++] = toupper(c);
		buf[i] = '\0';

		for (i = 0; i < elemsof(filter); i++) {
			if (strcmp(buf, filter[i].name) == 0) {
				if (filter[i].flag == 0) flags  = 0;	/* auto/default */
				else                     flags |= filter[i].flag;
			}
		}
	} while (c != '\0');

	return flags;
}


/*
**		透明色指定を読む
*/
static int transparent_color(png_color_16p trans_values, const char *arg)
{
	char c, buf[32];
	int i, n;

	if (arg == NULL) return B2P_TRANSPARENT_NONE;

	for (i = 0; (c = arg[i]) != '\0' && i < sizeof(buf)-1; i++)
		buf[i] = toupper(c);
	buf[i] = '\0';

	if (strcmp(buf, "NONE") == 0) {
		return B2P_TRANSPARENT_NONE;
	}
	if (buf[0] == '#') {
		n = sscanf(buf, "#%2hx%2hx%2hx", &trans_values->red,
		           &trans_values->green, &trans_values->blue);
		if (n == 3 && i >= 7) {
			return B2P_TRANSPARENT_RGB;
		}
	} else {
		n = sscanf(buf, "%hu,%hu,%hu",   &trans_values->red,
		           &trans_values->green, &trans_values->blue);
		if (n == 3 && trans_values->red <= 255 &&
		    trans_values->green <= 255 && trans_values->blue <= 255) {
			return B2P_TRANSPARENT_RGB;
		}
		if (n == 1 && trans_values->red <= 255) {
			trans_values->index = (png_byte)trans_values->red;
			return B2P_TRANSPARENT_PALETTE;
		}
	}

	xxprintf(wrn_invalidtrans, arg);

	return B2P_TRANSPARENT_NONE;
}


/* -----------------------------------------------------------------------
**		BMP ファイルの読み込み
*/

#define ERROR_ABORT(s) do { errmsg = (s); goto error_abort; } while (0)

/*
**		.bmp ファイルの読み込み
*/
static BOOL read_bmp(char *fn, IMAGE *img)
{
	BYTE bfh[FILEHED_SIZE + BMPV5HED_SIZE];
	BYTE *const bih = bfh + FILEHED_SIZE;
	BYTE rgbq[RGBQUAD_SIZE];
	DWORD offbits, bihsize, skip;
	DWORD compression, color_mask[4];
	UINT palette_size, true_pixdepth;
	BOOL alpha_check;
	PALETTE *pal;
	const char *errmsg;
	FILE *fp;
	int i;

	imgbuf_init(img);

	if (fn == NULL) {		/* read from stdin */
		fn = " (stdin)";
		fp = binary_stdio(fileno(stdin));
	} else {
		fp = fopen(fn, "rb");
	}
	if (fp == NULL) ERROR_ABORT(err_ropenfail);

	set_status("Reading %.80s", basname(fn));

	/* ------------------------------------------------------ */

	for (i = 0; ; i++) {		/* skip macbinary header */
		if (fread(bfh, (FILEHED_SIZE + BIHSIZE_SIZE), 1, fp) != 1)
			ERROR_ABORT(ferror(fp) ? err_readerr : err_readeof);
		if (mgetwl(bfh + BFH_WTYPE) == BMP_SIGNATURE) break;
		if (i != 0) ERROR_ABORT(err_not_a_bmp);
		if (fread(bfh, (128 - FILEHED_SIZE - BIHSIZE_SIZE), 1, fp) != 1)
			ERROR_ABORT(ferror(fp) ? err_readerr : err_readeof);
	}
	offbits = mgetdwl(bfh + BFH_DOFFBITS);
	bihsize = mgetdwl(bfh + BFH_DBIHSIZE);
	skip    = offbits - bihsize - FILEHED_SIZE;
	if (bihsize < COREHED_SIZE || bihsize > BMPV5HED_SIZE ||
	    offbits < (bihsize + FILEHED_SIZE)) ERROR_ABORT(err_invalid_hed);

	if (fread((bih + BIHSIZE_SIZE), (bihsize - BIHSIZE_SIZE), 1, fp) != 1)
		ERROR_ABORT(ferror(fp) ? err_readerr : err_readeof);

	if (bihsize >= INFOHED_SIZE) {		/* Windows-style BMP */
		img->width    = mgetdwl(bih + BIH_LWIDTH);
		img->height   = mgetdwl(bih + BIH_LHEIGHT);
		img->pixdepth = mgetwl(bih + BIH_WBITCOUNT);
		img->topdown  = FALSE;
		compression   = mgetdwl(bih + BIH_DCOMPRESSION);
		palette_size  = RGBQUAD_SIZE;
		if (img->height < 0) {
			img->height  = -img->height;
			img->topdown = TRUE;		/* top-down BMP */
		}
	} else {							/* OS/2-style BMP */
		img->width    = mgetwl(bih + BCH_WWIDTH);
		img->height   = mgetwl(bih + BCH_WHEIGHT);
		img->pixdepth = mgetwl(bih + BCH_WBITCOUNT);
		img->topdown  = FALSE;
		compression   = BI_RGB;
		palette_size  = RGBTRIPLE_SIZE;
	}
	img->alpha    = FALSE;
	alpha_check   = FALSE;
	true_pixdepth = img->pixdepth;

	if (img->width  <= 0) ERROR_ABORT(err_width_zero);
	if (img->height <= 0) ERROR_ABORT(err_height_zero);

	switch (compression) {
	case BI_RGB:
		if (img->pixdepth !=  1 && img->pixdepth !=  4 &&
		    img->pixdepth !=  8 && img->pixdepth != 16 &&
		    img->pixdepth != 24 && img->pixdepth != 32)
			ERROR_ABORT(err_invalid_bpp);

		if (img->pixdepth == 32 && alpha_bmp)
			alpha_check = TRUE;

		if (img->pixdepth == 16) {
			color_mask[3] = 0x0000;			/* alpha */
			color_mask[2] = 0x7C00;			/* red */
			color_mask[1] = 0x03E0;			/* green */
			color_mask[0] = 0x001F;			/* blue */
			compression   = BI_BITFIELDS;
		}
		break;

	case BI_BITFIELDS:
		if (img->pixdepth != 16 && img->pixdepth != 32)
			ERROR_ABORT(err_invalid_bpp);

		if (bihsize < INFOHED_SIZE + 12) {
			if (skip < (INFOHED_SIZE + 12 - bihsize))
				ERROR_ABORT(err_invalid_hed);
			if (fread((bih + bihsize), (INFOHED_SIZE + 12 - bihsize), 1, fp)
			    != 1) ERROR_ABORT(ferror(fp) ? err_readerr : err_readeof);
			skip -= (INFOHED_SIZE + 12 - bihsize);
		}
		color_mask[3] = 0x00000000;							/* alpha */
		color_mask[2] = mgetdwl(bih + B4H_DREDMASK);		/* red */
		color_mask[1] = mgetdwl(bih + B4H_DGREENMASK);		/* green */
		color_mask[0] = mgetdwl(bih + B4H_DBLUEMASK);		/* blue */

		if (img->pixdepth == 32 && alpha_bmp &&
		    bihsize >= INFOHED_SIZE + 16) {
			color_mask[3] = mgetdwl(bih + B4H_DALPHAMASK);	/* alpha */
			if (color_mask[3] != 0x00000000)
				img->alpha = TRUE;
		}

		if (img->pixdepth == 32 && color_mask[0] == 0x000000FF &&
		    color_mask[1] == 0x0000FF00 && color_mask[2] == 0x00FF0000 &&
		    (color_mask[3] == 0xFF000000 || color_mask[3] == 0x00000000)) {
			compression = BI_RGB;
		}
		break;

	case BI_RLE8:
		if (img->pixdepth != 8)
			ERROR_ABORT(err_invalid_bpp);
		break;

	case BI_RLE4:
		if (img->pixdepth != 4)
			ERROR_ABORT(err_invalid_bpp);
		break;

	default:
		ERROR_ABORT(err_compression);
	}

	if (img->pixdepth == 16) img->pixdepth = 24;

	if (img->pixdepth <= 8) {
		if (skip >= palette_size << img->pixdepth) {
			img->palnum =            1 << img->pixdepth;
			skip       -= palette_size << img->pixdepth;
		} else {
			img->palnum = skip / palette_size;
			skip        = skip % palette_size;
		}
		if (img->palnum == 0)
			ERROR_ABORT(err_no_palette);
	} else {
		img->palnum = 0;
	}
	if (!imgbuf_alloc(img)) ERROR_ABORT(err_outofmemory);

	/* ------------------------------------------------------ */

	for (pal = img->palette, i = img->palnum; i > 0; pal++, i--) {
		if (fread(rgbq, palette_size, 1, fp) != 1)
			ERROR_ABORT(ferror(fp) ? err_readerr : err_readeof);
		pal->red   = rgbq[RGBQ_RED];
		pal->green = rgbq[RGBQ_GREEN];
		pal->blue  = rgbq[RGBQ_BLUE];
	}
	for ( ; skip > 0; skip--) {
		if (fgetc(fp) == EOF)
			ERROR_ABORT(ferror(fp) ? err_readerr : err_readeof);
	}

	/* ------------------------------------------------------ */

	img->sigbit.red  = img->sigbit.green = img->sigbit.blue = 8;
	img->sigbit.gray = img->sigbit.alpha = 8;

	switch (compression) {
	case BI_RGB:
		errmsg = read_rgb_bits(img, fp);
		break;
	case BI_BITFIELDS:
		errmsg = read_bitfield_bits(img, fp, color_mask, true_pixdepth);
		break;
	case BI_RLE8:
	case BI_RLE4:
		errmsg = decompress_rle_bits(img, fp);
		break;
	default:
		errmsg = err_compression;
	}
	if (errmsg != NULL) ERROR_ABORT(errmsg);

	if (alpha_check) {
		img->alpha = is_4th_alpha(img);
		if (!img->alpha)
			xxprintf(wrn_alphaallzero, fn);
	}

	/* ------------------------------------------------------ */

	set_status("Read OK %.80s", basname(fn));

	if (fp != stdin) fclose(fp);

	return TRUE;

error_abort:				/* error */
	xxprintf(errmsg, fn);
	if (fp != stdin && fp != NULL) fclose(fp);
	imgbuf_free(img);

	return FALSE;
}


/*
**		第４のチャネルがアルファチャネルかどうか調べる
*/
static BOOL is_4th_alpha(IMAGE *img)
{
	LONG w, h;
	BYTE *p;

	if (img->pixdepth == 32) {		/* failsafe */
		for (h = img->height, p = img->bmpbits + 3; --h >= 0; )
			for (w = img->width; --w >= 0; p += 4)
				if (*p != 0) return TRUE;
	}

	return FALSE;
}


/*
**		BI_RGB (無圧縮) 形式の画像データを読む
*/
static const char *read_rgb_bits(IMAGE *img, FILE *fp)
{
#if 1
	DWORD rd  = 16*1024*1024;
	DWORD num = img->imgbytes;
	BYTE *ptr = img->bmpbits;

	while (num > 0) {
		if (rd > num) rd = num;

		if (fread(ptr, rd, 1, fp) != 1)
			return ferror(fp) ? err_readerr : err_readeof;

		ptr += rd; num -= rd;
	}
#else
	if (fread(img->bmpbits, img->imgbytes, 1, fp) != 1)
		return ferror(fp) ? err_readerr : err_readeof;
#endif
	return NULL;
}


/*
**		BI_BITFIELDS 形式の画像データを読む
*/
static const char *read_bitfield_bits(IMAGE *img, FILE *fp, DWORD *color_mask,
                                      UINT true_pixdepth)
{
	int  color_shift[4];
	int  color_sigbits[4];
	BYTE color_tbl[4][1<<7];
	DWORD true_rowbytes;
	BYTE *row, *p, *q;
	LONG w, h;
	DWORD v, u;
	int i, j, k;

	for (i = 0; i < 4; i++) {
		v = color_mask[i];
		if (v == 0) {
			color_shift[i]   = 0;
			color_sigbits[i] = 8;
		} else {
			for (j = 0; (v & 1) == 0; v >>= 1, j++) ;
			for (k = 0;     (v) != 0; v >>= 1, k++) ;
			color_shift[i]   = j;
			color_sigbits[i] = k;
		}
		if (color_sigbits[i] <= 7) {
			k = (1 << color_sigbits[i]) - 1;
			for (j = 0; j <= k; j++)
				color_tbl[i][j] = (0xFF * j + k/2) / k;
		}
	}

	if (color_sigbits[3] < 8) img->sigbit.alpha = color_sigbits[3];
	if (color_sigbits[2] < 8) img->sigbit.red   = color_sigbits[2];
	if (color_sigbits[1] < 8) img->sigbit.green = color_sigbits[1];
	if (color_sigbits[0] < 8) img->sigbit.blue  = color_sigbits[0];

	true_rowbytes = ((DWORD)img->width * (true_pixdepth/8) + 3) & (~3UL);

	for (h = img->height, row = img->bmpbits; --h >= 0;
	     row += img->rowbytes) {
		if (fread(row, true_rowbytes, 1, fp) != 1)
			return ferror(fp) ? err_readerr : err_readeof;

		switch (true_pixdepth) {
		case 16:
			for (w = img->width, p = row + (w-1)*2, q = row + (w-1)*3;
			     --w >= 0; p -= 2, q -= 3) {
				v = ((UINT)p[0]) + ((UINT)p[1] << 8);
				for (i = 0; i < 3; i++) {
					u = (v & color_mask[i]) >> color_shift[i];
					if (color_sigbits[i] <= 7)
						u = color_tbl[i][u];
					else if (color_sigbits[i] >= 9)
						u >>= (color_sigbits[i] - 8);
					q[i] = (BYTE) u;
				}
			}
			break;

		case 32:
			for (w = img->width, p = row; --w >= 0; p += 4) {
				v = ((DWORD)p[0]      ) + ((DWORD)p[1] <<  8) +
				    ((DWORD)p[2] << 16) + ((DWORD)p[3] << 24);
				for (i = 0; i < 4; i++) {
					u = (v & color_mask[i]) >> color_shift[i];
					if (color_sigbits[i] <= 7)
						u = color_tbl[i][u];
					else if (color_sigbits[i] >= 9)
						u >>= (color_sigbits[i] - 8);
					p[i] = (BYTE) u;
				}
			}
			break;
		}
	}

	return NULL;
}


/*
**		BI_RLE8/BI_RLE4 形式の画像データを読む
*/
static const char *decompress_rle_bits(IMAGE *img, FILE *fp)
{
	BYTE buf[1024];		/* 258 or above */
	BYTE *bfptr = buf;
	UINT  bfcnt = 0;
	UINT  rd, reclen;
	BYTE *row = img->bmpbits;
	LONG x = 0, y = 0;
	BYTE *p, c;
	int n;

	memset(img->bmpbits, 0, img->imgbytes);

	for (;;) {
		while (bfcnt < (reclen = 2) ||
		       (bfptr[0] == 0 &&
		        ((bfptr[1] == 2 && bfcnt < (reclen += 2)) ||
		         (bfptr[1] >= 3 &&
		          bfcnt < (reclen += (bfptr[1] * img->pixdepth + 15) / 16 * 2)
		         )))) {
			if (bfptr != buf && bfcnt != 0) memmove(buf, bfptr, bfcnt);
			if ((rd = fread(buf+bfcnt, 1, sizeof(buf)-bfcnt, fp)) == 0) {
				if (x >= img->width) { /*x = 0;*/ y += 1; }
				if (y >= img->height) return NULL;	/* missing EoB marker */
				else return ferror(fp) ? err_readerr : err_readeof;
			}
			bfptr  = buf;
			bfcnt += rd;
		}
		if (y >= img->height) {
			/* We simply discard the remaining records */
			if (bfptr[0] == 0 && bfptr[1] == 1) break;	/* EoB marker */
			bfptr += reclen;
			bfcnt -= reclen;
			continue;
		}
		if (bfptr[0] != 0) {				/* Encoded-mode record */
			n = bfptr[0];  c = bfptr[1];
			switch (img->pixdepth) {
			case 8:						/* BI_RLE8 */
				while (n > 0 && x < img->width) {
					row[x] = c;
					n--; x++;
				}
				break;
			case 4:						/* BI_RLE4 */
				if (x % 2 != 0 && x < img->width) {
					c = (c >> 4) | (c << 4);
					row[x/2] = (row[x/2] & 0xF0) | (c & 0x0F);
					n--; x++;
				}
				while (n > 0 && x < img->width) {
					row[x/2] = c;
					n-=2; x+=2;
				}
				if (n < 0) x--;
				break;
			}
		} else if (bfptr[1] >= 3) {			/* Absolute-mode record */
			n = bfptr[1];  p = bfptr + 2;
			switch (img->pixdepth) {
			case 8:						/* BI_RLE8 */
				while (n > 0 && x < img->width) {
					row[x] = *p;
					n--; x++; p++;
				}
				break;
			case 4:						/* BI_RLE4 */
				if (x % 2 != 0) {
					if (x < img->width) {
						row[x/2] = (row[x/2] & 0xF0) | (*p >> 4);
						n--; x++;
					}
					while (n > 0 && x < img->width) {
						row[x/2] = (p[0] << 4) | (p[1] >> 4);
						n-=2; x+=2; p++;
					}
					if (n < 0) x--;
				} else {
					while (n > 0 && x < img->width) {
						row[x/2] = *p;
						n-=2; x+=2; p++;
					}
					if (n < 0) x--;
				}
				break;
			}
		} else if (bfptr[1] == 2) {			/* Delta record */
			x += bfptr[2]; y += bfptr[3];
			row += bfptr[3] * img->rowbytes;
		} else if (bfptr[1] == 0) {			/* End of line marker */
			x = 0; y += 1;
			row += img->rowbytes;
		} else /*if (bfptr[1] == 1)*/ {		/* End of bitmap marker */
			break;
		}
		bfptr += reclen;
		bfcnt -= reclen;
	}

	return NULL;
}


/*
**	メモリから little-endien 形式 4バイト無符号整数を読む
*/
static unsigned long mgetdwl(void *ptr)
{
	unsigned char *p = ptr;

	return ((unsigned long)p[0]      ) + ((unsigned long)p[1] <<  8) +
	       ((unsigned long)p[2] << 16) + ((unsigned long)p[3] << 24);
}


/*
**	メモリから little-endien 形式 2バイト無符号整数を読む
*/
static unsigned int mgetwl(void *ptr)
{
	unsigned char *p = ptr;

	return ((unsigned int)p[0]) + ((unsigned int)p[1] << 8);
}


/* -----------------------------------------------------------------------
**		PNG ファイルの書き込み
*/

/*
**		.png ファイルの書き込み
*/
static BOOL write_png(char *fn, IMAGE *img)
{
	png_structp png_ptr;
	png_infop info_ptr;
	int bit_depth;
	int color_type;
	int interlace_type;
	png_byte trans[256];
	unsigned i;
	const char *errmsg;
	FILE *fp;

	if (fn == NULL) {
		fn = " (stdout)";
		fp = binary_stdio(fileno(stdout));
	} else {
		fp = fopen(fn, "wb");
	}
	if (fp == NULL) ERROR_ABORT(err_wopenfail);

	set_status("Writing %.80s", basname(fn));

	/* ------------------------------------------------------ */

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, fn,
	                                    png_my_error, png_my_warning);
	if (png_ptr == NULL) {
		ERROR_ABORT(err_outofmemory);
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, NULL);
		ERROR_ABORT(err_outofmemory);
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		/* If we get here, we had a problem reading the file */
		png_destroy_write_struct(&png_ptr, &info_ptr);
		ERROR_ABORT(NULL);
	}
	png_init_io(png_ptr, fp);
	png_set_compression_level(png_ptr, complevel);
	if (filters != 0)
		png_set_filter(png_ptr, PNG_FILTER_TYPE_BASE, filters);

	/* ------------------------------------------------------ */

	if (img->pixdepth == 24 || img->pixdepth == 32) {
		bit_depth  = 8;
		color_type = (img->pixdepth == 32 && img->alpha) ?
		                PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB;
		png_set_compression_mem_level(png_ptr, MAX_MEM_LEVEL);
	} else {
		bit_depth  = img->pixdepth;
		color_type = PNG_COLOR_TYPE_PALETTE;
		png_set_PLTE(png_ptr, info_ptr, img->palette, img->palnum);
	}
	interlace_type = (interlace) ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE;

	png_set_IHDR(png_ptr, info_ptr, img->width, img->height, bit_depth,
	             color_type, interlace_type, PNG_COMPRESSION_TYPE_DEFAULT,
	             PNG_FILTER_TYPE_DEFAULT);

	if (img->sigbit.red != 8 || img->sigbit.green != 8 || img->sigbit.blue != 8
	    || (color_type == PNG_COLOR_TYPE_RGB_ALPHA && img->sigbit.alpha != 8))
		png_set_sBIT(png_ptr, info_ptr, &img->sigbit);

	switch (trans_type) {
	case B2P_TRANSPARENT_RGB:
		switch (color_type) {
		case PNG_COLOR_TYPE_PALETTE:
			for (i = 0; i < img->palnum; i++) {
				if (img->palette[i].red   == trans_values.red   &&
				    img->palette[i].green == trans_values.green &&
				    img->palette[i].blue  == trans_values.blue) {
					trans[i++] = 0x00;
					break;
				}
				trans[i] = 0xFF;
			}
			if (trans[i-1] == 0x00) {
				png_set_tRNS(png_ptr, info_ptr, trans, i, NULL);
			} else {
				xxprintf(wrn_notranscolor, fn);
			}
			break;
		case PNG_COLOR_TYPE_RGB:
			png_set_tRNS(png_ptr, info_ptr, NULL, 0, &trans_values);
			break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			xxprintf(wrn_imagehasalpha, fn);
			break;
		}
		break;
	case B2P_TRANSPARENT_PALETTE:
		switch (color_type) {
		case PNG_COLOR_TYPE_PALETTE:
			if (trans_values.index < img->palnum) {
				for (i = 0; i < trans_values.index; i++) trans[i] = 0xFF;
				trans[i++] = 0x00;
				png_set_tRNS(png_ptr, info_ptr, trans, i, NULL);
			} else {
				xxprintf(wrn_notranscolor, fn);
			}
			break;
		case PNG_COLOR_TYPE_RGB:
			xxprintf(wrn_transtruecolor, fn);
			break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			xxprintf(wrn_imagehasalpha, fn);
			break;
		}
		break;
	}

	png_write_info(png_ptr, info_ptr);

	/* ------------------------------------------------------ */

	if (img->pixdepth == 32 && !img->alpha)
		png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);

	if (img->pixdepth == 24 || img->pixdepth == 32)
		png_set_bgr(png_ptr);

	/* ------------------------------------------------------ */

	png_set_write_status_fn(png_ptr, row_callback);
	init_progress_meter(png_ptr, img->width, img->height);

	png_write_image(png_ptr, img->rowptr);

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	/* ------------------------------------------------------ */

	set_status("OK      %.80s", basname(fn));
	feed_line();

	fflush(fp);
	if (fp != stdout) fclose(fp);
	imgbuf_free(img);

	return TRUE;

error_abort:				/* error */
	if (errmsg != NULL) xxprintf(errmsg, fn);
	if (fp != stdout && fp != NULL) fclose(fp);
	imgbuf_free(img);

	return FALSE;
}


/* -----------------------------------------------------------------------
**		ヘルプスクリーンの表示
*/

/*
**		使用法表示
*/
static void usage_exit(char *argv0, int status)
{
	static const char str_usage[] =
#ifdef JAPANESE /* -------------------------- */
#ifdef SJIS_ESCAPE
#define SJ_ESC(esc,raw)	esc
#else
#define SJ_ESC(esc,raw)	raw
#endif
		"bmp2png, BMP -> PNG コンバータ - version " BMP2PNG_VERSION "\n"
		"   " BMP2PNG_COPYRIGHT "\n"
		"   Compiled with libpng " PNG_LIBPNG_VER_STRING " and zlib " ZLIB_VERSION ".\n"
		"\n"
		"使い方 : %s [-スイッチ] 入力ファイル名 ...\n"
		"       : ... | %s [-スイッチ] | ...\n"
		"\n"
		"入力ファイル名にはワイルドカードが使えます (* と ?)\n"
		"出力ファイル名は入力ファイル名の拡張子を .png に変えた名前になります\n"
		"\n"
		"スイッチオプション (小文字でも可) :\n"
		"   -0..-9   圧縮レベル (デフォルトは -6)\n"
		"   -I       インターレース形式の PNG ファイルを作成する\n"
		"   -P color  指定した色を透明色にする\n"
		"             color: #RRGGBB(html式16進) / RR,GG,BB(10進RGB) / NN(パレット番号)\n"
		"   -F type[,...]  PNG の圧縮に使われるフィルタ・タイプを指定する\n"
		"                  type: none,sub,up,average(avg),paeth,all,auto(default)\n"
		"   -A, -B   アルファチャネルを保存する\n"
		"   -O name  出力ファイル名を指定する\n"
		"   -D dir   ファイルを出力するディレクトリを指定する\n"
		"   -E       変換が成功した場合には入力ファイルを削除する\n"
		"   -T       入力ファイルのタイムスタンプを出力ファイルに設定する\n"
		"   -Q       処理中, 一切の" SJ_ESC("表\示","表示") "をしない\n"
		"   -L       処理中のエラーをログファイル(%s)に記録する\n"
		"   -X       標準入力／標準出力を介した変換を無効にする\n";
#else  /* ----------------------------------- */
		"bmp2png, a BMP-to-PNG converter - version " BMP2PNG_VERSION "\n"
		"   " BMP2PNG_COPYRIGHT "\n"
		"   Compiled with libpng " PNG_LIBPNG_VER_STRING " and zlib " ZLIB_VERSION ".\n"
		"\n"
		"Usage: %s [-switches] inputfile(s) ...\n"
		"   or: ... | %s [-switches] | ...\n"
		"\n"
		"List of input files may use wildcards (* and ?)\n"
		"Output filename is same as input filename, but extension .png\n"
		"\n"
		"Switches (case-insensitive) :\n"
		"   -0..-9   Compression level (default: -6)\n"
		"   -I       Create interlaced PNG files\n"
		"   -P color  Mark the specified color as transparent\n"
		"             color: #RRGGBB(html hex) / RR,GG,BB(decimal) / NN(palette index)\n"
		"   -F type[,...]  Specify filter type(s) used to create PNG files\n"
		"                  type: none,sub,up,average(avg),paeth,all,auto(default)\n"
		"   -A, -B   Preserve alpha channel\n"
		"   -O name  Specify name for output file\n"
		"   -D dir   Output files into dir\n"
		"   -E       Delete input files after successful conversion\n"
		"   -T       Set the timestamp of input file on output file\n"
		"   -Q       Quiet mode\n"
		"   -L       Log errors to %s file\n"
		"   -X       Disable conversion through standard input/output\n";
#endif /* ----------------------------------- */
#if defined(WIN32) || defined(MSDOS)
	char exename[FILENAME_MAX];
	char *p;

	argv0 = strcpy(exename, basname(argv0));
	for (p = argv0; *p != '\0'; p++) *p = tolower(*p);
#endif
	xxprintf(str_usage, argv0, argv0, errlogfile);

	exit(status);
}


/* -----------------------------------------------------------------------
*/

#if (PNG_LIBPNG_VER >= 10007)
/*
**	dummy - see png_reset_zstream() in png.c
*/
int inflateReset(z_streamp z)
{
	return Z_OK;
}
#endif
