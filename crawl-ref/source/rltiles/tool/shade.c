#include "bm.h"

int myrand()
{
    static int seed=12345;
    seed *= 65539;
    return(seed&0x7fffffff);
}

int same_hue(int r, int g, int b, int r0, int g0, int b0)
{
    if (r==255)r=256;
    if (g==255)g=256;
    if (b==255)b=256;

    if(r0*g == g0*r && g0*b == b0*g && b0*r == r0*b) return 1;
    return 0;
}

void convert_hue(unsigned char *r, unsigned char *g, unsigned char *b,
                int r0, int g0, int b0, int modif)
{
    int rr,gg,bb;
    int max0 = r0;
    int max = *r;
    if(max<*g) max=*g;
    if(max<*b) max=*b;
    if(max==255) max=256;

    if(max0<g0) max0=g0;
    if(max0<b0) max0=b0;

    if (max <=32) modif /= 4;
    else
    if (max <=64) modif /= 2;

    rr = ( (max+modif) * r0 )/ max0;
    gg = ( (max+modif) * g0 )/ max0;
    bb = ( (max+modif) * b0 )/ max0;

    if(rr>255) rr=255;
    else if(rr<0) rr=0;
    if(gg>255) gg=255;
    else if(gg<0) gg=0;
    if(bb>255) bb=255;
    else if(bb<0) bb=0;

    *r=rr; *g=gg; *b=bb;
}


int main(int argc, char **argv){

unsigned char *ibuf[3];
int x,y;
int i;
char fn[100],st[1024];
char *flag;
unsigned char *nbuf[3];
int ncol[3],ccol[10][3],nccol,ccol2[10][3], modif[10];
FILE *ifp;
int level,l;
int xx,yy,c,f;
float prob,amp;
int thresh;

  stdpal();
  process_cpath(argv[0]);

if(argc!=1)
    strcpy(fn, argv[1]);
else 
    sprintf(fn,"%sshade.txt",cpath);

fprintf(stderr,"FILE=[%s]\n",fn);

ifp=fopen(fn,"r");
myfget(st,ifp);
sprintf(fn,"%s%s.bmp",cpath,st);
fprintf(stderr,"Orig file=[%s]\n",fn);
ibuf[0]=ibuf[1]=ibuf[2]=NULL;
bmread(fn,&x,&y,ibuf );
fprintf(stderr,"loaded  x=%d y=%d\n",x,y);
flag=malloc(x*y);
for(i=0;i<3;i++)nbuf[i]=malloc(x*y);

while(1){
myfget(st,ifp);
if(feof(ifp))break;
level=atoi(st);

//random perturbation amplitude/prob
myfget(st,ifp);
prob=atof(st);
thresh=(int)(0x7fffffff*prob);
if(prob==-1.0)thresh=-1;//ringmail
if(prob==-2.0)thresh=-2;//chainmail

myfget(st,ifp);
amp=atof(st);
printf("P=%f Amp=%f\n",prob,amp);

// Normal col
myfget(st,ifp);
fprintf(stderr,"Normal [%s]\n",st);
sscanf(st,"%d %d %d",&ncol[0],&ncol[1],&ncol[2]);

//Control col
myfget(st,ifp);
if(feof(ifp))break;
nccol=atoi(st);

for(i=0;i<nccol;i++){
	myfget(st,ifp);
	if(feof(ifp))exit(1);
        modif[i]=0;
	l=sscanf(st,"%d %d %d %d %d %d %d",&ccol[i][0],&ccol[i][1],&ccol[i][2]
			,&ccol2[i][0],&ccol2[i][1],&ccol2[i][2], &modif[i]);
	if(l==3){
		ccol2[i][0]=ccol[i][0];
		ccol2[i][1]=ccol[i][1];
		ccol2[i][2]=ccol[i][2];
	}
}//ncol

fprintf(stderr,"Level=%d ccol=%d\n",level,nccol);
fprintf(stderr,"Normal=%d %d %d\n",ncol[0],ncol[1],ncol[2]);

for(xx=0;xx<x;xx++){
for(yy=0;yy<y;yy++){
int ad=xx+yy*x;
flag[ad]=0;
if( same_hue(ibuf[0][ad], ibuf[1][ad], ibuf[2][ad], 
              ncol[0], ncol[1], ncol[2])) flag[ad]=1;
else
{
for(i=0;i<nccol;i++)
    if(same_hue(ibuf[0][ad], ibuf[1][ad], ibuf[2][ad], 
              ccol[i][0], ccol[i][1], ccol[i][2])) flag[ad]=2+i;
}
}}
/***** convert ******/
for(xx=0;xx<x;xx++){
for(yy=0;yy<y;yy++){
	int ad=xx+yy*x;
	int f=flag[ad];
	if(f>1) convert_hue(&ibuf[0][ad],&ibuf[1][ad],&ibuf[2][ad],
                       ccol2[f-2][0],ccol2[f-2][1],ccol2[f-2][2], modif[f-2]);
}
}

/********************************/
for(l=0;l<level;l++){
for(yy=0;yy<y;yy++){
for(xx=0;xx<x;xx++){
int ad=xx+yy*x;
int sum,n;
if(flag[ad]!=1){
for(c=0;c<3;c++)nbuf[c][ad]=ibuf[c][ad];
continue;
}
for(c=0;c<3;c++){
n=0;sum=0; // (int)(ibuf[c][ad])*1;
if(xx>0 && flag[ad-1]!=0){n++;sum+=ibuf[c][ad-1];}
if(xx<x-1 && flag[ad+1]!=0){n++;sum+=ibuf[c][ad+1];}
if(yy>0 && flag[ad-x]!=0){n++;sum+=ibuf[c][ad-x];}
if(yy<y-1 && flag[ad+x]!=0){n++;sum+=ibuf[c][ad+x];}
if(n!=0){
sum +=n/2;
sum/=n;
nbuf[c][ad]=sum;
}else nbuf[c][ad]=ibuf[c][ad];
}/*c*/
ad++;
}}/*xy**/

for(xx=0;xx<x;xx++){
for(yy=0;yy<y;yy++){
int ad=xx+yy*x;
for(c=0;c<3;c++){
ibuf[c][ad]=nbuf[c][ad];}}}
}/*level*/

/**random **/
if(thresh==-1){//ringmail

for(xx=0;xx<x;xx++){
for(yy=0;yy<y;yy++){
	int ad=xx+yy*x;
    if(flag[ad]!=0){
    int dd=0;
    int flag=(xx+2000-3*yy)%5;
    if(flag==0)dd=+64;
    if(flag==3||flag==4)dd=-32;
	for(c=0;c<3;c++){
	  int d=(int)ibuf[c][ad];
	  d=(int)(d+dd);
	  if(d>255)d=255;
	  if(d<0)d=0;
	  ibuf[c][ad]=(unsigned char)d;
	}
}
}}//XY
}//ringmail
if(thresh==-2){//chainmail

for(xx=0;xx<x;xx++){
for(yy=0;yy<y;yy++){
	int ad=xx+yy*x;
if(flag[ad]!=0){
    int dd=0;
    int flag=(xx+2000-2*yy)%4;
    if(flag==0)dd=+64;
	if(flag==1)dd=+32;
    if(flag==3)dd=-32;
	for(c=0;c<3;c++){
	  int d=(int)ibuf[c][ad];
	  d=(int)(d+dd);
	  if(d>255)d=255;
	  if(d<0)d=0;
	  ibuf[c][ad]=(unsigned char)d;
	}
}
}}//XY
}//chainmail

if(thresh>0){
for(xx=0;xx<x;xx++){
for(yy=0;yy<y;yy++){
	int ad=xx+yy*x;
if(myrand()<thresh && flag[ad]!=0){

    double r=1.0-amp+2*amp*(myrand()*1.0/0x7fffffff);
    if(r<0.0)r=0.0;
	for(c=0;c<3;c++){
	  int d=(int)ibuf[c][ad];
	  d=(int)(d*r);
	  if(d>255)d=255;
	  if(d<0)d=0;
	  ibuf[c][ad]=(unsigned char)d;
	}
}
}}//XY
}//if

}/*while*/

sprintf(fn,"%sb.bmp",cpath);
bmwrite_dither(fn,x,y,ibuf ,flag);


fclose(ifp);

}
