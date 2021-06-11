#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <allonet/server.h>

static int marketplace_pid = -1;
static alloserver *serv;

static void
child_handler(int sig)
{
    pid_t pid;
    int status;

    while((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if(pid == marketplace_pid)
        {
            fprintf(stderr, "Reaping marketplace at %d\n", pid);
            marketplace_pid = -1; // trigger relaunch
        }
    }
}

static void ensure_marketplace_running(void)
{
    if(marketplace_pid == -1)
    {
        marketplace_pid = fork();
        if(marketplace_pid == 0)
        {
            fprintf(stderr, "Launching marketplace...\n");
            exit(system("cd marketplace; ./allo/assist run alloplace://localhost"));
        }
    }
}

double get_ts_monod(void);
double last_stats_print = 0;
static void maybe_print_stats(void)
{
    double now = get_ts_monod();
    if(now > last_stats_print + 5.0)
    {
        last_stats_print = now;
        char stats[1024];
        alloserv_get_stats(serv, stats, 1024);
        fprintf(stderr, "\nStats at %.0f:\n%s\n", now, stats);
    }
}

int main(int argc, const char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr, "Usage: alloplace2 [place name]\n");
        return 1;
    }
    const char *placename = argv[1];

    printf("Launching alloplace2 as '%s'\n", placename);
    serv = alloserv_start_standalone(0, 21337, placename);
  
    if (serv == NULL)
    {
        return false;
    }

    int allosocket = allo_socket_for_select(serv);

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = child_handler;
    sigaction(SIGCHLD, &sa, NULL);

    int house_ok = system("cd marketplace/apps/allo-house; ./allo/assist run alloplace://localhost &");
    
    printf("alloplace2 is now entering runloop.\n");
    while (1) {
        if (alloserv_poll_standalone(allosocket) == false)
        {
            alloserv_stop_standalone();
            return false;
        }
        ensure_marketplace_running();
        maybe_print_stats();
    }

    alloserv_stop_standalone();
    return 0;
}
