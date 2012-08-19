#include <termios.h>
#include <sys/ioctl.h>

int term_width()
{
    struct winsize ts;

    if (ioctl(0, TIOCGWINSZ, &ts) || ts.ws_col <= 0)
        return 80; // unknown

    return ts.ws_col;
}
