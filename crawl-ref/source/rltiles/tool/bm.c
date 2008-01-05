#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "palette.h"

char cpath[1024];
/*** PATH separator ***/

#if defined(_WIN32)|| defined(WINDOWS)
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

void process_cpath(char *path){
  int i, pos;
#ifdef LINUX
  cpath[0]=0;
  return;
#endif
  pos = 0;
  cpath[0]=0;
  printf("path=%s\n",path);
  for(i=0;i<strlen(path);i++)if(path[i]==PATHSEP)pos=i;

  if(pos!=0){
    pos++;
    strncpy(cpath,path,pos);
    cpath[pos]=0;
        printf("pos=%d\n",pos);
  }
}

void fixalloc(char *buf[3], int size)
{
  buf[0]=malloc(size);
  buf[1]=malloc(size);
  buf[2]=malloc(size);
}

#if 0
#define WORD unsigned short
#define DWORD unsigned int

    typedef struct tagBITMAPFILEHEADER {
            WORD    bfType;         // 常に"BM"
            DWORD   bfSize;         // ファイルサイズ
            WORD    bfReserved1;    // 0に設定
            WORD    bfReserved2;    // 0に設定
            DWORD   bfOffBits;      // DIB形式ファイルの先頭からピクセルデータ領
    } BITMAPFILEHEADER;

    typedef struct tagBITMAPINFOHEADER {
            DWORD   biSize;         // この構造体のサイズ
            DWORD   biWidth;                // 幅（ピクセル単位）
            DWORD   biHeight;               // 高さ（ピクセル単位）
            WORD    biPlanes;               // 常に1
            WORD    biBitCount;     // 1ピクセルあたりのカラービットの数
            DWORD   biCompression;  // BI_RGB, BI_RLE8, BI_RLE4のいずれか
            DWORD   biSizeImage;    // イメージの全バイト数
            DWORD   biXPelsPerMeter;        // 0または水平解像度
            DWORD   biYPelsPerMeter;        // 0または垂直解像度
            DWORD   biClrUsed;      // 通常は0、biBitCount以下のカラー数に設定可
            DWORD   biClrImportant; // 通常は0
    } BITMAPINFOHEADER;
#endif

#define  NHASH 512
#define HASHMAX 100
int hashflag;

int hashn[NHASH];
int hashtab[NHASH][HASHMAX][4];
#define RGB2H(r,g,b) (  ((r)>>5)|(((b)>>5)<<3)|(((g)>>5)<<6) )
int palr[256],palg[256],palb[256];

unsigned int rev16(unsigned char *x)
{
    int r=x[1];
    r = (r<<8)|x[0];
    return r;
}
unsigned int rev32(unsigned char *x)
{
    int r=x[3];
    r = (r<<8)|x[2];
    r = (r<<8)|x[1];
    r = (r<<8)|x[0];
    return r;
}

/***** BMP read *****/
int bmread(char *fn, int *x, int *y, unsigned char *buf3[3])
{
unsigned char bmHead[14];
unsigned char bmInfo[40];

unsigned char pbuf[1024];
int i,j,k;
int xx,yy,x0,y0;
FILE *fp;
unsigned char *b0;
int bits, ofbits;

if(NULL==(fp=fopen(fn,"rb")))
{
    //printf("no file %s:",fn);
   return(1);
}
    fread(&bmHead,1,14,fp);
    fread(&bmInfo,1,40,fp);

xx=rev32(&bmInfo[4]);
yy=rev32(&bmInfo[8]);
bits=rev16(&bmInfo[14]);

//fprintf(stderr, "wx = %d wy = %d\n",xx,yy);

if(!buf3[0])buf3[0]=malloc(xx*yy);
if(!buf3[1])buf3[1]=malloc(xx*yy);
if(!buf3[2])buf3[2]=malloc(xx*yy);

ofbits = rev32(&bmHead[10]);

if(bits==24){
fseek(fp, ofbits, SEEK_SET);

b0=malloc(xx*yy*3);
fread(b0,1,3*xx*yy,fp);
fclose(fp);
j=0;
  for(y0=yy-1;y0>=0;y0--){
  for(x0=0;x0<xx;x0++){
      i=y0*xx+x0;
      k=b0[j];j++; buf3[2][i]=k;
      k=b0[j];j++; buf3[1][i]=k;
      k=b0[j];j++; buf3[0][i]=k;
  }}
free(b0);
*x=xx;*y=yy;
return(0);
}

 if(bits==4){
b0=malloc(xx*yy/2);
k=ofbits -54;
k/=4;
fread(pbuf,1,k*4,fp);
fread(b0,1,xx*yy/2,fp);
fclose(fp);

j=0;
for(y0=yy-1;y0>=0;y0--){
for(x0=0;x0<xx;x0++){i=y0*xx+x0;
if(j&1) k=b0[j/2]&0x0f;
 else k=b0[j/2]>>4;
j++;


buf3[0][i]=pbuf[ k*4+2 ];
buf3[1][i]=pbuf[ k*4+1 ];
buf3[2][i]=pbuf[ k*4+0 ];
}}
free(b0);
*x=xx;*y=yy;
return(0);

 }



b0=malloc(xx*yy);
//k=buf[46]+buf[47]*256;
k=ofbits -54;
fread(pbuf,1,k,fp);
fread(b0,1,xx*yy,fp);
fclose(fp);

j=0;
for(y0=yy-1;y0>=0;y0--){
for(x0=0;x0<xx;x0++){i=y0*xx+x0;
k=b0[j];j++;
buf3[0][i]=pbuf[ k*4+2 ];
buf3[1][i]=pbuf[ k*4+1 ];
buf3[2][i]=pbuf[ k*4+0 ];
}}
free(b0);
*x=xx;*y=yy;
return(0);
}


void forcereg(i) int i;{
int h,n,r,g,b;
r=palr[i];
g=palg[i];
b=palb[i];

h=RGB2H(r,g,b);
n=hashn[h];
hashtab[h][n][0]=i;
hashtab[h][n][1]=r;
hashtab[h][n][2]=g;
hashtab[h][n][3]=b;
n++;
hashn[h]=n;
}

void reg_rgb(int i, int r, int g, int b)
{
int h,n;

h=RGB2H(r,g,b);
n=hashn[h];
hashtab[h][n][0]=i;
hashtab[h][n][1]=r;
hashtab[h][n][2]=g;
hashtab[h][n][3]=b;
n++;
hashn[h]=n;
}

int cidx(r,g,b) int r,g,b;{
int r2,r2min,i,h,n,ix,dr,dg,db;

  ix = -1;
  if(hashflag){
    h=RGB2H(r,g,b);
    n=hashn[h];
    for(i=0;i<n;i++)
    {
      if( (r==hashtab[h][i][1])&&(g==hashtab[h][i][2])&&(b==hashtab[h][i][3]) )
      {
        ix=hashtab[h][i][0];
        break;
      }
    }
  }

  if(ix==-1){
    r2min=100000000;
    for(i=0;i<256;i++)
    {
      dr=palr[i]-r;
      dg=palg[i]-g;
      db=palb[i]-b;
      r2=(dr*dr+dg*dg+db*db);
      //r2+=(dr+dg+db)*(dr+dg+db);
      if(r2<r2min){ ix=i;r2min=r2;}
    }

    if(hashflag==1){
      fprintf(stderr,"Color %02x%02x%02xapproximated\n",r,g,b);

      hashtab[h][n][0]=ix;
      hashtab[h][n][1]=r;
      hashtab[h][n][2]=g;
      hashtab[h][n][3]=b;
      n++;
      hashn[h]=n;
      if(n==HASHMAX)
      {
        fprintf(stderr,"HASHMAX exceed! Turning hash off\n");
        hashflag=0;
      }
    }
  }
return(ix);
}

void put4(i,fp) int i;FILE *fp;{
fputc( ((i>> 0)&0xff)  ,fp);
fputc( ((i>> 8)&0xff)  ,fp);
fputc( ((i>>16)&0xff)  ,fp);
fputc( ((i>>24)&0xff)  ,fp);
}

void put2(i,fp) int i;FILE *fp;{
fputc( ((i>> 0)&0xff)  ,fp);
fputc( ((i>> 8)&0xff)  ,fp); }

void bmwrite(char *fn, int x, int y, unsigned char *buf3[3])
{
FILE *fp;
int i,j,k,xx,yy;

hashflag=1;
if(fn[0]==0) fp=stdout; else fp=fopen(fn,"wb");
if (!fp)
{
    printf("Error opening %s.\n", fn);
    exit(-1);
}
fputc('B',fp);fputc('M',fp);
put4(1024+54+x*y,fp);
put4(0,fp);
put4(0x436,fp);
put4(0x28,fp);
put4(x,fp); put4(y,fp);
put2(1,fp);put2(8,fp);
put4(0,fp);put4(x*y,fp);put4(0xb6d,fp);put4(0xb6d,fp);
put4(256,fp);put4(256,fp);
fwrite(pbuf,1,1024,fp);
for(yy=y-1;yy>=0;yy--){
for(xx=0;xx<x;xx++){i=yy*x+xx;
k=cidx( buf3[0][i],buf3[1][i],buf3[2][i] );fputc(k,fp);}}
fclose(fp);}

void bmwrite24(char *fn, int x, int y, unsigned char *buf3[3])
{
FILE *fp;
int i,xx,yy;

if(fn[0]==0) fp=stdout; else fp=fopen(fn,"wb");

fputc('B',fp);fputc('M',fp);
put4(54+3*x*y,fp);
put4(0,fp);
put4(54,fp);
put4(0x28,fp);
put4(x,fp); put4(y,fp);
put2(1,fp);put2(24,fp);
put4(0,fp);put4(x*y*3,fp);put4(0xb6d,fp);put4(0xb6d,fp);
put4(0,fp);put4(0,fp);
for(yy=y-1;yy>=0;yy--){
for(xx=0;xx<x;xx++){i=yy*x+xx;
fputc(buf3[2][i],fp);
fputc(buf3[1][i],fp);
fputc(buf3[0][i],fp);
}}
fclose(fp);}


void bmwrite_dither(char *fn, int x, int y, unsigned char *buf3[3],
                    unsigned char *flag)
{
    FILE *fp;
    int i,j,k,xx,yy;
    int *err_c[3], *err_n[3];
    unsigned char *buf;
    int dx,idat[3],udat[3],putdat[3],err[3];

    fprintf(stderr,"Saving %s x=%d y=%d\n",fn,x,y);

    for(i=0;i<3;i++){
        err_c[i]=malloc(sizeof(int)*(x+2));
        err_n[i]=malloc(sizeof(int)*(x+2));
        for(j=0;j<x+2;j++)err_c[i][j]=err_n[i][j]=0;
    }
    buf=malloc(x*y);

for(yy=0;yy<y;yy++){
//fprintf(stderr,"Y=%d x=%d\n",yy,x);
 
  for(i=0;i<3;i++){
  for(j=0;j<x+2;j++){err_c[i][j]=err_n[i][j];err_n[i][j]=0;}}
 

if((yy&1)==0){//even

for(xx=0;xx<x;xx++){
int do_ep = 1;
idat[0] = buf3[0][ xx+yy*x];
idat[1] = buf3[1][ xx+yy*x];
idat[2] = buf3[2][ xx+yy*x];
for(i=0;i<3;i++) {idat[i] += err_c[i][xx+1];
udat[i]=idat[i];if(udat[i]<0)udat[i]=0;
if(udat[i]>255)udat[i]=255;}

if(buf3[0][xx+yy*x]==0x47 && buf3[1][xx+yy*x]==0x6c && buf3[2][xx+yy*x]==0x6c)
  do_ep=0;
if (flag!=NULL)
{
    if (flag[xx+yy*x]==0) do_ep=0;
}
if (do_ep == 0)
k=cidx( buf3[0][xx+yy*x],buf3[1][xx+yy*x],buf3[2][xx+yy*x]);
else
k=cidx( udat[0],udat[1],udat[2]);

buf[xx+yy*x]=k;
//fprintf(stderr,"Y=%d xx=%d\n",yy,xx);
 
putdat[0]=palr[k];
putdat[1]=palg[k];
putdat[2]=palb[k];
for(i=0;i<3;i++) {
	err[i]=(idat[i]-putdat[i]+8)/16;
	//if(flag[xx+yy*x]!=1)err[i]=0;
	err_c[i][xx+1+1] += err[i]*7;
	err_n[i][xx-1+1] += err[i]*3;
	err_n[i][xx+0+1] += err[i]*5;
	err_n[i][xx+1+1] += err[i];
}/**i**/
}/**x**/
}else{
for(xx=x-1;xx>=0;xx--){
int do_ep=1;
idat[0] = buf3[0][ xx+yy*x];
idat[1] = buf3[1][ xx+yy*x];
idat[2] = buf3[2][ xx+yy*x];
for(i=0;i<3;i++) {idat[i] += err_c[i][xx+1];
udat[i]=idat[i];if(udat[i]<0)udat[i]=0;
if(udat[i]>255)udat[i]=255;}

if(buf3[0][xx+yy*x]==0x47 && buf3[1][xx+yy*x]==0x6c && buf3[2][xx+yy*x]==0x6c)
  do_ep=0;
if (flag!=NULL)
{
    if (flag[xx+yy*x]==0) do_ep=0;
}
if (do_ep == 0)
k=cidx( buf3[0][xx+yy*x],buf3[1][xx+yy*x],buf3[2][xx+yy*x]);
else
k=cidx( udat[0],udat[1],udat[2]);

buf[xx+yy*x]=(unsigned char)k;
putdat[0]=palr[k];
putdat[1]=palg[k];
putdat[2]=palb[k];
for(i=0;i<3;i++) {
        err[i]=(idat[i]-putdat[i]+8)/16;
        	//if(flag[xx+yy*x]!=1)err[i]=0;

        err_c[i][xx-1+1] += err[i]*7;
        err_n[i][xx+1+1] += err[i]*3;
        err_n[i][xx+0+1] += err[i]*5;
        err_n[i][xx-1+1] += err[i];
}/*i*/
}/*x*/

}/*else*/
}/*y*/

for(i=0;i<3;i++){
free(err_c[i]);
free(err_n[i]);}

if(fn[0]==0) fp=stdout; else fp=fopen(fn,"wb");
fputc('B',fp);fputc('M',fp);
put4(1024+54+x*y,fp);
put4(0,fp);
put4(0x436,fp);
put4(0x28,fp);
put4(x,fp);put4(y,fp);
put2(1,fp);put2(8,fp);
put4(0,fp);put4(x*y,fp);put4(0xb6d,fp);put4(0xb6d,fp);
put4(256,fp);put4(256,fp);
fwrite(pbuf,1,1024,fp);
for(yy=y-1;yy>=0;yy--){
for(xx=0;xx<x;xx++){i=yy*x+xx; fputc(buf[i],fp);}}
fclose(fp);
free(buf);

}/** exit**/


void myfget(ss,fp) char *ss;FILE *fp;{
#define STRMAX 200
/****
int ix=1; 
while(1){ ss[0]=getc(fp);
 if( ((ss[0]!=32)&&(ss[0]!='#'))||(feof(fp)))break;
 if(ss[0]=='#') fgets(ss,99,fp);}

while(1){
  ss[ix]=getc(fp);ix++;
  if( (ss[ix-1]<33)||(ix==STRMAX)||(feof(fp)) )break;}
ss[ix-1]=0;printf("%s\n",ss);
****/
int l;
while(1){
        fgets(ss,STRMAX,fp);
        if(feof(fp)){fprintf(stderr,"FILE EOF\n");return;}
        if(ss[0]=='#')continue;
        if(ss[0]<32)continue;
        break;
}
    l=strlen(ss);ss[l-1]=0;
}

void oldcolors()
{
int coldat[] = {
  // Old gold
  0xE6, 0x68541f,
  0xE7, 0x807020,
  0xE8, 0xaa8834,
  0xEA, 0xf2c44d,
  0xEC, 0xfcfc99,

  0xe6, 0x604818,
  0xe7, 0x806020,
  0xe8, 0xa07828,
  0xe9, 0xc09030,
  0xea, 0xe0a838,
  0xeb, 0xffc040,
  //Old zombie
  0xee, 0x756958,
  0xef, 0x91876e,
  0xf0, 0xab9a81,
  //Old brass
  0xf1, 0xe0c0a0,
  0xe9, 0xd0a850
  -1, -1
};
 
  int i=0;

  while(coldat[i]!= -1)
  {
    int ix = coldat[i+0];
    int col= coldat[i+1];
    int r= col>>16;
    int g= (col>>8)&0xff;
    int b= (col)&0xff;

    reg_rgb( ix,r,g,b);
    palr[ix]=r;
    palg[ix]=g;
    palb[ix]=b;
    i+=2;
  }
}

void stdpal(){
int i;

  for(i=0;i<256;i++){
        palr[i]=pbuf[i*4+2];
        palg[i]=pbuf[i*4+1];
        palb[i]=pbuf[i*4+0];
  }
  for(i=0;i<NHASH;i++)hashn[i]=0;
  for(i=0;i<256;i++)forcereg(i);

  oldcolors();
}


int getval(char *buf, char *tag, int *val)
{
    int len = strlen(tag);
    if(buf[0]!='%') return 0;
    if (strncmp(&buf[1], tag, len)!=0) return 0;
    *val = atoi(&buf[len+2]);
    return 1;
}

int getname(char *buf, char *tag, char *name)
{
    int len = strlen(tag);
    if(buf[0]!='%') return 0;
    if (strncmp(&buf[1], tag, len)!=0) return 0;
    strcpy(name, &buf[len+2]);
    return 1;
}

