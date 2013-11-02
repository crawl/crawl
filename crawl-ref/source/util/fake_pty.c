#include <pty.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

static pid_t crawl;
static int tty;

static void sigalrm(int signum)
{
    kill(crawl, SIGTERM);
}

static void slurp_output()
{
    struct pollfd pfd;
    char buf[1024];

    signal(SIGALRM, sigalrm);
    alarm(3600); // a hard limit of an hour (big tests on a Raspberry...)

    pfd.fd     = crawl;
    pfd.events = POLLIN;

    while (poll(&pfd, 1, 60000) > 0) // 60 seconds with no output -> die die die!
    {
        if (read(tty, buf, sizeof(buf)) <= 0)
            break;
    }

    kill(crawl, SIGTERM); // shooting a zombie is ok, let's make sure it's dead
}

int main(int argc, char * const *argv)
{
    struct winsize ws;
    int slave;
    int ret;

    if (argc <= 1)
    {
        fprintf(stderr, "Usage: fake_pty program [args]\n");
        return 1;
    }

    ws.ws_row = 24;
    ws.ws_col = 80;
    ws.ws_xpixel = ws.ws_ypixel = 0;

    // We want to let stderr through, thus can't use forkpty().
    if (openpty(&tty, &slave, 0, 0, &ws))
    {
        fprintf(stderr, "Can't create a pty: %s\n", strerror(errno));
        return 1;
    }

    switch (crawl = fork())
    {
    case -1:
        fprintf(stderr, "Can't fork: %s\n", strerror(errno));
        return 1;

    case 0:
        close(tty);
        dup2(slave, 0);
        dup2(slave, 1); // but _not_ stderr!
        close(slave);
        execvp(argv[1], argv + 1);
        fprintf(stderr, "Can't run '%s': %s\n", argv[1], strerror(errno));
        return 1;

    default:
        close(slave);
        slurp_output();
        if (waitpid(crawl, &ret, 0) != crawl)
            return 1; // can't happen
        if (WIFEXITED(ret))
            return WEXITSTATUS(ret);
        if (WIFSIGNALED(ret))
            return 128 + WTERMSIG(ret);
        // Neither exited nor signaled?  Did the process eat mushrooms meant
        // fo the mother-in-law or what?
        return 1;
    }
}
