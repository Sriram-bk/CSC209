#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    // Your implementation here.
    const char a = '/';
    char *filename;
    filename = strrchr(argv[1], a);
    filename += sizeof(char);

    char *fname;
    fname = malloc(sizeof(char) * (strlen(filename) + 1));
    strncpy(fname, filename, strlen(filename));
    fname[strlen(filename)]='\0';

    printf("%s\n", filename);
    printf("%lu\n", strlen(filename));
    printf("%s\n", fname);
    printf("%lu\n", strlen(fname));

    free(fname);
    return 0;
}
