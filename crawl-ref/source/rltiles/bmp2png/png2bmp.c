/*
**  png2bmp --- conversion from PNG to (Windows style) BMP
**
**  Copyright (C) 1999-2005 MIYASAKA Masaru
**
**  For conditions of distribution and use,
**  see copyright notice in common.h.
*/

#include "common.h"
#include "bmphed.h"

#define PNG2BMP_VERSION		"1.62 (Sep 4, 2005)"
#define PNG2BMP_COPYRIGHT	"Copyright (C) 1999-2005 MIYASAKA Masaru"

char outnam[FILENAME_MAX];
char outdir[FILENAME_MAX];
int  deletesrc = 0;
int  copytime  = 0;

#define P2B_ALPHABMP_NONE		0
#define P2B_ALPHABMP_ARGB		1	/* -a option; 32bit ARGB(RGB) BMP */
#define P2B_ALPHABMP_BITFIELD	2	/* -b option; 32bit Bitfield BMP  */

int alpha_format = P2B_ALPHABMP_NONE;
int expand_trans = 0;

#if defined(WIN32) || defined(MSDOS)
const char errlogfile[] = ".\\P2BERROR.LOG";
#else
const char errlogfile[] = "./p2berror.log";
#endif

	/* error messages */
#ifdef JAPANESE /* ---------- */
const char wrn_mkdirfail[]   =
        "WARNING: 出力先ディレクトリを作れません - %s\n"
        "WARNING:   -> -%c オプション での出力先指定は無視されます\n";
const char err_ropenfail[]   = "SKIPPED: 該当するファイルがありません - %s\n";
const char err_wopenfail[]   = "SKIPPED: 出力ファイルを作成できません - %s\n";
const char err_outofmemory[] = "SKIPPED: 作業用メモリが確保できません - %s\n";
	/* -- */
const char err_writeerr[]    = "SKIPPED: 書き込みエラーが発生しました - %s\n";
const char err_not_a_png[]   = "SKIPPED: PNG ファイルではありません - %s\n";
#else  /* ------------------- */
const char wrn_mkdirfail[]   =
        "WARNING: Cannot create a directory - %s\n"
        "WARNING:   -> Output directory specified by '-%c' will be ignored.\n";
const char err_ropenfail[]   = "SKIPPED: No such file or directory - %s\n";
const char err_wopenfail[]   = "SKIPPED: Cannot create - %s\n";
const char err_outofmemory[] = "SKIPPED: Out of memory - %s\n";
	/* -- */
const char err_writeerr[]    = "SKIPPED: Write operation failed - %s\n";
const char err_not_a_png[]   = "SKIPPED: Not a PNG file - %s\n";
#endif /* ------------------- */

static BOOL read_png(char *, IMAGE *);
static int skip_macbinary(png_structp);
static void to4bpp(png_structp, png_row_infop, png_bytep);
static BOOL write_bmp(char *, IMAGE *);
static const char *write_rgb_bits(IMAGE *, FILE *);
static void mputdwl(void *, unsigned long);
static void mputwl(void *, unsigned int);
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

#ifdef __LCC__					/* lcc-win32 */
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
		switch (toupper(opt)) {
		case 'E':  deletesrc ^= 1;  break;
		case 'T':  copytime  ^= 1;  break;
		case 'Q':  quietmode ^= 1;  break;
		case 'L':  errorlog  ^= 1;  break;

		case 'X':
			r_stdin  = 0;
			w_stdout = 0;
			break;

		case 'A':
			alpha_format = (alpha_format == P2B_ALPHABMP_ARGB) ?
			                P2B_ALPHABMP_NONE : P2B_ALPHABMP_ARGB;
			break;

		case 'B':
			alpha_format = (alpha_format == P2B_ALPHABMP_BITFIELD) ?
			                P2B_ALPHABMP_NONE : P2B_ALPHABMP_BITFIELD;
			break;

		case 'R':
			expand_trans ^= 1;
			break;

		case 'F':
			/* '-F' option of bmp2png (ignored on png2bmp) */
			break;

		case 'P':
			/* '-P' option of bmp2png (ignored on png2bmp) */
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
				if (!read_png(arg, &image)) return 1;
				if (!write_bmp(NULL, &image)) return 1;
				if (deletesrc) remove(arg);
				return 0;
			} else {
				if (outdir[0] != '\0') {
					strcat(strcpy(outf, outdir), basname(arg));
				} else {
					strcpy(outf, arg);
				}
#ifdef WIN32_LFN
				strcpy(suffix(outf), is_dos_filename(outf) ? ".BMP" : ".bmp");
#else
				strcpy(suffix(outf), ".bmp");
#endif
			}
			/* ---------------------- */
			if (!read_png(arg, &image)) {
				failure++;
				break;
			}
			renbak(outf);
			if (!write_bmp(outf, &image)) {
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
		if (!read_png(NULL, &image)) return 1;
		if (outnam[0] != '\0') {
			renbak(outnam);
			return !write_bmp(outnam, &image);
		} else if (w_stdout) {
			return !write_bmp(NULL, &image);
		} else {
			strcat(strcpy(outf, outdir), "___stdin.bmp");
			renbak(outf);
			return !write_bmp(outf, &image);
		}
	}

	return (failure > 255) ? 255 : failure;
}


/* -----------------------------------------------------------------------
**		PNG ファイルの読み込み
*/

#define ERROR_ABORT(s) do { errmsg = (s); goto error_abort; } while (0)

/*
**		.pngファイルからの読み込み
*/
static BOOL read_png(char *fn, IMAGE *img)
{
	png_structp png_ptr;
	png_infop info_ptr, end_info;
	png_uint_32 width, height;
	int bit_depth, color_type;
	int xbit_depth, xcolor_type, xchannels;
	const char *errmsg;
	FILE *fp;

	imgbuf_init(img);

	if (fn == NULL) {
		fn = " (stdin)";
		fp = binary_stdio(fileno(stdin));
	} else {
		fp = fopen(fn, "rb");
	}
	if (fp == NULL) ERROR_ABORT(err_ropenfail);

	set_status("Reading %.80s", basname(fn));

	/* ------------------------------------------------------ */

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, fn,
	                                   png_my_error, png_my_warning);
	if (png_ptr == NULL) {
		ERROR_ABORT(err_outofmemory);
	}
	info_ptr = png_create_info_struct(png_ptr);
	end_info = png_create_info_struct(png_ptr);
	if (info_ptr == NULL || end_info == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		ERROR_ABORT(err_outofmemory);
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		/* If we get here, we had a problem writing the file */
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		ERROR_ABORT(NULL);
	}
	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, skip_macbinary(png_ptr));

	/* ------------------------------------------------------ */

	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
	             &color_type, NULL, NULL, NULL);

	/* ------------------------------------------------------ */

	if (color_type & PNG_COLOR_MASK_ALPHA) {
		if (alpha_format == P2B_ALPHABMP_NONE) {
			png_set_strip_alpha(png_ptr);
			color_type &= ~PNG_COLOR_MASK_ALPHA;
		}
	} else if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		if (alpha_format != P2B_ALPHABMP_NONE && expand_trans) {
			png_set_tRNS_to_alpha(png_ptr);
			color_type |=  PNG_COLOR_MASK_ALPHA;
			color_type &= ~PNG_COLOR_MASK_PALETTE;
		}
	}
	if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb(png_ptr);
	}
	if (color_type == PNG_COLOR_TYPE_RGB ||
		color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
		png_set_bgr(png_ptr);
	}
	if (!(color_type & PNG_COLOR_MASK_ALPHA) && bit_depth == 2) {
		png_set_user_transform_info(png_ptr, NULL, 4, 1);
		png_set_read_user_transform_fn(png_ptr, to4bpp);
	}
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	png_read_update_info(png_ptr, info_ptr);

	/* ------------------------------------------------------ */

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &xbit_depth,
	             &xcolor_type, NULL, NULL, NULL);
	xchannels = png_get_channels(png_ptr, info_ptr);

	img->width    = (LONG)width;
	img->height   = (LONG)height;
	img->pixdepth = (UINT)xbit_depth * xchannels;
	img->palnum   = (img->pixdepth <= 8) ? (1 << img->pixdepth) : 0;
	img->topdown  = FALSE;
	img->alpha    = (xcolor_type & PNG_COLOR_MASK_ALPHA) ? TRUE : FALSE;

	if (!imgbuf_alloc(img)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		ERROR_ABORT(err_outofmemory);
	}

	if (img->palnum > 0) {
		if (xcolor_type == PNG_COLOR_TYPE_PALETTE) {
			png_colorp palette;
			int num_palette;
			png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
			if (num_palette > (int)img->palnum) num_palette = img->palnum;
			memset(img->palette,       0, img->palnum * sizeof(png_color));
			memcpy(img->palette, palette, num_palette * sizeof(png_color));
		} else {
			int depth = (bit_depth == 16) ? 8 : bit_depth;
			memset(img->palette, 0, img->palnum * sizeof(png_color));
			png_build_grayscale_palette(depth, img->palette);
		}
	}

	/* ------------------------------------------------------ */

	png_set_read_status_fn(png_ptr, row_callback);
	init_progress_meter(png_ptr, img->width, img->height);

	png_read_image(png_ptr, img->rowptr);

	png_read_end(png_ptr, end_info);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	/* ------------------------------------------------------ */

	set_status("Read OK %.80s", basname(fn));

	if (fp != stdin) fclose(fp);

	return TRUE;

error_abort:				/* error */
	if (errmsg != NULL) xxprintf(errmsg, fn);
	if (fp != stdin && fp != NULL) fclose(fp);
	imgbuf_free(img);

	return FALSE;
}


/*
**		ファイル先頭にあるマックバイナリを読み跳ばす
*/
static int skip_macbinary(png_structp png_ptr)
{
#if 0
	void png_read_data(png_structp, png_bytep, png_size_t);
	enum { PNG_BYTES_TO_CHECK = 8, MACBIN_SIZE = 128 };	/* ^ in pngrio.c */
	png_byte buf[MACBIN_SIZE];
	png_bytep sig;

	png_read_data(png_ptr, buf, PNG_BYTES_TO_CHECK);
	if (png_sig_cmp(buf, 0, PNG_BYTES_TO_CHECK) == 0)
								return PNG_BYTES_TO_CHECK;

	png_read_data(png_ptr, buf, MACBIN_SIZE);
	sig = buf + MACBIN_SIZE - PNG_BYTES_TO_CHECK;
	if (png_sig_cmp(sig, 0, PNG_BYTES_TO_CHECK) == 0)
								return PNG_BYTES_TO_CHECK;

	xxprintf(err_not_a_png, (char *)png_get_error_ptr(png_ptr));
	longjmp(png_jmpbuf(png_ptr), 1);

#endif
	return 0;	/* to quiet compiler warnings */
}


/*
**		4色形式 -> 16色形式への変換
*/
static void to4bpp(png_structp png_ptr, png_row_infop row_info, png_bytep data)
{
	static const png_byte pix[] = {
		0x00, 0x01, 0x02, 0x03,  0x10, 0x11, 0x12, 0x13,
		0x20, 0x21, 0x22, 0x23,  0x30, 0x31, 0x32, 0x33,
	};
	png_uint_32 rowb;
	png_bytep p, q;
	png_byte c;

	rowb = (row_info->width + 1) / 2;
	q = data + rowb;
	p = data + rowb / 2;

	if (rowb % 2 == 1) {
		c = *p;
		*(--q) = pix[c >> 4];
	}
	while (p > data) {
		c = *(--p);
		*(--q) = pix[c & 0x0F];
		*(--q) = pix[c >> 4];
	}
	row_info->bit_depth   = 4;
	row_info->pixel_depth = 4;
	row_info->rowbytes    = rowb;
}


/* -----------------------------------------------------------------------
**		BMP ファイルの書き込み
*/

/*
**		.bmp ファイルの書き込み
*/
static BOOL write_bmp(char *fn, IMAGE *img)
{
	BYTE bfh[FILEHED_SIZE + BMPV4HED_SIZE];
	BYTE *const bih = bfh + FILEHED_SIZE;
	BYTE rgbq[RGBQUAD_SIZE];
	BOOL alpha_bitfield;
	DWORD bihsize, offbits, filesize;
	PALETTE *pal;
	const char *errmsg;
	FILE *fp;
	UINT i;

	if (fn == NULL) {
		fn = " (stdout)";
		fp = binary_stdio(fileno(stdout));
	} else {
		fp = fopen(fn, "wb");
	}
	if (fp == NULL) ERROR_ABORT(err_wopenfail);

	set_status("Writing %.80s", basname(fn));

	/* ------------------------------------------------------ */

	alpha_bitfield = (img->alpha && alpha_format == P2B_ALPHABMP_BITFIELD);
	bihsize = (alpha_bitfield) ? BMPV4HED_SIZE : INFOHED_SIZE;
	offbits = FILEHED_SIZE + bihsize + RGBQUAD_SIZE * img->palnum;
	filesize = offbits + img->imgbytes;

	memset(bfh, 0, sizeof(bfh));

	mputwl( bfh + BFH_WTYPE   , BMP_SIGNATURE);
	mputdwl(bfh + BFH_DSIZE   , filesize);
	mputdwl(bfh + BFH_DOFFBITS, offbits);

	mputdwl(bih + BIH_DSIZE     , bihsize);
	mputdwl(bih + BIH_LWIDTH    , (DWORD)img->width);
	mputdwl(bih + BIH_LHEIGHT   , (DWORD)img->height);
	mputwl( bih + BIH_WPLANES   , 1);
	mputwl( bih + BIH_WBITCOUNT , img->pixdepth);
	mputdwl(bih + BIH_DSIZEIMAGE, img->imgbytes);

	if (alpha_bitfield) {
		mputdwl(bih + BIH_DCOMPRESSION, BI_BITFIELDS);
		mputdwl(bih + B4H_DALPHAMASK, 0xFF000000);
		mputdwl(bih + B4H_DREDMASK  , 0x00FF0000);
		mputdwl(bih + B4H_DGREENMASK, 0x0000FF00);
		mputdwl(bih + B4H_DBLUEMASK , 0x000000FF);
	}

	if (fwrite(bfh, (FILEHED_SIZE + bihsize), 1, fp) != 1)
		ERROR_ABORT(err_writeerr);

	/* ------------------------------------------------------ */

	memset(rgbq, 0, sizeof(rgbq));

	for (pal = img->palette, i = img->palnum; i > 0; i--, pal++) {
		rgbq[RGBQ_RED]   = pal->red;
		rgbq[RGBQ_GREEN] = pal->green;
		rgbq[RGBQ_BLUE]  = pal->blue;
		if (fwrite(rgbq, RGBQUAD_SIZE, 1, fp) != 1)
			ERROR_ABORT(err_writeerr);
	}

	/* ------------------------------------------------------ */

	if ((errmsg = write_rgb_bits(img, fp)) != NULL) ERROR_ABORT(errmsg);

	/* ------------------------------------------------------ */

	set_status("OK      %.80s", basname(fn));
	feed_line();

	fflush(fp);
	if (fp != stdout) fclose(fp);
	imgbuf_free(img);

	return TRUE;

error_abort:				/* error */
	xxprintf(errmsg, fn);
	if (fp != stdout && fp != NULL) fclose(fp);
	imgbuf_free(img);

	return FALSE;
}


/*
**		BI_RGB (無圧縮) 形式の画像データを書く
*/
static const char *write_rgb_bits(IMAGE *img, FILE *fp)
{
#if 1
	DWORD wr  = 16*1024*1024;
	DWORD num = img->imgbytes;
	BYTE *ptr = img->bmpbits;

	while (num > 0) {
		if (wr > num) wr = num;

		if (fwrite(ptr, wr, 1, fp) != 1)
			return err_writeerr;

		ptr += wr; num -= wr;
	}
#else
	if (fwrite(img->bmpbits, img->imgbytes, 1, fp) != 1)
		return err_writeerr;
#endif
	return NULL;
}


/*
**	メモリへ little-endien 形式 4バイト無符号整数を書く
*/
static void mputdwl(void *ptr, unsigned long val)
{
	unsigned char *p = ptr;

	p[0] = (unsigned char)(val       & 0xFF);
	p[1] = (unsigned char)(val >>  8 & 0xFF);
	p[2] = (unsigned char)(val >> 16 & 0xFF);
	p[3] = (unsigned char)(val >> 24 & 0xFF);
}


/*
**	メモリへ little-endien 形式 2バイト無符号整数を書く
*/
static void mputwl(void *ptr, unsigned int val)
{
	unsigned char *p = ptr;

	p[0] = (unsigned char)(val      & 0xFF);
	p[1] = (unsigned char)(val >> 8 & 0xFF);
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
		"png2bmp, PNG -> BMP コンバータ - version " PNG2BMP_VERSION "\n"
		"   " PNG2BMP_COPYRIGHT "\n"
		"   Compiled with libpng " PNG_LIBPNG_VER_STRING " and zlib " ZLIB_VERSION ".\n"
		"\n"
		"使い方 : %s [-スイッチ] 入力ファイル名 ...\n"
		"       : ... | %s [-スイッチ] | ...\n"
		"\n"
		"入力ファイル名にはワイルドカードが使えます (* と ?)\n"
		"出力ファイル名は入力ファイル名の拡張子を .bmp に変えた名前になります\n"
		"\n"
		"スイッチオプション (小文字でも可) :\n"
		"   -A       アルファチャネルを保存する (32bit ARGB BMP 形式で)\n"
		"   -B       アルファチャネルを保存する (32bit Bitfield BMP 形式で)\n"
		"   -R       透明色をアルファチャネルに変換する (-A か -B と共に使う)\n"
		"   -O name  出力ファイル名を指定する\n"
		"   -D dir   ファイルを出力するディレクトリを指定する\n"
		"   -E       変換が成功した場合には入力ファイルを削除する\n"
		"   -T       入力ファイルのタイムスタンプを出力ファイルに設定する\n"
		"   -Q       処理中, 一切の" SJ_ESC("表\示","表示") "をしない\n"
		"   -L       処理中のエラーをログファイル(%s)に記録する\n"
		"   -X       標準入力／標準出力を介した変換を無効にする\n";
#else  /* ----------------------------------- */
		"png2bmp, a PNG-to-BMP converter - version " PNG2BMP_VERSION "\n"
		"   " PNG2BMP_COPYRIGHT "\n"
		"   Compiled with libpng " PNG_LIBPNG_VER_STRING " and zlib " ZLIB_VERSION ".\n"
		"\n"
		"Usage: %s [-switches] inputfile(s) ...\n"
		"   or: ... | %s [-switches] | ...\n"
		"\n"
		"List of input files may use wildcards (* and ?)\n"
		"Output filename is same as input filename, but extension .bmp\n"
		"\n"
		"Switches (case-insensitive) :\n"
		"   -A       Preserve alpha channel (save in 32bit ARGB BMP format)\n"
		"   -B       Preserve alpha channel (save in 32bit Bitfield BMP format)\n"
		"   -R       Convert transparent color to alpha channel (use with -A or -B)\n"
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
	fprintf(stdout, str_usage, argv0, argv0, errlogfile);

	exit(status);
}

