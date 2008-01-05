/*
**  bmphed.h - .bmp file header macros
**
**  Public domain by MIYASAKA Masaru (July 13, 2004)
*/

#ifndef BMPHED_H
#define BMPHED_H

	/* BMP file signature */
#define BMP_SIGNATURE		0x4D42
#define BMP_SIG_BYTES		2

	/* BITMAPFILEHEADER */
#define BFH_WTYPE			0		/* WORD         bfType;          */
#define BFH_DSIZE			2		/* DWORD        bfSize;          */
#define BFH_WRESERVED1		6		/* WORD         bfReserved1;     */
#define BFH_WRESERVED2		8		/* WORD         bfReserved2;     */
#define BFH_DOFFBITS		10		/* DWORD        bfOffBits;       */
#define BFH_DBIHSIZE		14		/* DWORD        biSize;          */
#define FILEHED_SIZE		14		/* sizeof(BITMAPFILEHEADER)      */
#define BIHSIZE_SIZE		4		/* sizeof(biSize)                */

	/* BITMAPINFOHEADER, BITMAPV4HEADER, BITMAPV5HEADER */
#define BIH_DSIZE			0		/* DWORD        biSize;          */
#define BIH_LWIDTH			4		/* LONG         biWidth;         */
#define BIH_LHEIGHT			8		/* LONG         biHeight;        */
#define BIH_WPLANES			12		/* WORD         biPlanes;        */
#define BIH_WBITCOUNT		14		/* WORD         biBitCount;      */
#define BIH_DCOMPRESSION	16		/* DWORD        biCompression;   */
#define BIH_DSIZEIMAGE		20		/* DWORD        biSizeImage;     */
#define BIH_LXPELSPERMETER	24		/* LONG         biXPelsPerMeter; */
#define BIH_LYPELSPERMETER	28		/* LONG         biYPelsPerMeter; */
#define BIH_DCLRUSED		32		/* DWORD        biClrUsed;       */
#define BIH_DCLRIMPORANT	36		/* DWORD        biClrImportant;  */
#define B4H_DREDMASK		40		/* DWORD        bV4RedMask;      */
#define B4H_DGREENMASK		44		/* DWORD        bV4GreenMask;    */
#define B4H_DBLUEMASK		48		/* DWORD        bV4BlueMask;     */
#define B4H_DALPHAMASK		52		/* DWORD        bV4AlphaMask;    */
#define B4H_DCSTYPE			56		/* DWORD        bV4CSType;       */
#define B4H_XENDPOINTS		60		/* CIEXYZTRIPLE bV4Endpoints;    */
#define B4H_DGAMMARED		96		/* DWORD        bV4GammaRed;     */
#define B4H_DGAMMAGREEN		100		/* DWORD        bV4GammaGreen;   */
#define B4H_DGAMMABLUE		104		/* DWORD        bV4GammaBlue;    */
#define B5H_DINTENT			108		/* DWORD        bV5Intent;       */
#define B5H_DPROFILEDATA	112		/* DWORD        bV5ProfileData;  */
#define B5H_DPROFILESIZE	116		/* DWORD        bV5ProfileSize;  */
#define B5H_DRESERVED		120		/* DWORD        bV5Reserved;     */
#define INFOHED_SIZE		40		/* sizeof(BITMAPINFOHEADER)      */
#define BMPV4HED_SIZE		108		/* sizeof(BITMAPV4HEADER)        */
#define BMPV5HED_SIZE		124		/* sizeof(BITMAPV5HEADER)        */

	/* BITMAPCOREHEADER */
#define BCH_DSIZE			0		/* DWORD  bcSize;                */
#define BCH_WWIDTH			4		/* WORD   bcWidth;               */
#define BCH_WHEIGHT			6		/* WORD   bcHeight;              */
#define BCH_WPLANES			8		/* WORD   bcPlanes;              */
#define BCH_WBITCOUNT		10		/* WORD   bcBitCount;            */
#define COREHED_SIZE		12		/* sizeof(BITMAPCOREHEADER)      */

	/* RGBQUAD */
#define RGBQ_BLUE			0		/* BYTE   rgbBlue;     */
#define RGBQ_GREEN			1		/* BYTE   rgbGreen;    */
#define RGBQ_RED			2		/* BYTE   rgbRed;      */
#define RGBQ_RESERVED		3		/* BYTE   rgbReserved; */
#define RGBQUAD_SIZE		4		/* sizeof(RGBQUAD)     */

	/* RGBTRIPLE */
#define RGBT_BLUE			0		/* BYTE   rgbtBlue;    */
#define RGBT_GREEN			1		/* BYTE   rgbtGreen;   */
#define RGBT_RED			2		/* BYTE   rgbtRed;     */
#define RGBTRIPLE_SIZE		3		/* sizeof(RGBTRIPLE)   */

	/* Constants for the biCompression field */
#ifndef BI_RGB
#define BI_RGB				0L		/* Uncompressed        */
#define BI_RLE8				1L		/* RLE (8 bits/pixel)  */
#define BI_RLE4				2L		/* RLE (4 bits/pixel)  */
#define BI_BITFIELDS		3L		/* Bitfield            */
#define BI_JPEG				4L		/* JPEG Extension      */
#define BI_PNG				5L		/* PNG Extension       */
#endif

#endif /* BMPHED_H */
