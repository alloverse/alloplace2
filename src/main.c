#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <allonet/server.h>

double get_ts_monod(void);

struct embedded_app {
    const char *name;
    const char *launchfmt;
    int pid;
    double last_launched_at;
} embedded_apps[] = {
    {
        "marketplace",
        "cd marketplace; ./allo/assist run alloplace://%s:%d", -1
    },
    {
        "settings",
        "cd placesettings; ./allo/assist run alloplace://%s:%d", -1
    },
    {
        "decorator",
        "cd decorator; ./allo/assist run alloplace://%s:%d", -1
    },
    {
        "serve",
        "cd marketplace; env APPS_ROOT=./apps/ python3 allo/serve.py", -1
    }
};
size_t embedded_apps_count = sizeof(embedded_apps)/sizeof(*embedded_apps);

static alloserver *serv;
static int port;
static char *hostname;

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
        
        for(int i = 0; i < embedded_apps_count; i++)
        {
            if(pid == embedded_apps[i].pid)
            {
                fprintf(stderr, "Reaping %s at %d. %s\n", embedded_apps[i].name, pid, reason);
                embedded_apps[i].pid = -1; // trigger relaunch
            }
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

static void ensure_running(struct embedded_app *app)
{
    if(app->pid == -2) return;

    double now = get_ts_monod();

    if(app->pid == -1 && now - app->last_launched_at > 2.0)
    {
        app->last_launched_at = now;
        app->pid = fork();
        if(app->pid == 0)
        {
            fprintf(stderr, "Launching %s as pid %d...\n", app->name, getpid());
            make_forked_env_safe();
            char appcmd[1024];
            sprintf(appcmd, app->launchfmt, hostname, port);
            launch(appcmd);
        }
    }
}

double last_stats_print = 0;
bool do_print_stats = false;
static void maybe_print_stats(void)
{
    double now = get_ts_monod();
    if(now > last_stats_print + 5.0 && do_print_stats)
    {
        last_stats_print = now;
        char stats[1024];
        alloserv_get_stats(serv, stats, 1024);
        fprintf(stderr, "\nStats at %.0f:\n%s\n", now, stats);
    }
}

int main(int argc, const char **argv)
{
    const char *ehostname = getenv("ALLOPLACE_HOST");
    if(ehostname)
    {
        hostname = strdup(ehostname);
    }
    else
    {
        hostname = strdup("localhost");
        fprintf(stderr, "\n\n!! WARNING:\nPlease provide an internet-public hostname or IP in the environment variable ALLOPLACE_HOST. "
                        "Otherwise, AlloApps on the internet will not be able to connect to this Place.\n\n\n");
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
        for(int i = 0; i < embedded_apps_count; i++)
            embedded_apps[i].pid = -2;
    }
    if(getenv("ALLOPLACE_PRINT_STATS"))
    {
        do_print_stats = true;
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
    
    printf("alloplace2 is now entering runloop.\n");
    while (1) {
        if (alloserv_poll_standalone(allosocket) == false)
        {
            alloserv_stop_standalone();
            return false;
        }
        for(int i = 0; i < embedded_apps_count; i++)
            ensure_running(&embedded_apps[i]);
        maybe_print_stats();
    }

    alloserv_stop_standalone();
    return 0;
}
