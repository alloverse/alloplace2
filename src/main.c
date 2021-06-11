#include <stdio.h>
#include <allonet/server.h>

int main(int argc, const char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr, "Usage: alloplace2 [place name]\n");
        return 1;
    }
    const char *placename = argv[1];
    return alloserv_run_standalone(0, 21337, placename);
}
