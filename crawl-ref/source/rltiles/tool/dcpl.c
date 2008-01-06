#include "bm.h"

/** Some global **/
//Flags 
int corpse, mesh, slant,rim;
// Files
FILE *mfp,*sfp;
//Strings
char outname[1024], subsname[200], sdir[200];
char realname[1024];

/** Parts related **/
int parts_n;
#define MAXPARTS 20
int parts_nx[MAXPARTS], parts_ny[MAXPARTS];
int parts_ox[MAXPARTS], parts_oy[MAXPARTS];
int parts_start[MAXPARTS], parts_number[MAXPARTS];

char parts_names[MAXPARTS][64];

int parts_comment_ofs[MAXPARTS];
int n_comments, pos_comment;
#define MAXTOTAL 1000
int part_comment_ofs[MAXTOTAL];
char comment[MAXTOTAL*60];

int part_x,part_y;
int part_n;
int part_nx,part_ny;
char part_name[32];
int part_wx, part_wy, part_ox, part_oy;

/*** BUFFER MEMORY ***/
#define XX 30
int xx0;
#define LX (XX)


/*** tmp buffer, floor , final output, final queue ***/
unsigned char *tbuf[3],*fbuf[3],*dbuf[3], *obuf[3];

/*** normal floor*/
#define WOADR(x,y,xx,yy) \
((x)*32+xx+  xx0*32*((y)*32+yy))

#define ADR(x,y) ((x)+(y)*32)

/*** output width/height in block ***/
int bx,by;

/****************************/
/* Wrapper routines ********/
/**************************/
int load_pxxx(fnam)
    char *fnam;
{
    int x,y;

    sprintf(realname,"%s%s%c%s.bmp",cpath,sdir,PATHSEP,fnam);
    if(bmread(realname,&x,&y,tbuf)==0) return 0;

    sprintf(realname,"%s%s.bmp",cpath,fnam);
    if(bmread(realname,&x,&y,tbuf)==0) return 0;

    if(subsname[0])
    {
        sprintf(realname,"%s%s%c%s.bmp",cpath,sdir,PATHSEP,subsname);
        if(bmread(realname,&x,&y,tbuf)==0) return 0;

        sprintf(realname,"%s%s.bmp",cpath,subsname);
        if(bmread(realname,&x,&y,tbuf)==0) return 0;
    }

    return 1;
}


void clr_buf()
{
int xx,yy;

    for(xx=0;xx<32;xx++)
    {
        for(yy=0;yy<32;yy++)
        {
            dbuf[0][ ADR(xx,yy) ]=0x47;
            dbuf[1][ ADR(xx,yy) ]=0x6c;
            dbuf[2][ ADR(xx,yy) ]=0x6c;
        }
    }
}

void cp_floor()
{
    int xx,yy,c;
    for(xx=0;xx<32;xx++)
        for(yy=0;yy<32;yy++)
            for(c=0;c<3;c++)
                dbuf[c][ ADR(xx,yy) ]=fbuf[c][ ADR(xx,yy)];
}

void cp_monst_32()
{
    int xx,yy,c,dd[3],ad;
    char dflag[33][32];
    int xmin,xmax,ymin,ymax,ox,oy;

    if(corpse==1 )
    {
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
            }
        }
        ox=(xmax+xmin)/2-16;
        oy=(ymax+ymin)/2-16;
    }

    /** copy loop **/
    for(xx=0;xx<32;xx++){
        for(yy=0;yy<32;yy++){
            dflag[xx][yy]=0;
            ad=ADR(xx,yy);

            if(corpse==1)
            {
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

            if(mesh==2)
            {
                if( (dd[0]!=0x47)||(dd[1]!=0x6c)|| (dd[2]!=0x6c))
                {
                    if( ((xx+yy)&1) ==0)dd[0]=dd[1]=dd[2]=0;
                }
            }
            if(mesh==1)
            {
                 if((((xx/2)+(yy/2))&1) ==1)dd[0]=dd[1]=dd[2]=0;
            }

            if( (dd[0]==0x47)&&(dd[1]==0x6c)&& (dd[2]==0x6c))continue;
            if( (corpse==1) &&(dd[0]==0)&&(dd[1]==0)&& (dd[2]==0))continue;

            for(c=0;c<3;c++) {dbuf[c][ADR(xx,yy)]=dd[c];}
            dflag[xx][yy]=1;
        }
    }/*XY*/

#if 1
    if(corpse==1)
    {
        for(xx=0;xx<32;xx++)
        {
            int cy=18;
            if(xx<4 || xx>=28)cy+=2;else
            if(xx<12 || xx>=20) cy+=1;
            if(dflag[xx][cy-2]==1 && dflag[xx][cy+1]==1 )
            {
                for(yy=cy-1;yy<=cy-0;yy++)
                { 
                    dbuf[0][ADR(xx,yy)]=32;
                    dbuf[1][ADR(xx,yy)]=0;
                    dbuf[2][ADR(xx,yy)]=0;
                    dflag[xx][yy]=1;
                 }
             }
        }

        /** shade**/
        for(xx=1;xx<32;xx++){
            for(yy=1;yy<32;yy++){
                if(dflag[xx][yy]==0 && dflag[xx-1][yy-1]==1)
                {
                    dbuf[0][ADR(xx,yy)]=0;
                    dbuf[1][ADR(xx,yy)]=0;
                    dbuf[2][ADR(xx,yy)]=0;
                }
            }
         }

        for(xx=3;xx<32;xx++){
            for(yy=3;yy<32;yy++){
                if(dflag[xx][yy]==0 && dflag[xx-1][yy-1]==0
                    && dflag[xx-2][yy-2]==1  && dflag[xx-3][yy-3]==1)
                {
                    dbuf[0][ADR(xx,yy)]=0;
                    dbuf[1][ADR(xx,yy)]=0;
                    dbuf[2][ADR(xx,yy)]=0;
                }
            }
        }

    }
#endif
}


void bflush()
{
    int xx,yy,c;
    for(xx=part_ox;xx<part_ox+part_wx;xx++){
        for(yy=part_oy;yy<part_oy+part_wy;yy++){
            for(c=0;c<3;c++){
                obuf[c][WOADR(bx,by,part_x*part_wx+xx-part_ox,part_y*part_wy+yy-part_oy)]
                  = dbuf[c][ADR(xx,yy)];
            }
        }
    }
}


void  load_monst(fnam) char *fnam;{
	if( load_pxxx(fnam)){
		 printf("no file %s.bmp\n",fnam);
                getchar();
		exit(1);
	}
	cp_monst_32();
	bflush();
}

void flush_part()
{

    if(part_x!=0 || part_y!=0)
    {
        part_x=part_y=0;
        bx++;if(bx==xx0){bx=0;by++;;}
    }
    parts_number[parts_n]=part_n;
    parts_n++;

}

void process_config(char *fname)
{
    int i,j;
    char tmp[100],st[1024];
    char *nuke;
    FILE *fp=fopen(fname,"r");
    if(fp==NULL)
    {
        printf("Error no config file %s\nHit return",fname);
        getchar();
        exit(1);
    }


    while(1){
        fgets(tmp,99,fp);
        if(feof(fp))break;
        i=0;
        while(i<99 && tmp[i]>=32) i++;
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
        if (getname(tmp,"subst",subsname)) continue;
        if (getname(tmp,"sdir",sdir)) continue;
        if (getname(tmp,"name", outname)) continue;
        if (getval(tmp,"width",&xx0)) continue;
        /****/
        if (getname(tmp,"parts_ctg",part_name))
        {
            if(part_n!=0)
                flush_part();
            part_n=0;
            strcpy(parts_names[parts_n],part_name); 
            parts_comment_ofs[parts_n] = n_comments;
            parts_start[parts_n]=bx+by*xx0;
            fprintf(sfp,"#define TILEP_PART_%s %d\n",part_name, parts_n);
            fprintf(sfp,"enum %s {\n",part_name);
            fprintf(sfp," TILEP_%s_000,\n",part_name);

            continue;
        }

        if (getval(tmp,"parts_wx",&part_wx))
        {
            parts_nx[parts_n]=part_nx=32/part_wx;
            continue;
        }

        if (getval(tmp,"parts_wy",&part_wy))
        {
            parts_ny[parts_n]=part_ny=32/part_wy;
            continue;
        }

        if (getval(tmp,"parts_ox", &part_ox))
        {
            parts_ox[parts_n]=part_ox;
            continue;
        }

        if (getval(tmp,"parts_oy", &part_oy))
        {
            parts_oy[parts_n]=part_oy;
            continue;
        }

        /****/
        if (tmp[0]=='#' || tmp[0]<32){
            if(tmp[0]=='#') fprintf(sfp,"//%s\n",tmp);
            continue;
        }

        if (strcmp(tmp, "%end") == 0) 
        {
            fprintf(sfp," N_PART_%s};\n\n",part_name);
            continue;
        }
        /*** normal bitmap ***/
#define WID 32
        clr_buf();
        cp_floor();

        i=0;
        while(i<99 && tmp[i]>32)i++;
        tmp[i]=0; strcpy(st, &tmp[i+1]);
        load_monst(tmp);

        fprintf(mfp,
            "<area shape=\"rect\" coords=\"%d,%d,%d,%d\" alt=\"%s\" href=%s>\n",
            bx*WID + part_x*part_wx,
            by*WID + part_y*part_wy,
            bx*WID + part_x*part_wx + part_wx-1,
            by*WID + part_y*part_wy + part_wy-1,
            st, realname);

        if(!strstr(st,"IGNORE_COMMENT")){
            nuke=strstr(st,"/*");if(nuke)*nuke=0;
            if (st && strcmp(st,"") != 0 && strcmp(st, "\n") != 0)
            {
                fprintf(sfp," TILEP_%s_%s,\n", part_name, st);
                i=strlen(st);
                strncpy(&comment[pos_comment],st,i);
            }
            else
            {
                fprintf(sfp," FILLER_%s_%d,\n", part_name, part_n);
                parts_names[i][0] = 0;
                i = 1;
                strncpy(&comment[pos_comment],"\0",i);
            }

            part_comment_ofs[n_comments]=pos_comment;
            pos_comment += i;
            n_comments++;

            //  n_comments = pos_comment=0;
            //int parts_comment_ofs[];
            //int part_comment_ofs[MAXTOTAL];
            //char comment[MAXTOTAL*60];
        }
        else
        {
            i=0;
            part_comment_ofs[n_comments]=pos_comment;
            pos_comment += i;
            n_comments++;
        }

        part_n++;
        part_x++;
        if(part_x==part_nx)
        {
            part_x=0;
            part_y++;
            if(part_y==part_ny)
            {
                part_y=0;
                bx++;
                if(bx==xx0)
                {
                    bx=0;
                    by++;
                }
            }
        }

        /* normal */

    }/* while */
    fclose(fp);
}

/********************************************/

int main(int argc, char **argv)
{

    int i,j,k,l,m,n,fl;
    char fn[100],st2[100];

    slant=corpse=mesh=rim=0;

    bx=by=0;

    /* parts related */
    parts_n=0;

    part_x=part_y=0;
    part_n=0;
    part_wx=part_wy=32;
    part_ox=part_oy=0;

    /* comments */
    n_comments = pos_comment=0;
    //int parts_comment_ofs[];
    //int part_comment_ofs[MAXTOTAL];
    //char comment[MAXTOTAL*60];


    process_cpath(argv[0]);

    xx0=XX;
    subsname[0]=0;
    sdir[0]=0;
    realname[0]=0;

    stdpal();
    fixalloc(tbuf,64*64);
    fixalloc(dbuf,64*64);
    fixalloc(fbuf,64*64);
    fixalloc(obuf, 32*64*(64)*64);


    strcpy(outname,"tile");

    sprintf(fn,"%smap.htm",cpath);
    mfp=fopen(fn,"w");
    if(mfp==NULL){
        printf("Error could not open %s\nHit return",fn);
        getchar();
        exit(1);
    }

    sprintf(fn,"%stiledef-p.h",cpath);
    sfp=fopen(fn,"w");
    if(sfp==NULL){
        printf("Error could not open %s\nHit return",fn);
        getchar();
        exit(1);
    }
    fprintf(sfp,"/* Automatically generated by tile generator. */\n");

    fprintf(mfp,"<HTML>\n");
    fprintf(mfp,"<MAP NAME=\"nhmap\">\n");

    printf("%s\ncpath=%s\n",argv[0],cpath);
    if(argc==1)
        sprintf(fn,"%sdc-pl.txt",cpath);
    else
        strcpy(fn,argv[1]);
    process_config(fn);

    if(part_n!=0)flush_part();

    fprintf(sfp,"\n#define TILEP_TOTAL %d\n",bx+by*xx0);
    fprintf(sfp,"#define TILEP_PER_ROW %d\n\n",xx0);

    fprintf(sfp,"#define TILEP_PARTS_TOTAL %d\n\n",parts_n);

    fprintf(sfp,"const int tilep_parts_start[TILEP_PARTS_TOTAL]=\n    {");
    for(i=0;i<parts_n-1;i++)fprintf(sfp," %d,",parts_start[i]);
    fprintf(sfp," %d};\n",parts_start[parts_n-1]);

    fprintf(sfp,"const int tilep_parts_total[TILEP_PARTS_TOTAL]=\n    {");
    for(i=0;i<parts_n-1;i++)fprintf(sfp," %d,",parts_number[i]);
    fprintf(sfp," %d};\n",parts_number[parts_n-1]);

    fprintf(sfp,"const int tilep_parts_ox[TILEP_PARTS_TOTAL]=\n    {");
    for(i=0;i<parts_n-1;i++)fprintf(sfp," %d,",parts_ox[i]);
    fprintf(sfp," %d};\n",parts_ox[parts_n-1]);

    fprintf(sfp,"const int tilep_parts_oy[TILEP_PARTS_TOTAL]=\n    {");
    for(i=0;i<parts_n-1;i++)fprintf(sfp," %d,",parts_oy[i]);
    fprintf(sfp," %d};\n",parts_oy[parts_n-1]);

    fprintf(sfp,"const int tilep_parts_nx[TILEP_PARTS_TOTAL]=\n    {");
    for(i=0;i<parts_n-1;i++)fprintf(sfp," %d,",parts_nx[i]);
    fprintf(sfp," %d};\n",parts_nx[parts_n-1]);

    fprintf(sfp,"const int tilep_parts_ny[TILEP_PARTS_TOTAL]=\n    {");
    for(i=0;i<parts_n-1;i++)fprintf(sfp," %d,",parts_ny[i]);
    fprintf(sfp," %d};\n",parts_ny[parts_n-1]);

    fclose(sfp);

    sprintf(fn,"%stilep-cmt.h",cpath);
    sfp=fopen(fn,"w");
    if(sfp==NULL){
        printf("Error could not open %s\nHit return",fn);
        getchar();
        exit(1);
    }
    fprintf(sfp,"/* Automatically generated by tile generator. */\n");
    fprintf(sfp,"#include \"tiledef-p.h\"\n");

    fprintf(sfp,"static const char *tilep_parts_name[%d]={\n",parts_n);
    for(i=0;i<parts_n-1;i++)
    {
        fprintf(sfp," \"%s\",\n",parts_names[i]);
    }
    i=parts_n-1;
    fprintf(sfp," \"%s\"\n};\n",parts_names[i]);


    fprintf(sfp,"const int tilep_comment_ofs[TILEP_PARTS_TOTAL]= {\n");
    for(i=0;i<parts_n-1;i++)fprintf(sfp," %d,",parts_comment_ofs[i]);
    fprintf(sfp," %d};\n",parts_comment_ofs[parts_n-1]);

    fprintf(sfp,"static const char *tilep_comment[%d]={\n",n_comments);
    for(i=0;i<n_comments-1;i++)
    {
        int len=part_comment_ofs[i+1]-part_comment_ofs[i];
        strncpy(st2, &comment[part_comment_ofs[i]],len);
        st2[len]=0;
        fprintf(sfp," \"%s\",\n",st2);
    }
    i=pos_comment-part_comment_ofs[n_comments-1];
    strncpy(st2, &comment[part_comment_ofs[n_comments-1]],i);
    st2[i]=0;
    fprintf(sfp," \"%s\" };\n",st2);
    fclose(sfp);

    fprintf(mfp,"<IMG SRC=%s.bmp USEMAP=\"#nhmap\" >\n", outname);


    fclose(mfp);
    i=by*32;if(bx!=0)i+=32;

    sprintf(fn,"%s%s.bmp",cpath,outname);
    bmwrite(fn,xx0*32,i,obuf);

}
