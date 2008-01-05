#include "bm.h"

#define WID 64
/** Some global **/
int corpse=0, mesh =0,slant=0;
int rim=0;
int exp_wall;
int dsize;
int sx32 = 16;
int sy32 = 24;

FILE *mfp,*sfp;
char outname[1024], ctgname[100], subsname[100];
char sdir[300];
char realname[1024];


/*** BUFFER MEMORY ***/
#define XX 16
int xx0;
#define YY 30
#define LX (XX)

/*** tmp buffer, floor , final output, final queue ***/
unsigned char *tbuf[3],fbuf[3][128*64],
 *obuf[3],dbuf[3][128*64];


unsigned char wallbuf[4][3][32*48];
unsigned char wall2buf[3][128*64];


int f_wx;

/*** normal floor*/
#define WOADR(x,y,xx,yy) \
((x)*64+xx+  xx0*64*((y)*64+yy))


#define ADR32(x,y) ((x)+(y)*32)
#define ADR64(x,y) ((x)+(y)*64)


/*** output width/height in block ***/
int bx,by;


/**************************/
/* Wrapper routines *******/
/**************************/

int load_it(char *fnam, int *wx, int *wy)
{
  sprintf(realname,"%s%s%c%s.bmp",cpath,sdir,PATHSEP,fnam);
  if(bmread(realname,wx,wy,tbuf)==0) return 0;

  sprintf(realname,"%s%s.bmp",cpath,fnam);
  if(bmread(realname,wx,wy,tbuf)==0) return 0;

  if(subsname[0]){
    sprintf(realname,"%s%s%c%s.bmp",cpath,sdir,PATHSEP,subsname);
    if(bmread(realname,wx,wy,tbuf)==0) return 0;

    sprintf(realname,"%s%s.bmp",cpath,subsname);
    if(bmread(realname,wx,wy,tbuf)==0) return 0;
  }

return 1;
}


void clr_dbuf() {
int xx,yy;

for(xx=0;xx<64;xx++){
for(yy=0;yy<64;yy++){
dbuf[0][ ADR64(xx,yy) ]=0x47;
dbuf[1][ ADR64(xx,yy) ]=0x6c;
dbuf[2][ ADR64(xx,yy) ]=0x6c;
}}
}

#define TILEX 64
#define TILEY 64
void make_rim(){
static unsigned char dflag[TILEX][TILEY];
int x,y,c,dd[3],ad;
int n0,n1,n2;

for(y=0;y<TILEY;y++){
for(x=0;x<TILEX;x++){
  dflag[x][y]=1;
  ad=x + y *TILEX;
  for(c=0;c<3;c++)dd[c]=dbuf[c][ad];
  if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c)) dflag[x][y]=0;
  if( (dd[0]==0)&&(dd[1]==0)&& (dd[2]==0)) dflag[x][y]=2;
}
}

for(x=0;x<TILEX;x++){
for(y=0;y<TILEY;y++){
  ad=x+y*TILEX;
if(dflag[x][y]==2 || dflag[x][y]==0){
n0=n1=n2=0;
if(x>0){ 
 if(dflag[x-1][y]==0) n0++;
 if(dflag[x-1][y]==1) n1++;
 if(dflag[x-1][y]==2) n2++;
}

if(y>0){
 if(dflag[x][y-1]==0) n0++;
 if(dflag[x][y-1]==1) n1++;
 if(dflag[x][y-1]==2) n2++;
}

if(x<TILEX-1){
 if(dflag[x+1][y]==0) n0++;
 if(dflag[x+1][y]==1) n1++;
 if(dflag[x+1][y]==2) n2++;
}
if(y<TILEY-1){
 if(dflag[x][y+1]==0) n0++;
 if(dflag[x][y+1]==1) n1++;
 if(dflag[x][y+1]==2) n2++;
}

if(n1!=0 )
dbuf[0][x+y*TILEX]=dbuf[1][x+y*TILEX]=dbuf[2][x+y*TILEX]=0x10;


}}}

}

void cp_floor_64(){
int xx,yy,c;
for(xx=0;xx<64;xx++)
for(yy=0;yy<64;yy++)
for(c=0;c<3;c++)
dbuf[c][ ADR64(xx,yy) ]=fbuf[c][ ADR64(xx,yy)];
}

void cp_floor_32(){
int xx,yy,c;
for(xx=0;xx<32;xx++)
for(yy=0;yy<32;yy++)
for(c=0;c<3;c++)
dbuf[c][ ADR64(32+xx-yy,32+(xx+yy)/2) ]=fbuf[c][ ADR32(xx,yy)];
}


void cp_monst_32(){
int xx,yy,c,dd[3],ad;
char dflag[32][32];
int xmin,xmax,ymin,ymax;
int ox=0;
int oy=0;

if(corpse==1 ){
	xmin=ymin=31;
	xmax=ymax=0;
	for(xx=0;xx<32;xx++){
	for(yy=0;yy<32;yy++){
		ad=ADR32(xx,yy);
		for(c=0;c<3;c++)dd[c]=tbuf[c][ad];
		if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;
		if( (dd[0]==0)&&(dd[1]==0)&& (dd[2]==0))continue;
		if(xx<xmin)xmin=xx;
		if(xx>xmax)xmax=xx;
		if(yy<ymin)ymin=yy;
		if(yy>ymax)ymax=yy;
	}}/*xy**/
	ox=(xmax+xmin)/2-16;
	oy=(ymax+ymin)/2-16;
}

if(slant==1){
  for(xx=0;xx<32;xx++){
  for(yy=0;yy<32;yy++){
    int x1 = xx-yy+32;
    int y1=  32+(xx+yy)/2;
    ad = ADR32(xx,yy);
    for(c=0;c<3;c++){dd[c]=tbuf[c][ad];}
    if(mesh==2){
      if( (dd[0]!=0x47)||(dd[1]!=0x6c)|| (dd[2]!=0x6c)){
        if( ((x1+y1)&1) ==0)dd[0]=dd[1]=dd[2]=0;
      }
    }
    if(mesh==1){
      if( (((x1/2)+(y1/2))&1) ==1)dd[0]=dd[1]=dd[2]=0;
    }
    if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;
    for(c=0;c<3;c++) {dbuf[c][ADR64(x1,y1)]=dd[c];}
  }}
return;
}

if(dsize==1){
  for(xx=0;xx<32;xx++){
  for(yy=0;yy<32;yy++){
    int x1 = xx*2;
    int y1=  yy*2;
    ad = ADR32(xx,yy);
    for(c=0;c<3;c++){dd[c]=tbuf[c][ad];}
    if(mesh==2){
      if( (dd[0]!=0x47)||(dd[1]!=0x6c)|| (dd[2]!=0x6c)){
        if( ((x1+y1)&1) ==0)dd[0]=dd[1]=dd[2]=0;
      }
    }
    if(mesh==1){
      if( (((x1/2)+(y1/2))&1) ==1)dd[0]=dd[1]=dd[2]=0;
    }
    if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;
    for(c=0;c<3;c++) 
    {
        dbuf[c][ADR64(x1,y1)]=dd[c];
        dbuf[c][ADR64(x1+1,y1)]=dd[c];
        dbuf[c][ADR64(x1,y1+1)]=dd[c];
        dbuf[c][ADR64(x1+1,y1+1)]=dd[c];
    }
  }}
return;
}


for(xx=0;xx<32;xx++){
for(yy=0;yy<32;yy++){
dflag[xx][yy]=0;
ad=ADR32(xx,yy);

if(corpse==1){
    int x1=xx+ox;
    int y1=(yy+oy)*2-16;
    int cy=18;
    if(xx<4 || xx>=28)cy+=2;else
    if(xx<12 || xx>=20) cy+=1;

    if(yy>=cy-1 && yy<=cy+0)continue;
    x1 += (y1-16)/4;
    if(y1>=cy){y1-=2;x1-=3;}else {y1 +=2;x1+=3;}
    if(x1<0 || x1>=32 || y1<0 || y1>=32)continue;
    ad=ADR32(x1,y1);
}

/*** normal***/
for(c=0;c<3;c++){dd[c]=tbuf[c][ad];}
if(mesh==2){
if( (dd[0]!=0x47)||(dd[1]!=0x6c)|| (dd[2]!=0x6c)){
if( ((xx+yy)&1) ==0)dd[0]=dd[1]=dd[2]=0;
}
}
if(mesh==1){
if( (((xx/2)+(yy/2))&1) ==1)dd[0]=dd[1]=dd[2]=0;
}

if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;
if( (corpse==1) &&(dd[0]==0)&&(dd[1]==0)&& (dd[2]==0))continue;

for(c=0;c<3;c++) {dbuf[c][ADR64(sx32+xx,sy32+yy)]=dd[c];}
 dflag[xx][yy]=1;
}}


#if 1
if(corpse==1){
for(xx=0;xx<32;xx++){
int cy=18;
if(xx<4 || xx>=28)cy+=2;else
if(xx<12 || xx>=20) cy+=1;
if(dflag[xx][cy-2]==1 && dflag[xx][cy+1]==1 ){
for(yy=cy-1;yy<=cy-0;yy++){ dbuf[0][ADR64(16+xx,32+yy)]=32;
dbuf[1][ADR64(16+xx,32+yy)]=0;dbuf[2][ADR64(16+xx,32+yy)]=0;
dflag[xx][yy]=1;
}}
}

/** shade**/
for(xx=1;xx<32;xx++){
for(yy=1;yy<32;yy++){
if(dflag[xx][yy]==0 && dflag[xx-1][yy-1]==1){
dbuf[0][ADR64(xx,yy)]=0;
dbuf[1][ADR64(xx,yy)]=0;
dbuf[2][ADR64(xx,yy)]=0;
}
}}

for(xx=3;xx<32;xx++){
for(yy=3;yy<32;yy++){
if(dflag[xx][yy]==0 && dflag[xx-1][yy-1]==0
 && dflag[xx-2][yy-2]==1  && dflag[xx-3][yy-3]==1){
dbuf[0][ADR64(xx,yy)]=0;
dbuf[1][ADR64(xx,yy)]=0;
dbuf[2][ADR64(xx,yy)]=0;
}
}}

}
#endif
}

void cp_monst_64(){
int xx,yy,c,dd[3],ad;
for(xx=0;xx<64;xx++){
for(yy=0;yy<64;yy++){
    ad=ADR64(xx,yy);
    /*** normal***/
    for(c=0;c<3;c++){dd[c]=tbuf[c][ad];}
    if(mesh==2)
    {
        if( (dd[0]!=0x47)||(dd[1]!=0x6c)|| (dd[2]!=0x6c))
            if( ((xx+yy)&1) ==0)dd[0]=dd[1]=dd[2]=0;
    }

    if(mesh==1)
        if( (((xx/2)+(yy/2))&1) ==1)dd[0]=dd[1]=dd[2]=0;

    if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;

    for(c=0;c<3;c++) {dbuf[c][ADR64(xx,yy)]=dd[c];}
}}
}


void cp_monst_4864(){
int xx,yy,c,dd[3],ad;
for(xx=0;xx<48;xx++){
for(yy=0;yy<64;yy++){
    ad= xx+yy*48;
    /*** normal***/
    for(c=0;c<3;c++){dd[c]=tbuf[c][ad];}
    if(mesh==2)
    {
        if( (dd[0]!=0x47)||(dd[1]!=0x6c)|| (dd[2]!=0x6c))
            if( ((xx+yy)&1) ==0)dd[0]=dd[1]=dd[2]=0;
    }

    if(mesh==1)
        if( (((xx/2)+(yy/2))&1) ==1)dd[0]=dd[1]=dd[2]=0;

    if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;

    for(c=0;c<3;c++) {dbuf[c][8+xx+yy*64]=dd[c];}
}}
}

void bflush(){
int xx,yy,c;
if(rim==1) make_rim();

    fprintf(mfp,"<area shape=\"rect\" coords=\"%d,%d,%d,%d\" href=%s>\n",
    bx*WID,by*WID,bx*WID+WID-1,by*WID+WID-1,
    realname);

for(xx=0;xx<64;xx++){
for(yy=0;yy<64;yy++){
for(c=0;c<3;c++){
obuf[c][WOADR(bx,by,xx,yy)]= dbuf[c][ADR64(xx,yy)];
}}}
}


void copy_wall(int wall_ix, int xofs, int yofs){
int xx,yy,c;
unsigned char dd[3];
     for(xx=0;xx<64;xx++){
     for(yy=0;yy<64;yy++){
       int x=xx-xofs-16;
       int y=yy-yofs-8;
       int ad = x+y*32;
       if(x<0 || y<0 || x>=32 || y>=48) continue;
       for(c=0;c<3;c++){dd[c]=wallbuf[wall_ix][c][ad];}
       if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;
       for(c=0;c<3;c++) {dbuf[c][ADR64(xx,yy)]=dd[c];}
     }}
}

void copy_wall_vert(int wall_ix, int xofs, int yofs){
int xx,yy,c,ymax;
unsigned char dd[3];
    for(xx=0;xx<64;xx++){
    for(yy=0;yy<64;yy++){
	int x=xx-xofs-16;
	int y=yy-yofs-8;
	int ad = x+y*32;
	if(x<0 || y<0 || x>=32 || y>=48) continue;

	ymax= 8+x/2;
	if(ymax> 8+(31-x)/2) ymax=8+(31-x)/2;
	if(y<=ymax) continue;
 
        for(c=0;c<3;c++){dd[c]=wallbuf[wall_ix][c][ad];}
        if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;

	//Mesh
//	if( ((x/2+y/2)&1) == 0) dd[0]=dd[1]=dd[2]=0;

        for(c=0;c<3;c++) {dbuf[c][ADR64(xx,yy)]=dd[c];}
    }}
}

void expand_wall(){
//unsigned char wallbuf[4][3][32*48];
int xx,yy,c,ix;
exp_wall=1;
for(ix=0;ix<4;ix++){
for(xx=0;xx<32;xx++){
for(yy=0;yy<48;yy++){
wallbuf[ix][0][xx+yy*32]=0x47;
wallbuf[ix][1][xx+yy*32]=0x6c;
wallbuf[ix][2][xx+yy*32]=0x6c;
}}}

//decompose wall bmp
for(xx=0;xx<32;xx++){
  int ymax= 8+xx/2;
  if(ymax> 8+(31-xx)/2) ymax=8+(31-xx)/2;
  for(yy=0;yy<ymax;yy++){
    ix=0;
    if(2*yy+xx >=32)ix +=1;
    if(2*yy-xx >=0 )ix +=2;
    for(c=0;c<3;c++)wallbuf[ix][c][xx+yy*32]=tbuf[c][xx+yy*32];
  }

  for(yy=ymax;yy<48;yy++){
    if(xx<8) ix=2;else if(xx<24) ix=3; else ix=1;
    for(c=0;c<3;c++)wallbuf[ix][c][xx+yy*32]=tbuf[c][xx+yy*32];
  }
}//xx

/*
    0
1  1 2  2
  3 4 5
 6 7 8 9
  A B C
4  D E  8
    F
*/

for(ix=0;ix<16;ix++){
     clr_dbuf();
    if(f_wx==32)cp_floor_32(); else cp_floor_64();

    if((ix&3)==3) copy_wall(3,0,-16);

    if(ix&1) copy_wall(1,-16,-8);
    if(ix&2) copy_wall(2,16,-8);

    if(ix&1) copy_wall(3,-16,-8);
    copy_wall(0, 0,0);
    if(ix&2) copy_wall(3,16,-8);

    if((ix&5)==5) {copy_wall(1,-32,0);copy_wall_vert(2,-16,0);}
    copy_wall(2,0,0);
    copy_wall(1,0,0);
    if((ix&10)==10) {copy_wall(2,32,0);copy_wall_vert(1,16,0);}

    if(ix&4) {copy_wall(0,-16,8);copy_wall_vert(3,-16,0);}
    copy_wall(3,0,0);
    if(ix&8) {copy_wall(0,16,8);copy_wall_vert(3,16,0);}

    if(ix&4) {copy_wall(1,-16,8);copy_wall_vert(2,0,8);}
    if(ix&8) {copy_wall(2,16,8); copy_wall_vert(1,0,8);}
    if((ix&12)==12) {copy_wall(0,0,16);copy_wall_vert(3,0,8);}

    bflush();

    bx++;if(bx==xx0){bx=0;by++;}
  }/*ix*/
}


static void copy_wall2_h1(int ix, int xofs, int yofs){
int xx,yy,c,ad;

unsigned char dd[3];
     for(xx=0;xx<64;xx++){
     for(yy=0;yy<64;yy++){
       int x=xx-xofs;
       int y=yy-yofs;;
       ad = x+64+y*128;
       if (x<0 || y<0 || x>63 || y>63)continue;
	   if(2*y>=x+32) continue;
	   if(2*y>=95-x) continue;
       if((ix%3)==0) if (2*y>=47-x)continue;
       if((ix%3)==1) if ((2*y<47-x) || (2*y>=79-x))continue;
       if((ix%3)==2) if(2*y<79-x)continue;

       if((ix/3)==0) if(2*y>=x-16)continue;
       if((ix/3)==1) if((2*y<x-16) || (2*y>=x+16))continue;
       if((ix/3)==2) if(2*y<x+16) continue;

        for(c=0;c<3;c++){dd[c]=tbuf[c][ad];}
        if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;

        for(c=0;c<3;c++) {dbuf[c][ADR64(xx,yy)]=dd[c];}
     }}
}

void copy_wall2_h2(int ix, int xofs, int yofs){
int xx,yy,c,ad;

unsigned char dd[3];
     for(xx=0;xx<64;xx++){
     for(yy=0;yy<64;yy++){
       int x=xx-xofs;
       int y=yy-yofs;;
       ad = x+y*128;
       if (x<0 || y<0 || x>63 || y>63)continue;
       if(2*y>=x+32) continue;
	   if(2*y>=95-x) continue;
	   
       if ((ix%2)==0)if (2*y>=63-x)continue;
       if((ix%2)==1) if (2*y<63-x)continue;
       	 
       if((ix/2)==0)if(2*y>=x)continue;
       if((ix/2)==1)if(2*y<x)continue;


        for(c=0;c<3;c++){dd[c]=tbuf[c][ad];}
        if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;

        for(c=0;c<3;c++) {dbuf[c][ADR64(xx,yy)]=dd[c];}
     }}
}


void copy_wall_v2(int ix, int kind, int xofs, int yofs){
int xx,yy,c,ymax,ad;
unsigned char dd[3];
    for(xx=0;xx<64;xx++){
    for(yy=0;yy<64;yy++){
	int x=xx-xofs;
	int y=yy-yofs;
	ad = x+kind*64+y*128;
	if(x<0 || y<0 || x>=64 || y>=64) continue;

	ymax= 16+x/2;
	if(x>=32) ymax=16+(63-x)/2;
	if(y<ymax) continue;
    if(y>ymax+32)continue;

    if(ix==0) if(x>=8)continue;
    if(ix==1) if(x<8 || x>=24)continue;
    if(ix==2) if(x<24 || x>=40)continue;
    if(ix==3) if(x<40 || x>=56)continue;
    if(ix==4) if(x<56)continue;
    
    for(c=0;c<3;c++){dd[c]=tbuf[c][ad];}
        if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;
        for(c=0;c<3;c++) {dbuf[c][ADR64(xx,yy)]=dd[c];}
    }}
}
void expand_wall2(){
//void copy_wall2_h(int kind, int ix, int xofs, int yofs)
int ix;
exp_wall=1;

for(ix=0;ix<16;ix++){
     clr_dbuf();
    if(f_wx==32)cp_floor_32(); else cp_floor_64();

if((ix&3)==0) copy_wall2_h1(0,   0,  8);
if((ix&3)==1) copy_wall2_h1(1, -16,  0);
if((ix&3)==2) copy_wall2_h1(3,  16,  0);
if((ix&3)==3) copy_wall2_h2(0, 0, 0);

if((ix&5)==0) copy_wall2_h1(6,  16,  0);
if((ix&5)==1) copy_wall2_h1(7,   0, -8);
if((ix&5)==4) copy_wall2_h1(3,   0,  8);
if((ix&5)==5) copy_wall2_h2(2, 0, 0);


if((ix&10)==0) copy_wall2_h1(2, -16,  0);
if((ix&10)==2) copy_wall2_h1(5,   0, -8);
if((ix&10)==8) copy_wall2_h1(1,   0,  8);
if((ix&10)==10) copy_wall2_h2(1, 0, 0);

if((ix&12)==0) copy_wall2_h1(8,   0, -8);
if((ix&12)==4) copy_wall2_h1(5, -16,  0);
if((ix&12)==8) copy_wall2_h1(7,  16,  0);
if((ix&12)==12) copy_wall2_h2(3, 0, 0);


if((ix&5)==5) copy_wall_v2(0, 0,  0,  0);
if((ix&10)==10) copy_wall_v2(4, 0,  0,  0);

if((ix&4)!=0) copy_wall_v2(1, 0,  0,  0);
if((ix&8)!=0) copy_wall_v2(3, 0,  0,  0);


if((ix&12)==12) copy_wall_v2(2, 0,  0,  0);

if((ix&5)==1) copy_wall_v2(1, 1,  0,  -8);
if((ix&12)==8) copy_wall_v2(1, 1,  16,  0);

if((ix&10)==2) copy_wall_v2(3, 1,  0,  -8);
if((ix&12)==4) copy_wall_v2(3, 1, -16,  0);

if((ix&5)==0)  copy_wall_v2(0, 1,  16,  0);
if((ix&10)==0) copy_wall_v2(4, 1, -16,  0);
if((ix&12)==0) copy_wall_v2(2, 1,   0, -8);

    bflush();
    bx++;if(bx==xx0){bx=0;by++;}
}
}


void  load_monst(fnam) char *fnam;{
int wx, wy;
    if( load_it(fnam, &wx, &wy))
    {
	 printf("no file %s.bmp\n",fnam);
        getchar();
	exit(1);
    }
    exp_wall=0;
    if(wx==128 && wy==64) expand_wall2();
    else if(wx==48 && wy==64) cp_monst_4864();
    else if(wx==32 && wy==48) expand_wall();
    else if(wx==32)cp_monst_32();
    else if(wx==64)cp_monst_64();
    bflush();
}

void process_config(char *fname)
{
  int i,j;
  char tmp[100],st[1024];
  char *nuke;
  FILE *fp=fopen(fname,"r");
  if(fp==NULL){
    printf("Error no config file %s\nHit return",fname);
    getchar();
    exit(1);
  }


while(1){
  fgets(tmp,99,fp);
  if(feof(fp))break;
  i=0;while(i<99 && tmp[i]>=32)i++;
  tmp[i]=0;

fprintf(stderr,"[%s]\n",tmp);

  if(getname(tmp,"back",st))
  {
    /*** Set Background BMP (format "%back bmpname") ***/
    if(strncmp(st,"none",4)==0)
    {
       /**  clear **/
       for(i=0;i<32*32;i++){fbuf[0][i]=0x47;fbuf[1][i]=fbuf[2][i]=0x6c;}
       f_wx=64;
    }
    else
    {
       int wy;
       load_it(st, &f_wx, &wy);
       for(i=0;i<f_wx*wy;i++)for(j=0;j<3;j++)fbuf[j][i]=tbuf[j][i];
    }
    continue;
  }

  if (getname(tmp,"include",st)){
   char fn2[200];
   sprintf(fn2,"%s%s",cpath, st);
   if(strcmp(fname,fn2)!=0) process_config(fn2);
   continue;
  }

  if (getval(tmp,"slant",&slant)) continue;
  if (getval(tmp,"dsize",&dsize)) continue;
  if (getval(tmp,"mesh",&mesh)) continue;
  if (getval(tmp,"rim",&rim)) continue;
  if (getval(tmp,"corpose",&corpse)) continue;
  if (getname(tmp,"ctg",ctgname)) continue;
  if (getname(tmp,"subst",subsname)) continue;
  if (getname(tmp,"sdir",sdir)) continue;
  if (getname(tmp,"name", outname)) continue;
  if (getval(tmp,"width",&xx0)) continue;
  if (getval(tmp,"sx",&sx32)) continue;
  if (getval(tmp,"sy",&sy32)) continue;
  if (tmp[0]=='#' || tmp[0]<32){
    if(tmp[0]<32)  {}
    else fprintf(sfp,"\n//%s\n",tmp);
    continue;
  } 

   /*** normal bitmap ***/

    clr_dbuf();
    if(f_wx==32)cp_floor_32(); else cp_floor_64();
    i=0;while(i<99 && tmp[i]>32)i++;
    tmp[i]=0; strcpy(st, &tmp[i+1]);
    load_monst(tmp);
    if(!strstr(st,"IGNORE_COMMENT"))
    {
        nuke=strstr(st,"/*");if(nuke)*nuke=0;
        if(exp_wall)
        fprintf(sfp,"TILE_%s, (TILE_TOTAL+%d),\n",st,bx+by*xx0-16);
	else
        fprintf(sfp,"TILE_%s, (TILE_TOTAL+%d),\n",st,bx+by*xx0);
    }

    if(!exp_wall){bx++;if(bx==xx0){bx=0;by++;}}

}/* while */

 fclose(fp);
}

int main(argc,argv)
int argc;
char *argv[];
{

  int i;
  char fn[100];

  fixalloc(tbuf,256*256);

  slant=corpse=mesh=dsize=0;

  bx=by=0;
  process_cpath(argv[0]);
  fixalloc(obuf, LX*64*(YY)*64);


  xx0=XX;
  ctgname[0]=0;
  subsname[0]=0;
  sdir[0]=0;

  stdpal();

  strcpy(outname,"tile");

sprintf(fn,"%stiledef-qv.h",cpath);
sfp=fopen(fn,"w");
if(sfp==NULL){
        printf("Error could not open %s\nHit return",fn);
        getchar();
        exit(1);
}

mfp=fopen("map.htm","w");
fprintf(mfp,"<HTML><head>\n");
fprintf(mfp,"<base href=\"http://cvs.sourceforge.net/viewcvs.py/rltiles/rltiles/
\">\n");
fprintf(mfp,"</head><body><MAP NAME=\"nhmap\">\n");


fprintf(sfp,"/* Automatically generated by tile generator. */\n");
fprintf(sfp,"const int tile_qv_pair_table[] ={\n");

  printf("%s\ncpath=%s\n",argv[0],cpath);
  if(argc==1)
    sprintf(fn,"%sdc-qv.txt",cpath);
  else strcpy(fn,argv[1]);
  process_config(fn);


fprintf(sfp,"-1, -1 };\n");

fprintf(sfp,"\n#define TILE_TOTAL_EX %d\n",bx+by*xx0);
fprintf(sfp,"#define TILE_PER_ROW_EX %d\n",xx0);


fclose(sfp);
i=by*64;if(bx!=0)i+=64;

sprintf(fn,"%s%s.bmp",cpath,outname);
bmwrite(fn,xx0*64,i,obuf);

fprintf(mfp,"<IMG SRC=http://rltiles.sf.net/%s.png USEMAP=\"#nhmap\" >\n</body>\
n</html>\n", outname);
fclose(mfp);

return 0;
}
