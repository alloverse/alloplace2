#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <allonet/server.h>

double get_ts_monod(void);

static int marketplace_pid = -1;
static int serve_pid = -1;
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
        char reason[255] = "(unknown exit reason)";
        if(WIFEXITED(status)) snprintf(reason, 255, "(exited normally, code %d)", WEXITSTATUS(status));
        if(WIFSIGNALED(status)) snprintf(reason, 255, "(terminated by signal %d)", WTERMSIG(status));
        if(WIFCONTINUED(status)) snprintf(reason, 255, "(continued)");
        
        if(pid == marketplace_pid)
        {
            fprintf(stderr, "Reaping marketplace at %d. %s\n", pid, reason);
            marketplace_pid = -1; // trigger relaunch
        }
        else if(pid == settings_pid)
        {
            fprintf(stderr, "Reaping settings at %d. %s\n", pid, reason);
            settings_pid = -1; // trigger relaunch
        }
        else if(pid == serve_pid)
        {
            fprintf(stderr, "Reaping serve at %d. %s\n", pid, reason);
            serve_pid = -1; // trigger relaunch
        }
    }
    if(pid < 0)
    {
        perror("child_handler");
    }
}

// otherwise the apps will receive udp traffic that is sent to the server (!!)
void make_forked_env_safe(void)
{
    for(int i = 3; i < 1023; i++) {
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

static void launch(char *cmd)
{
    int child_status = system(cmd);
    int exit_code = WEXITSTATUS(child_status);
    if (child_status == -1 || exit_code == 127) {
        printf("Failed to launch child process '%s' (status code: %d, exit code: %d)\n", cmd, child_status, exit_code);
    }
    if (exit_code != 0) {
        printf("Child '%s' exited with error, code %d\n", cmd, exit_code);
    }
    exit(exit_code);
}

static double marketplace_last_launched_at;

static void ensure_marketplace_running(void)
{
    if(marketplace_pid == -2) return;

    double now = get_ts_monod();

    if(marketplace_pid == -1 && now - marketplace_last_launched_at > 2.0)
    {
        marketplace_last_launched_at = now;
        marketplace_pid = fork();
        if(marketplace_pid == 0)
        {
            fprintf(stderr, "Launching marketplace as pid %d...\n", getpid());
            make_forked_env_safe();
            char marketcmd[1024];
            sprintf(marketcmd, "cd marketplace; ./allo/assist run alloplace://localhost:%d", port);
            launch(marketcmd);
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
            launch(cmd);
        }
    }
}

static void ensure_serve_running(void)
{
    if(serve_pid == -2) return;

    if(serve_pid == -1)
    {
        serve_pid = fork();
        if(serve_pid == 0)
        {
            fprintf(stderr, "Launching serve...\n");
            make_forked_env_safe();
            char cmd[1024];
            sprintf(cmd, "cd marketplace; env APPS_ROOT=./apps/ python3 allo/serve.py");
            launch(cmd);
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
    const char *hostname = getenv("ALLOPLACE_HOST");
    if(!hostname)
    {
        fprintf(stderr, "!! WARNING: Please provide an internet-public hostname or IP in the environment variable ALLOPLACE_HOST. "
                        "Otherwise, AlloApps on the internet will not be able to connect to this Place.\n");
        hostname = "localhost";
    }
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
        serve_pid = -2;
    }

    printf("======\n\nServing the Alloverse Place \"%s\" on %s:%d\n\n======\n", placename, hostname, port);
    serv = alloserv_start_standalone(hostname, 0, port, placename);
  
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
        ensure_serve_running();
        maybe_print_stats();
    }

    alloserv_stop_standalone();
    return 0;
}
