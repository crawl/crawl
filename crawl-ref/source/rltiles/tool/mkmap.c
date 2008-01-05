#include "bm.h"

/** Some global **/
int corpse, mesh,slant, rim;
FILE *mfp,*sfp;
char outname[1024], ctgname[100], subsname[100];
char sdir[300];
char realname[1024];

/*** BUFFER MEMORY ***/
#define XX 30
int xx0;
#define YY 90
#define LX (XX)


/*** tmp buffer, floor , final output, final queue ***/
unsigned char *tbuf[3],fbuf[3][32*32], *obuf[3],dbuf[3][32*32];


/*** normal floor*/
#define WOADR(x,y,xx,yy) \
((x)*32+xx+  xx0*32*((y)*32+yy))


#define ADR(x,y) ((x)+(y)*32)

/*** output width/height in block ***/
int bx,by;



/****************************************/
/* Wrapper routines **************/
/**************************/

int load_pxxx(fnam) char *fnam;{
int x,y;

sprintf(realname,"%s%s%c%s.bmp",cpath,sdir,PATHSEP,fnam);
if(bmread(realname,&x,&y,tbuf)==0) return 0;

sprintf(realname,"%s%s.bmp",cpath,fnam);
if(bmread(realname,&x,&y,tbuf)==0) return 0;

if(subsname[0]){
    sprintf(realname,"%s%s%c%s.bmp",cpath,sdir,PATHSEP,subsname);
    if(bmread(realname,&x,&y,tbuf)==0) return 0;

    sprintf(realname,"%s%s.bmp",cpath,subsname);
    if(bmread(realname,&x,&y,tbuf)==0) return 0;
}


return 1;
}


void clr_buf() {
int xx,yy;

for(xx=0;xx<32;xx++){
for(yy=0;yy<32;yy++){
dbuf[0][ ADR(xx,yy) ]=0x47;
dbuf[1][ ADR(xx,yy) ]=0x6c;
dbuf[2][ ADR(xx,yy) ]=0x6c;
}}
}

void cp_floor(){
int xx,yy,c;
for(xx=0;xx<32;xx++)
for(yy=0;yy<32;yy++)
for(c=0;c<3;c++)
dbuf[c][ ADR(xx,yy) ]=fbuf[c][ ADR(xx,yy)];
}

#define TILEX 32
#define TILEY 32

void make_rim(){
static unsigned char dflag[TILEX][TILEY];
int x,y,c,dd[3],ad;
int n0,n1,n2;



for(y=0;y<TILEY;y++){
for(x=0;x<TILEX;x++){
  dflag[x][y]=1;
  ad=ADR(x,y);
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

if(x<31){
 if(dflag[x+1][y]==0) n0++;
 if(dflag[x+1][y]==1) n1++;
 if(dflag[x+1][y]==2) n2++;
}
if(y<31){
 if(dflag[x][y+1]==0) n0++;
 if(dflag[x][y+1]==1) n1++;
 if(dflag[x][y+1]==2) n2++;
}

if(n1!=0 )
dbuf[0][x+y*32]=dbuf[1][x+y*32]=dbuf[2][x+y*32]=0x10;



}}}

}

void cp_monst_32(){
int xx,yy,c,dd[3],ad;
char dflag[32][32];
int xmin,xmax,ymin,ymax,ox,oy;

if(corpse==1 ){
	xmin=ymin=31;
	xmax=ymax=0;
	for(xx=0;xx<32;xx++){
	for(yy=0;yy<32;yy++){
		ad=ADR(xx,yy);
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

for(xx=0;xx<32;xx++){
for(yy=0;yy<32;yy++){
dflag[xx][yy]=0;
ad=ADR(xx,yy);
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
ad=ADR(x1,y1);
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

for(c=0;c<3;c++) {dbuf[c][ADR(xx,yy)]=dd[c];}
 dflag[xx][yy]=1;
}}


#if 1
if(corpse==1){
for(xx=0;xx<32;xx++){
int cy=18;
if(xx<4 || xx>=28)cy+=2;else
if(xx<12 || xx>=20) cy+=1;
if(dflag[xx][cy-2]==1 && dflag[xx][cy+1]==1 ){
for(yy=cy-1;yy<=cy-0;yy++){ dbuf[0][ADR(xx,yy)]=32;
dbuf[1][ADR(xx,yy)]=0;dbuf[2][ADR(xx,yy)]=0;
dflag[xx][yy]=1;
}}
}

/** shade**/
for(xx=1;xx<32;xx++){
for(yy=1;yy<32;yy++){
if(dflag[xx][yy]==0 && dflag[xx-1][yy-1]==1){
dbuf[0][ADR(xx,yy)]=0;
dbuf[1][ADR(xx,yy)]=0;
dbuf[2][ADR(xx,yy)]=0;
}
}}

for(xx=3;xx<32;xx++){
for(yy=3;yy<32;yy++){
if(dflag[xx][yy]==0 && dflag[xx-1][yy-1]==0
 && dflag[xx-2][yy-2]==1  && dflag[xx-3][yy-3]==1){
dbuf[0][ADR(xx,yy)]=0;
dbuf[1][ADR(xx,yy)]=0;
dbuf[2][ADR(xx,yy)]=0;
}
}}




}
#endif
if(rim==1)make_rim();
}


void bflush(){
int xx,yy,c;
for(xx=0;xx<32;xx++){
for(yy=0;yy<32;yy++){
for(c=0;c<3;c++){
obuf[c][WOADR(bx,by,xx,yy)]= dbuf[c][ADR(xx,yy)];
}}}
}


void  load_monst(fnam) char *fnam;{
	if( load_pxxx(fnam)){
		 printf("no file pxxx/%s.bmp or %s/%s.bmp\n",fnam,sdir,fnam);
                getchar();
		exit(1);
	}
	cp_monst_32();
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

  if(getname(tmp,"back",st))
  {
    /*** Set Background BMP (format "%back bmpname") ***/
    if(strncmp(st,"none",4)==0)
    {
       /**  clear **/
       for(i=0;i<32*32;i++){fbuf[0][i]=0x47;fbuf[1][i]=fbuf[2][i]=0x6c;}
    }
    else
    {
       load_pxxx(st);
       for(i=0;i<32*32;i++)for(j=0;j<3;j++)fbuf[j][i]=tbuf[j][i];
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
  if (getval(tmp,"rim",&rim)) continue;
  if (getval(tmp,"mesh",&mesh)) continue;
  if (getval(tmp,"corpse",&corpse)) continue;
  if (getname(tmp,"ctg",ctgname)) continue;
  if (getname(tmp,"subst",subsname)) continue;
  if (getname(tmp,"sdir",sdir)) continue;
  if (getname(tmp,"name", outname)) continue;
  if (getval(tmp,"width",&xx0)) continue;
  if (tmp[0]=='#' || tmp[0]<32){
    if(tmp[0]=='#')fprintf(sfp,"//%s\n",tmp);
    if(tmp[0]<32) fprintf(sfp,"\n");
    continue;
  } 

/*** normal bitmap ***/
#define WID 32
clr_buf();cp_floor();
i=0;while(i<99 && tmp[i]>32)i++;
tmp[i]=0; strcpy(st, &tmp[i+1]);
 load_monst(tmp);

fprintf(mfp,"<area shape=\"rect\" coords=\"%d,%d,%d,%d\" href=%s>\n",
bx*WID,by*WID,bx*WID+WID-1,by*WID+WID-1,
realname);

if(!strstr(st,"IGNORE_COMMENT")){
nuke=strstr(st,"/*");if(nuke)*nuke=0;
fprintf(sfp,"#define TILE_%s %d\n",st,bx+by*xx0);
}

bx++;if(bx==xx0){bx=0;by++;;}



}/* while */
 fclose(fp);
}

int main(argc,argv)
int argc;
char *argv[];
{

  int i;
  char fn[100],st2[100];

  slant=corpse=mesh=rim=0;

  bx=by=0;
  process_cpath(argv[0]);

  xx0=XX;
  ctgname[0]=0;
  subsname[0]=0;
  sdir[0]=0;

  stdpal();
  fixalloc(tbuf,256*256);
  fixalloc(obuf, LX*64*(YY)*64);


  strcpy(outname,"tile");

sprintf(fn,"%smap.htm",cpath);
mfp=fopen(fn,"w");
if(mfp==NULL){
	printf("Error could not open %s\nHit return",fn);
	getchar();
	exit(1);
}


sprintf(fn,"%stiledef.h",cpath);
sfp=fopen(fn,"w");
if(sfp==NULL){
        printf("Error could not open %s\nHit return",fn);
        getchar();
        exit(1);
}
fprintf(sfp,"/* Automatically generated by tile generator. */\n");


fprintf(mfp,"<HTML><head>\n");
fprintf(mfp,"<base href=\"http://cvs.sourceforge.net/viewcvs.py/rltiles/rltiles/\">\n");
fprintf(mfp,"</head><body><MAP NAME=\"nhmap\">\n");


  printf("%s\ncpath=%s\n",argv[0],cpath);
  if(argc==1)
    sprintf(fn,"%sdc-all.txt",cpath);
  else strcpy(fn,argv[1]);
  process_config(fn);



fprintf(sfp,"#define TILE_TOTAL %d\n",bx+by*xx0);
fprintf(sfp,"#define TILE_PER_ROW %d\n",xx0);

fprintf(mfp,"<IMG SRC=http://rltiles.sf.net/%s.png USEMAP=\"#nhmap\" >\n</body>\n</html>\n", outname);


fclose(mfp);
fclose(sfp);
i=by*32;if(bx!=0)i+=32;

sprintf(fn,"%s%s.bmp",cpath,outname);
bmwrite(fn,xx0*32,i,obuf);

}
