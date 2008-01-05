#include "bm.h"

/** Some global **/
int corpse=0;
int mesh=0;
int slant=0;
int rim=0;
FILE *mfp=NULL;  // map html
FILE *sfp=NULL;  // "tiledef.h"
FILE *tfp=NULL; // tiles.txt
FILE *cfp=NULL; // lengths of tile counts
int tilecount = 0;
int tilecountidx = -1;
int counts[1024];
int countnames[100][100];

char outname[1024], ctgname[100], subsname[100];
char sdir[300];
char realname[1024];
char imgname[1024];
char tiledefname[1024];
char enumprefix[100];
const int read_size = 2048;

/*** BUFFER MEMORY ***/
#define XX 30
int xx0;
#define YY 90
#define LX (XX)

/*** tmp buffer, floor , final output, final queue ***/
unsigned char *tbuf[3],fbuf[3][32*32],*obuf[3], dbuf[3][32*32];

/*** compose buffer */
unsigned char cbuf[3][32*32];

/*** normal floor*/
#define WOADR(x,y,xx,yy) ((x)*32+xx+  xx0*32*((y)*32+yy))
#define ADR(x,y) ((x)+(y)*32)

/*** output width/height in block ***/
int bx,by;
int filler = 0;

unsigned char bkg[3] = { 0x47, 0x6c, 0x6c };

#define WID 32

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
            dbuf[0][ ADR(xx,yy) ]=bkg[0];
            dbuf[1][ ADR(xx,yy) ]=bkg[1];
            dbuf[2][ ADR(xx,yy) ]=bkg[2];
        }
    }
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

void make_rim(unsigned char buf[3][TILEX*TILEY]){
    static unsigned char dflag[TILEX][TILEY];
    int x,y,c,dd[3],ad;
    int n0,n1,n2;

    // dflag:
    // 0 = background
    // 1 = tile
    // 2 = black

    for(y=0;y<TILEY;y++){
        for(x=0;x<TILEX;x++){
          dflag[x][y]=1;
          ad=ADR(x,y);
          for(c=0;c<3;c++)dd[c]=buf[c][ad];
          if( (dd[0]==bkg[0])&&(dd[1]==bkg[1])&& (dd[2]==bkg[2])) dflag[x][y]=0;
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
                // n1 = tiles adjacent but not diagonal that are tile pixels
                if(n1!=0 )
                    buf[0][x+y*32]=buf[1][x+y*32]=buf[2][x+y*32]=0x10;
            }
        }
    }

}

void cp_monst_32(){
    int xx,yy,c,dd[3],ad;
    char dflag[32][32];
    int xmin,xmax,ymin,ymax,ox,oy;

    if(corpse==1)
    {
        xmin=ymin=31;
        xmax=ymax=0;
        for(xx=0;xx<32;xx++){
            for(yy=0;yy<32;yy++){
                ad=ADR(xx,yy);
                for(c=0;c<3;c++)dd[c]=tbuf[c][ad];
                if( (dd[0]==bkg[0])&&(dd[1]==bkg[1])&& (dd[2]==bkg[2]))continue;
                if( (dd[0]==0)&&(dd[1]==0)&& (dd[2]==0))continue;
                if(xx<xmin)xmin=xx;
                if(xx>xmax)xmax=xx;
                if(yy<ymin)ymin=yy;
                if(yy>ymax)ymax=yy;
            }
        }
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
                if( (dd[0]!=bkg[0])||(dd[1]!=bkg[1])|| (dd[2]!=bkg[2])){
                    if( ((xx+yy)&1) ==0)dd[0]=dd[1]=dd[2]=0;
                }
            }
            if(mesh==1){
                if( (((xx/2)+(yy/2))&1) ==1)dd[0]=dd[1]=dd[2]=0;
            }

            if( (dd[0]==bkg[0])&&(dd[1]==bkg[1])&& (dd[2]==bkg[2]))continue;
            if( (corpse==1) &&(dd[0]==0)&&(dd[1]==0)&& (dd[2]==0))continue;

            for(c=0;c<3;c++) {dbuf[c][ADR(xx,yy)]=dd[c];}
                dflag[xx][yy]=1;
        }
    }


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
if(rim==1)make_rim(dbuf);
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
    int x,y;
    int i,j;
    char tmp[read_size],st[read_size];
    char *nuke;
    FILE *fp=fopen(fname,"r");
    if(fp==NULL){
        printf("Error no config file %s\nHit return",fname);
        getchar();
        exit(1);
    }

    while(1){
        fgets(tmp,read_size,fp);
        if(feof(fp))break;
        i=0;while(i<read_size && tmp[i]>=32)i++;
        tmp[i]=0;

        if(getname(tmp,"back",st))
        {
            /*** Set Background BMP (format "%back bmpname") ***/
            if(strncmp(st,"none",4)==0)
            {
                /**  clear **/
                for(i=0;i<32*32;i++){fbuf[0][i]=bkg[0];fbuf[1][i]=bkg[1];fbuf[2][i]=bkg[2];}
            }
            else
            {
                load_pxxx(st);
                for(i=0;i<32*32;i++)for(j=0;j<3;j++)fbuf[j][i]=tbuf[j][i];
            }
            continue;
        }

        if (getname(tmp,"include",st)){
            char fn2[read_size];
            sprintf(fn2,"%s%s",cpath, st);
            if(strcmp(fname,fn2)!=0) process_config(fn2);
            continue;
        }

        if (getname(tmp,"htmlfile",st))
        {
            char fn2[read_size];
            sprintf(fn2,"%s%s",cpath, st);
            mfp=fopen(fn2,"w");
            if(mfp==NULL)
            {
                printf("Error could not open %s\nHit return",fn2);
                getchar();
                exit(1);
            }
            fprintf(mfp,"<HTML><head>\n");
            continue;
        }

        if (getname(tmp,"tilelist",st))
        {
            char fn2[read_size];
            sprintf(fn2,"%s%s",cpath, st);
            tfp=fopen(fn2,"w");
            if(tfp==NULL)
            {
                printf("Error could not open %s\nHit return",fn2);
                getchar();
                exit(1);
            }
            fprintf(tfp,"%%tilefile %s\n", imgname);
            fprintf(tfp,"%%rim %d\n", rim);
            continue;
        }

        if (getname(tmp,"tiledef",st))
        {
            char fn[read_size];
            sprintf(fn,"%s%s",cpath,st);
            strcpy(tiledefname, st);;
            sfp=fopen(fn,"w");
            if(sfp==NULL)
            {
                printf("Error could not open %s\nHit return",fn);
                getchar();
                exit(1);
            }
            fprintf(sfp,"/* Automatically generated by tile generator. */\n");
            fprintf(sfp, "enum TILE_%sIDX {\n", enumprefix);
            continue;
        }

        if (getname(tmp,"tilecount",st))
        {
            char fn[read_size];
            sprintf(fn,"%s%s",cpath,st);
            cfp=fopen(fn,"w");
            if(cfp==NULL)
            {
                printf("Error could not open %s\nHit return",fn);
                getchar();
                exit(1);
            }
            fprintf(cfp,"/* Automatically generated by tile generator. */\n");
            fprintf(cfp,"#include \"%s\"\n", tiledefname);
            fprintf(cfp, "enum TILE_%sCOUNT_IDX {\n", enumprefix);
            continue;
        }

        if (getname(tmp,"enumprefix",st))
        {
            strcpy(enumprefix, st);
            continue;
        }

        if (getname(tmp,"htmlhead",st))
        {
            if(mfp)fprintf(mfp,"%s\n",st);
            continue;
        }

        if (getname(tmp,"htmlbody",st))
        {
            if(mfp)fprintf(mfp,"</head><body>\n<map name=\"nhmap\">\n");
            continue;
        }

        if (getval(tmp,"slant",&slant)) continue;
        if (getval(tmp,"rim",&rim))
        {
            if (tfp) fprintf(tfp, "%%rim %d\n", rim);
            continue;
        }
        if (getval(tmp,"mesh",&mesh)) continue;
        if (getval(tmp,"corpse",&corpse)) continue;

        if (getname(tmp,"ctg",ctgname)) continue;
        if (getname(tmp,"subst",subsname)) continue;
        if (getname(tmp,"sdir",sdir)) continue;
        if (getname(tmp,"name", outname)) 
        {
            sprintf(imgname, "%s.bmp", outname);
            continue;
        }
        if (getname(tmp,"htmlimg",imgname)) continue;
        if (getval(tmp,"width",&xx0)) continue;
        if (tmp[0]=='#' || tmp[0]<32){
            if(tmp[0]<32) fprintf(sfp,"\n");
            if(tmp[0]=='#')fprintf(sfp,"//%s\n",tmp);
            continue;
        }

        // begin a 32x32 composing sequence
        if (getname(tmp,"start",st))
        {
            clr_buf();
            for (i = 0; i < 32*32; i++)
            {
                cbuf[0][i] = fbuf[0][i];
                cbuf[1][i] = fbuf[1][i];
                cbuf[2][i] = fbuf[2][i];
            }
            continue;
        }

        // compose an image onto the current buffer
        if (getname(tmp,"compose",st))
        {
            unsigned char tempbuf[3][TILEX * TILEY];

            if(load_pxxx(st)){
                printf("no file pxxx/%s.bmp or %s/%s.bmp\n",st,sdir,st);
                getchar();
                exit(1);
            }

            // Copy into a temporary buffer so that we can use the rim func.
            for(i=0;i<TILEX*TILEY;i++)
            {
                tempbuf[0][i] = tbuf[0][i];
                tempbuf[1][i] = tbuf[1][i];
                tempbuf[2][i] = tbuf[2][i];
            }
            if (rim == 1)
                make_rim(tempbuf);

            for(i=0;i<32*32;i++)
            {
                if (tempbuf[0][i] != bkg[0] ||
                    tempbuf[1][i] != bkg[1] ||
                    tempbuf[2][i] != bkg[2])
                {
                    cbuf[0][i] = tempbuf[0][i];
                    cbuf[1][i] = tempbuf[1][i];
                    cbuf[2][i] = tempbuf[2][i];
                }
            }
            continue;
        }

        if (getname(tmp,"nextrow",st))
        {
            if (bx == 0)
                continue;

            while (bx != xx0)
            {
                fprintf(sfp, " TILE_%sFILLER%d,\n", enumprefix, filler++);
                bx++;
            }

            bx = 0;
            by ++;
            continue;
        }

        // finish composing
        if (getname(tmp,"finish",st))
        {
            realname[0] = 0;
            for (i=0;i<32*32;i++)
            {
                tbuf[0][i] = cbuf[0][i];
                tbuf[1][i] = cbuf[1][i];
                tbuf[2][i] = cbuf[2][i];
            }

            // Rim has already been applied during composing, so turn it off
            // temporarily.
            int storerim = rim;
            rim = 0;
            cp_monst_32();
            rim = storerim;

            bflush();
        }
        else
        {
            /*** normal bitmap ***/
            clr_buf();cp_floor();
            i=0;while(i<read_size && tmp[i]>32)i++;
            tmp[i]=0; strcpy(st, &tmp[i+1]);

            if (tfp)
            {
                fprintf(tfp,"%%sx %d\n%%sy %d\n%%ex %d\n%%ey %d\n",
                    bx*WID,by*WID,bx*WID+WID-1,by*WID+WID-1);
            }

            load_monst(tmp);
        }

        if(mfp)
        {
            fprintf(
            mfp,"<area shape=\"rect\" coords=\"%d,%d,%d,%d\" alt=\"%s\" href=\"%s\">\n",
            bx*WID,by*WID,bx*WID+WID-1,by*WID+WID-1,
            st,realname);
        }

        if (tfp)
        {
            if (corpse)
                fprintf(tfp,"%%skip\n");
            else
                fprintf(tfp,"%%file %s\n", realname);
        }

        if(!strstr(st,"IGNORE_COMMENT")){
            nuke=strstr(st,"/*");if(nuke)*nuke=0;
            if (st && strcmp(st, "") != 0 && strcmp(st, "\n") != 0)
            {
                fprintf(sfp," TILE_%s,\n",st);
                if (cfp)
                {
                    if (tilecountidx == -1)
                        tilecountidx++;
                    else
                        counts[tilecountidx++] = tilecount;
                    fprintf(cfp, "    IDX_%s,\n",st);
                    sprintf(countnames[tilecountidx], "TILE_%s", st);
                    tilecount = 1;
                }
            }
            else
            {
                fprintf(sfp, " TILE_%sFILLER%d,\n", enumprefix, filler++);
                tilecount++;
            }
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
    char fn[100];

    bx=by=0;
    process_cpath(argv[0]);

    xx0=XX;
    ctgname[0]=0;
    subsname[0]=0;
    sdir[0]=0;
    enumprefix[0] = 0;

    stdpal();
    fixalloc(tbuf,256*256);
    fixalloc(obuf, LX*64*(YY)*64);


    strcpy(outname,"tile");
    strcpy(imgname,"tile.bmp");


    printf("%s\ncpath=%s\n",argv[0],cpath);
    if(argc==1)
        sprintf(fn,"%sdc-2d.txt",cpath);
    else 
        strcpy(fn,argv[1]);
    process_config(fn);

    if (sfp)
    {
        fprintf(sfp, "TILE_%sTOTAL};\n\n", enumprefix);
        fprintf(sfp,"#define TILE_%sPER_ROW %d\n", enumprefix, xx0);
        fclose(sfp);
    }

    if(mfp)
    {
        fprintf(mfp,"</map>\n<img src=%s usemap=\"#nhmap\" >\n", imgname);
        fprintf(mfp,"</body></html>\n");
        fclose(mfp);
    }

    if (cfp)
    {
        int i;

        fprintf(cfp, "    IDX_%sTOTAL\n};\n\n", enumprefix);

        counts[tilecountidx++] = tilecount;

        fprintf(cfp, "int tile_%scount[IDX_%sTOTAL] =\n{\n", 
            enumprefix, enumprefix);

        for (i = 0; i < tilecountidx; i++)
        {
            fprintf(cfp, (i < tilecountidx - 1) ? " %d,\n" : " %d\n",
                counts[i]);
        }

        fprintf(cfp, "};\n\n");

        fprintf(cfp, "int tile_%sstart[IDX_%sTOTAL] = \n{\n",
            enumprefix, enumprefix);

        for (i = 0; i < tilecountidx; i++)
        {
            fprintf(cfp, (i < tilecountidx - 1) ? " %s,\n" : " %s\n",
                countnames[i]);
        }

        fprintf(cfp, "};\n\n");
        close(cfp);
    }

    if(tfp)
    {
        fclose(tfp);
    }

    i=by*32;
    if(bx!=0)i+=32;
    sprintf(fn,"%s%s.bmp",cpath,outname);
    bmwrite(fn,xx0*32,i,obuf);
    return 0;
}
