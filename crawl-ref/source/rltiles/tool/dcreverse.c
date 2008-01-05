#include "bm.h"

#define TILEX (32)
#define TILEY (32)

unsigned char *tbuf[3];
unsigned char *dbuf[3];

/*** BUFFER MEMORY ***/
#define XX 30
#define YY 90
#define LX (XX)

#define BIGADR(x,y) ((x)+(y)*LX*32)
#define ADR(x,y) ((x)+(y)*32)

const int read_size = 2048;
int rim = 0;
int tile = 0;
int sx = 0;
int sy = 0;
int ex = 0;
int ey = 0;
char tilename[2048];
char compositename[2048];
unsigned char bkg[3] =
{
    0x47,
    0x6c,
    0x6c
};

int is_background(unsigned char d[3])
{
    if (bkg[0]==d[0] && bkg[1]==d[1] && bkg[2]==d[2])
        return 1;
    else
        return 0;
}

int is_rim(unsigned char d[3])
{
    if (d[0]==1 && d[1]==1 && d[2]==1)
        return 1;
    else
        return 0;
}

int is_black(unsigned char d[3])
{
    if (d[0]==0 && d[1]==0 && d[2]==0)
        return 1;
    else
        return 0;
}

void remove_rim()
{
    int dflag[32][32];
    unsigned char dd[3];
    int x,y,c;
    int ad;
    int n0, n1, n2;

    // 0 - background
    // 1 - tile
    // 2 - black
    // 3 - rim

    for (x = 0; x < 32; x++)
        for (y = 0; y < 32; y++)
        {
            ad = ADR(x,y);
            dd[0]=dbuf[0][ad];
            dd[1]=dbuf[1][ad];
            dd[2]=dbuf[2][ad];
            if (is_background(dd))
                dflag[x][y] = 0;
            else if (is_black(dd))
                dflag[x][y] = 2;
            else if (is_rim(dd))
                dflag[x][y] = 3;
            else
                dflag[x][y] = 1;
        }

    for(x=0;x<TILEX;x++){
        for(y=0;y<TILEY;y++){
            ad=ADR(x,y);
            if(dflag[x][y]==3) {
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

                if (n1 != 0)
                {
                    dbuf[0][ad]=bkg[0];
                    dbuf[1][ad]=bkg[1];
                    dbuf[2][ad]=bkg[2];
                }
            }
        }
    }
}

void copy_tile()
{
    // copy relevant part of tbuf into dbuf, removing the rim if necessary

    int xx,yy,c;
    for (xx = 0; xx < 32; xx++)
        for (yy = 0; yy < 32; yy++)
            for (c = 0; c < 3; c++)
                dbuf[c][ADR(xx,yy)] = tbuf[c][BIGADR(sx+xx,sy+yy)];
    if (rim)
        remove_rim();
}

void write_file()
{
    // write dbuf to tilenam
    bmwrite(tilename,32,32,dbuf);
}

void process_list(char *fname)
{
    int i;
    int x,y;
    char tmp[read_size];

    FILE *fp=fopen(fname,"r");
    if (fp==NULL){
        printf("Error: couldn't open %s\n", fname);
        getchar();
        exit(1);
    }

    while(1){
        fgets(tmp,read_size,fp);
        if (feof(fp))
            break;
        i=0;
        while (i < read_size && tmp[i] >= 32)
            i++;
        tmp[i] = 0;

        if (getname(tmp,"tilefile",compositename))
        {
            if (bmread(compositename,&x,&y,tbuf) != 0)
            {
                break;
            }
        }
        if (getname(tmp,"skip",tilename))
            continue;
        if (getval(tmp,"rim",&rim))
            continue;
        if (getval(tmp,"sx",&sx))
            continue;
        if (getval(tmp,"sy",&sy))
            continue;
        if (getval(tmp,"ex",&ex))
            continue;
        if (getval(tmp,"ey",&ey))
            continue;

        if (getname(tmp,"file",tilename))
        {
            printf("Reading tile %s (%d,%d,%d,%d) rim(%d)\n",
                compositename, sx, sy, ex, ey, rim);
            copy_tile();
            printf("Writing tile %s.\n", tilename);
            write_file();
        }
    }

    fclose(fp);
}

int main(argc, argv)
int argc;
char *argv[];
{
    if (argc <= 1) return;

    process_cpath(argv[0]);
    stdpal();

    fixalloc(tbuf, LX*64*(YY)*64);
    fixalloc(dbuf, 32*32);

    printf("%s\ncpath=%s\n",argv[0],cpath);

    process_list(argv[1]);
}
