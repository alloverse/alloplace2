#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <allonet/server.h>

static int marketplace_pid = -1;
static int settings_pid = -1;
static alloserver *serv;
static int port;

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
        else if(pid == settings_pid)
        {
            fprintf(stderr, "Reaping settings at %d\n", pid);
            settings_pid = -1; // trigger relaunch
        }
    }
}

// otherwise the apps will receive udp traffic that is sent to the server (!!)
void make_forked_env_safe(void)
{
    for(int i = 2; i < 1023; i++) {
        close(i);
    }
}

void system2(const char *cmd)
{
    pid_t bg = fork();
    if(bg == 0)
    {
        make_forked_env_safe();
        exit(system(cmd));
    }
}

static void ensure_marketplace_running(void)
{
    if(marketplace_pid == -2) return;

    if(marketplace_pid == -1)
    {
        marketplace_pid = fork();
        if(marketplace_pid == 0)
        {
            fprintf(stderr, "Launching marketplace...\n");
            make_forked_env_safe();
            char marketcmd[1024];
            sprintf(marketcmd, "cd marketplace; ./allo/assist run alloplace://localhost:%d", port);
            exit(system(marketcmd));
        }
    }
}

static void ensure_settings_running(void)
{
    if(settings_pid == -2) return;

    if(settings_pid == -1)
    {
        settings_pid = fork();
        if(settings_pid == 0)
        {
            fprintf(stderr, "Launching settings app...\n");
            make_forked_env_safe();
            char cmd[1024];
            sprintf(cmd, "cd placesettings; ./allo/assist run alloplace://localhost:%d", port);
            exit(system(cmd));
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
    const char *placename = getenv("ALLOPLACE_NAME");
    if(!placename) placename = "Unnamed place";
    const char *portstr = getenv("ALLOPLACE_PORT");
    if(!portstr || sscanf(portstr, "%d", &port) == 0)
    {
        port = 21337;
    }
    if(getenv("ALLOPLACE_DISABLE_MARKETPLACE"))
    {
        marketplace_pid = -2;
        settings_pid = -2;
    }

    printf("Launching alloplace2 as '%s' on *:%d\n", placename, port);
    serv = alloserv_start_standalone(0, port, placename);
  
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

    char decocmd[1024];
    sprintf(decocmd, "cd marketplace/apps/allo-decorator; ./allo/assist run alloplace://localhost:%d &", port);
    system2(decocmd);
    
    printf("alloplace2 is now entering runloop.\n");
    while (1) {
        if (alloserv_poll_standalone(allosocket) == false)
        {
            alloserv_stop_standalone();
            return false;
        }
        ensure_marketplace_running();
        ensure_settings_running();
        maybe_print_stats();
    }

    alloserv_stop_standalone();
    return 0;
}
