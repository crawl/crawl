#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define XP_APT 13

int exp_needed(int lev)
{
    lev--;

    unsigned int level = 0;

    // Basic plan:
    // Section 1: levels  1- 5, second derivative goes 10-10-20-30.
    // Section 2: levels  6-13, second derivative is exponential/doubling.
    // Section 3: levels 14-27, second derivative is constant at 6000.
    //
    // Section three is constant so we end up with high levels at about
    // their old values (level 27 at 850k), without delta2 ever decreasing.
    // The values that are considerably different (ie level 13 is now 29000,
    // down from 41040 are because the second derivative goes from 9040 to
    // 1430 at that point in the original, and then slowly builds back
    // up again).  This function smoothes out the old level 10-15 area
    // considerably.

    // Here's a table:
    //
    // level      xp      delta   delta2
    // =====   =======    =====   ======
    //   1           0        0       0
    //   2          10       10      10
    //   3          30       20      10
    //   4          70       40      20
    //   5         140       70      30
    //   6         270      130      60
    //   7         520      250     120
    //   8        1010      490     240
    //   9        1980      970     480
    //  10        3910     1930     960
    //  11        7760     3850    1920
    //  12       15450     7690    3840
    //  13       29000    13550    5860
    //  14       48500    19500    5950
    //  15       74000    25500    6000
    //  16      105500    31500    6000
    //  17      143000    37500    6000
    //  18      186500    43500    6000
    //  19      236000    49500    6000
    //  20      291500    55500    6000
    //  21      353000    61500    6000
    //  22      420500    67500    6000
    //  23      494000    73500    6000
    //  24      573500    79500    6000
    //  25      659000    85500    6000
    //  26      750500    91500    6000
    //  27      848000    97500    6000


    switch (lev)
    {
    case 1:
        level = 1;
        break;
    case 2:
        level = 10;
        break;
    case 3:
        level = 30;
        break;
    case 4:
        level = 70;
        break;

    default:
        if (lev < 13)
        {
            lev -= 4;
            level = 10 + 10 * lev + (60 << lev);
        }
        else
        {
            lev -= 12;
            level = 15500 + 10500 * lev + 3000 * lev * lev;
        }
        break;
    }

    return ((level - 1) * XP_APT / 10);
}

char* colors[4]={"\e[0m", "\e[33m", "\e[31m", "\e[1;31m",};

void ex(const char *name, int xp)
{
    int lev;
    
    printf("%-15s ", name);
    for (lev = 1; lev <= 27; lev++)
    {
        int tier;

        int factor = ceil(sqrt(exp_needed(lev+1) / 30.0));
        int tension = xp/(1+factor);
        
        if (tension <= 0)
            tier = 0;
        else if (tension <= 5)
            tier = 1;
        else if (tension <= 32)
            tier = 2;
        else
            tier = 3;

        if (tension > 999)
            printf(" %s***\e[0m", colors[tier]);
        else
            printf(" %s%3d\e[0m", colors[tier], tension);
    }
    printf("\n");
}

int main()
{
    int lev;
    printf("%-15s ", "");
    for (lev = 1; lev <= 27; lev++)
        printf(" \e[30;1m%3d\e[0m", lev);
    printf("\n");
    ex("newt", 1);
    ex("orc", 2);
    ex("snake", 13);
    ex("iguana", 36);
    ex("Sigmund", 104);
    ex("orc warrior", 132);
    ex("yak", 204);
    ex("yaktaur", 397);
    ex("spiny frog", 408);
    ex("deep elf knight", 871);
    ex("yaktaur captain", 1399);
    ex("stone giant", 2026);
    ex("bone dragon", 2352);
    ex("storm dragon", 2991);
    ex("lich", 3776);
    ex("Xtahua", 5449);
    ex("greater mummy", 7515);
    ex("orb of fire", 11437);
    ex("Cerebov", 15000);
    return 0;
}
