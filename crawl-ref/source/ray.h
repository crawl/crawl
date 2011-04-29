/*
 *  File:       ray.h
 *  Summary:    Ray definition
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author: zelgadis $ on $Date: 2007-09-16 04:49:23 +0100 (Sun, 16 Sep 2007) $
 *
 *  Modified for Hexcrawl by Martin Bays, 2007
 *
 *  Change History (most recent first):
 *
 * <1>     7/11/07    MPC   Split off from externs.h
 */

#ifndef RAY_H
#define RAY_H

struct ray_def
{
public:
    int n;                    // accpos and dir are n-division points
    hexcoord cell;            // nearest position in lattice L
    hexdir disp;              // displacement from cell in (1/n)L
    hexdir dir;               // dir of movement, element of (1/n)L

    int fullray_idx;          // for cycling: where did we come from?

public:
    hexcoord pos() const { return cell; }
    int x() const { return cell.x; }
    int y() const { return cell.y; }
    
    void advance();
    void advance_and_bounce();
    void regress();
    void deflect(int deflect_amount);

private:
    bool recentre();
    void rescale(int m);
};

#endif
